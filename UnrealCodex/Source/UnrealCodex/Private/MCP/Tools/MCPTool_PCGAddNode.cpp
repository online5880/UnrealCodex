// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PCGAddNode.h"

#include "EditorAssetLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
	static FString ResolveAliasString(const TSharedRef<FJsonObject>& Params, const TCHAR* Primary, const TCHAR* Alias)
	{
		FString Value;
		if (Params->TryGetStringField(Primary, Value) && !Value.IsEmpty())
		{
			return Value;
		}
		if (Params->TryGetStringField(Alias, Value) && !Value.IsEmpty())
		{
			return Value;
		}
		return FString();
	}

	static bool TryExtractObjectArray(UObject* Source, const TCHAR* FieldName, TArray<UObject*>& OutObjects)
	{
		if (!Source)
		{
			return false;
		}

		FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Source->GetClass(), FieldName);
		if (!ArrayProp)
		{
			return false;
		}

		FObjectPropertyBase* ObjInner = CastField<FObjectPropertyBase>(ArrayProp->Inner);
		if (!ObjInner)
		{
			return false;
		}

		void* ArrayData = ArrayProp->ContainerPtrToValuePtr<void>(Source);
		if (!ArrayData)
		{
			return false;
		}

		FScriptArrayHelper Helper(ArrayProp, ArrayData);
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			UObject* Item = ObjInner->GetObjectPropertyValue(Helper.GetRawPtr(Index));
			if (Item)
			{
				OutObjects.Add(Item);
			}
		}

		return true;
	}

	static TArray<UObject*> ExtractGraphNodes(UObject* GraphAsset)
	{
		TArray<UObject*> Nodes;
		if (!TryExtractObjectArray(GraphAsset, TEXT("Nodes"), Nodes))
		{
			TryExtractObjectArray(GraphAsset, TEXT("AllNodes"), Nodes);
		}
		return Nodes;
	}

	static UObject* InvokeAddNode(UObject* GraphAsset, UClass* NodeClass)
	{
		if (!GraphAsset || !NodeClass)
		{
			return nullptr;
		}

		const TCHAR* CandidateFunctions[] = {
			TEXT("AddNodeOfType"),
			TEXT("AddNode")
		};

		for (const TCHAR* FunctionName : CandidateFunctions)
		{
			UFunction* Function = GraphAsset->FindFunction(FName(FunctionName));
			if (!Function)
			{
				continue;
			}

			TArray<uint8> ParamsMemory;
			ParamsMemory.SetNumZeroed(Function->ParmsSize);

			bool bBoundInput = false;

			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				FProperty* Prop = *It;
				if (!Prop->HasAnyPropertyFlags(CPF_Parm) || Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				if (FClassProperty* ClassProp = CastField<FClassProperty>(Prop))
				{
					ClassProp->SetPropertyValue_InContainer(ParamsMemory.GetData(), NodeClass);
					bBoundInput = true;
					break;
				}

				if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Prop))
				{
					UObject* SettingsObject = NodeClass->HasAnyClassFlags(CLASS_Abstract)
						? NodeClass->GetDefaultObject()
						: NewObject<UObject>(GetTransientPackage(), NodeClass);
					ObjectProp->SetObjectPropertyValue_InContainer(ParamsMemory.GetData(), SettingsObject);
					bBoundInput = true;
					break;
				}
			}

			if (!bBoundInput)
			{
				continue;
			}

			GraphAsset->ProcessEvent(Function, ParamsMemory.GetData());

			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				FProperty* Prop = *It;
				if (!Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					continue;
				}

				if (FObjectPropertyBase* ReturnObj = CastField<FObjectPropertyBase>(Prop))
				{
					return ReturnObj->GetObjectPropertyValue_InContainer(ParamsMemory.GetData());
				}
			}
		}

		return nullptr;
	}
}

FMCPToolResult FMCPTool_PCGAddNode::Execute(const TSharedRef<FJsonObject>& Params)
{
	const FString GraphPath = ResolveAliasString(Params, TEXT("graph_path"), TEXT("graphPath"));
	if (GraphPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: graph_path (or graphPath)."));
	}

	const FString NodeType = ResolveAliasString(Params, TEXT("node_type"), TEXT("nodeType"));
	if (NodeType.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: node_type (or nodeType)."));
	}

	FString NodeLabel = ResolveAliasString(Params, TEXT("node_label"), TEXT("nodeLabel"));
	if (NodeLabel.IsEmpty())
	{
		NodeLabel = NodeType;
	}

	UObject* GraphAsset = UEditorAssetLibrary::LoadAsset(GraphPath);
	if (!GraphAsset)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("PCGGraph not found: %s"), *GraphPath));
	}

	const FString ClassName = GraphAsset->GetClass()->GetName();
	if (!ClassName.Equals(TEXT("PCGGraph"), ESearchCase::IgnoreCase) && !ClassName.Contains(TEXT("PCGGraph"), ESearchCase::IgnoreCase))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Asset is not a PCGGraph: %s (class=%s)"), *GraphPath, *ClassName));
	}

	UClass* NodeClass = StaticLoadClass(UObject::StaticClass(), nullptr, *NodeType);
	if (!NodeClass)
	{
		NodeClass = FindObject<UClass>(nullptr, *NodeType);
	}
	if (!NodeClass)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("PCG node type not found: %s"), *NodeType));
	}

	const TArray<UObject*> NodesBefore = ExtractGraphNodes(GraphAsset);
	TSet<FString> ExistingNodeNames;
	for (UObject* Node : NodesBefore)
	{
		if (Node)
		{
			ExistingNodeNames.Add(Node->GetName());
		}
	}

	UObject* NewNode = InvokeAddNode(GraphAsset, NodeClass);
	if (!NewNode)
	{
		const TArray<UObject*> NodesAfterFallback = ExtractGraphNodes(GraphAsset);
		for (UObject* Candidate : NodesAfterFallback)
		{
			if (Candidate && !ExistingNodeNames.Contains(Candidate->GetName()))
			{
				NewNode = Candidate;
				break;
			}
		}
	}

	if (!NewNode)
	{
		return FMCPToolResult::Error(
			TEXT("Failed to add node. Could not invoke PCGGraph AddNode/AddNodeOfType via reflection.")
		);
	}

	GraphAsset->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(GraphPath);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graphPath"), GraphPath);
	Data->SetStringField(TEXT("nodeType"), NodeType);
	Data->SetStringField(TEXT("nodeLabel"), NodeLabel);
	Data->SetStringField(TEXT("nodeId"), NewNode->GetName());
	Data->SetBoolField(TEXT("added"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added PCG node '%s' to graph '%s'"), *NewNode->GetName(), *GraphAsset->GetName()),
		Data);
}
