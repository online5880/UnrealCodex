// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PCGGetInfo.h"

#include "EditorAssetLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
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

	static FString TryExtractString(UObject* Source, const TCHAR* FieldName)
	{
		if (!Source)
		{
			return FString();
		}

		if (FStrProperty* StrProp = FindFProperty<FStrProperty>(Source->GetClass(), FieldName))
		{
			return StrProp->GetPropertyValue_InContainer(Source);
		}

		if (FNameProperty* NameProp = FindFProperty<FNameProperty>(Source->GetClass(), FieldName))
		{
			return NameProp->GetPropertyValue_InContainer(Source).ToString();
		}

		if (FTextProperty* TextProp = FindFProperty<FTextProperty>(Source->GetClass(), FieldName))
		{
			return TextProp->GetPropertyValue_InContainer(Source).ToString();
		}

		return FString();
	}

	static UObject* TryExtractObject(UObject* Source, const TCHAR* FieldName)
	{
		if (!Source)
		{
			return nullptr;
		}

		if (FObjectPropertyBase* ObjProp = FindFProperty<FObjectPropertyBase>(Source->GetClass(), FieldName))
		{
			return ObjProp->GetObjectPropertyValue_InContainer(Source);
		}
		return nullptr;
	}
}

FMCPToolResult FMCPTool_PCGGetInfo::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString GraphPath = ExtractOptionalString(Params, TEXT("graph_path"));
	if (GraphPath.IsEmpty())
	{
		GraphPath = ExtractOptionalString(Params, TEXT("graphPath"));
	}
	if (GraphPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: graph_path (or graphPath)."));
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

	TArray<UObject*> NodeObjects;
	if (!TryExtractObjectArray(GraphAsset, TEXT("Nodes"), NodeObjects))
	{
		TryExtractObjectArray(GraphAsset, TEXT("AllNodes"), NodeObjects);
	}

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UObject* Node : NodeObjects)
	{
		TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
		NodeObj->SetStringField(TEXT("nodeId"), Node->GetName());
		NodeObj->SetStringField(TEXT("nodeType"), Node->GetClass()->GetName());
		NodesArray.Add(MakeShared<FJsonValueObject>(NodeObj));
	}

	TArray<UObject*> EdgeObjects;
	if (!TryExtractObjectArray(GraphAsset, TEXT("Edges"), EdgeObjects))
	{
		TryExtractObjectArray(GraphAsset, TEXT("Connections"), EdgeObjects);
	}

	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	for (UObject* Edge : EdgeObjects)
	{
		UObject* SourceNodeObj = TryExtractObject(Edge, TEXT("InputNode"));
		UObject* TargetNodeObj = TryExtractObject(Edge, TEXT("OutputNode"));

		if (!SourceNodeObj)
		{
			SourceNodeObj = TryExtractObject(Edge, TEXT("SourceNode"));
		}
		if (!TargetNodeObj)
		{
			TargetNodeObj = TryExtractObject(Edge, TEXT("TargetNode"));
		}

		TSharedPtr<FJsonObject> ConnectionObj = MakeShared<FJsonObject>();
		ConnectionObj->SetStringField(TEXT("sourceNode"), SourceNodeObj ? SourceNodeObj->GetName() : FString());
		ConnectionObj->SetStringField(TEXT("sourcePin"), TryExtractString(Edge, TEXT("InputPinLabel")));
		ConnectionObj->SetStringField(TEXT("targetNode"), TargetNodeObj ? TargetNodeObj->GetName() : FString());
		ConnectionObj->SetStringField(TEXT("targetPin"), TryExtractString(Edge, TEXT("OutputPinLabel")));
		ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnectionObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graphPath"), GraphPath);
	Data->SetNumberField(TEXT("nodeCount"), NodesArray.Num());
	Data->SetArrayField(TEXT("nodes"), NodesArray);
	Data->SetArrayField(TEXT("connections"), ConnectionsArray);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved PCG graph info: %s (%d nodes, %d connections)"), *GraphAsset->GetName(), NodesArray.Num(), ConnectionsArray.Num()),
		Data);
}
