// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PCGConnectNodes.h"

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

	static bool SetPinNameParam(FProperty* Property, void* ParamsMemory, const FString& Value)
	{
		if (FStrProperty* StrProp = CastField<FStrProperty>(Property))
		{
			StrProp->SetPropertyValue_InContainer(ParamsMemory, Value);
			return true;
		}

		if (FNameProperty* NameProp = CastField<FNameProperty>(Property))
		{
			NameProp->SetPropertyValue_InContainer(ParamsMemory, FName(*Value));
			return true;
		}

		if (FTextProperty* TextProp = CastField<FTextProperty>(Property))
		{
			TextProp->SetPropertyValue_InContainer(ParamsMemory, FText::FromString(Value));
			return true;
		}

		return false;
	}

	static bool InvokeConnectNodes(UObject* GraphAsset, UObject* SourceNode, const FString& SourcePin, UObject* TargetNode, const FString& TargetPin, bool& bOutConnected)
	{
		if (!GraphAsset || !SourceNode || !TargetNode)
		{
			return false;
		}

		const TCHAR* CandidateFunctions[] = {
			TEXT("AddEdge")
		};

		for (const TCHAR* FunctionName : CandidateFunctions)
		{
			UFunction* Function = GraphAsset->FindFunction(FName(FunctionName));
			if (!Function)
			{
				continue;
			}

			TArray<FProperty*> InputParams;
			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				FProperty* Prop = *It;
				if (Prop->HasAnyPropertyFlags(CPF_Parm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					InputParams.Add(Prop);
				}
			}

			if (InputParams.Num() < 4)
			{
				continue;
			}

			TArray<uint8> ParamsMemory;
			ParamsMemory.SetNumZeroed(Function->ParmsSize);

			bool bValidBinding = true;

			if (FObjectPropertyBase* SourceNodeProp = CastField<FObjectPropertyBase>(InputParams[0]))
			{
				SourceNodeProp->SetObjectPropertyValue_InContainer(ParamsMemory.GetData(), SourceNode);
			}
			else
			{
				bValidBinding = false;
			}

			if (bValidBinding && !SetPinNameParam(InputParams[1], ParamsMemory.GetData(), SourcePin))
			{
				bValidBinding = false;
			}

			if (bValidBinding)
			{
				if (FObjectPropertyBase* TargetNodeProp = CastField<FObjectPropertyBase>(InputParams[2]))
				{
					TargetNodeProp->SetObjectPropertyValue_InContainer(ParamsMemory.GetData(), TargetNode);
				}
				else
				{
					bValidBinding = false;
				}
			}

			if (bValidBinding && !SetPinNameParam(InputParams[3], ParamsMemory.GetData(), TargetPin))
			{
				bValidBinding = false;
			}

			if (!bValidBinding)
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

				if (FBoolProperty* ReturnBool = CastField<FBoolProperty>(Prop))
				{
					bOutConnected = ReturnBool->GetPropertyValue_InContainer(ParamsMemory.GetData());
					return true;
				}
			}

			// Function existed and executed but has no boolean return; treat as success.
			bOutConnected = true;
			return true;
		}

		return false;
	}
}

FMCPToolResult FMCPTool_PCGConnectNodes::Execute(const TSharedRef<FJsonObject>& Params)
{
	const FString GraphPath = ResolveAliasString(Params, TEXT("graph_path"), TEXT("graphPath"));
	if (GraphPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: graph_path (or graphPath)."));
	}

	const FString SourceNodeName = ResolveAliasString(Params, TEXT("source_node"), TEXT("sourceNode"));
	if (SourceNodeName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: source_node (or sourceNode)."));
	}

	const FString SourcePin = ResolveAliasString(Params, TEXT("source_pin"), TEXT("sourcePin"));
	if (SourcePin.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: source_pin (or sourcePin)."));
	}

	const FString TargetNodeName = ResolveAliasString(Params, TEXT("target_node"), TEXT("targetNode"));
	if (TargetNodeName.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: target_node (or targetNode)."));
	}

	const FString TargetPin = ResolveAliasString(Params, TEXT("target_pin"), TEXT("targetPin"));
	if (TargetPin.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: target_pin (or targetPin)."));
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

	TArray<UObject*> NodeObjects = ExtractGraphNodes(GraphAsset);
	TMap<FString, UObject*> NodesByName;
	for (UObject* Node : NodeObjects)
	{
		if (Node)
		{
			NodesByName.Add(Node->GetName(), Node);
		}
	}

	UObject** SourceNodePtr = NodesByName.Find(SourceNodeName);
	if (!SourceNodePtr)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Source node not found: %s"), *SourceNodeName));
	}

	UObject** TargetNodePtr = NodesByName.Find(TargetNodeName);
	if (!TargetNodePtr)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Target node not found: %s"), *TargetNodeName));
	}

	bool bConnected = false;
	if (!InvokeConnectNodes(GraphAsset, *SourceNodePtr, SourcePin, *TargetNodePtr, TargetPin, bConnected))
	{
		return FMCPToolResult::Error(TEXT("Failed to connect nodes. Could not invoke PCGGraph AddEdge via reflection."));
	}

	if (!bConnected)
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("Failed to connect %s.%s -> %s.%s"), *SourceNodeName, *SourcePin, *TargetNodeName, *TargetPin)
		);
	}

	GraphAsset->MarkPackageDirty();
	UEditorAssetLibrary::SaveAsset(GraphPath);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graphPath"), GraphPath);
	Data->SetStringField(TEXT("sourceNode"), SourceNodeName);
	Data->SetStringField(TEXT("sourcePin"), SourcePin);
	Data->SetStringField(TEXT("targetNode"), TargetNodeName);
	Data->SetStringField(TEXT("targetPin"), TargetPin);
	Data->SetBoolField(TEXT("connected"), true);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Connected %s.%s -> %s.%s"), *SourceNodeName, *SourcePin, *TargetNodeName, *TargetPin),
		Data);
}
