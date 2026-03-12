// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PCGCreateGraph.h"

#include "AssetToolsModule.h"
#include "EditorAssetLibrary.h"
#include "Modules/ModuleManager.h"
#include "Factories/Factory.h"
#include "UObject/UnrealType.h"

namespace
{
	bool TryGetStringAlias(const TSharedRef<FJsonObject>& Params, const TCHAR* Primary, const TCHAR* Alias, FString& OutValue)
	{
		if (Params->TryGetStringField(Primary, OutValue) && !OutValue.IsEmpty())
		{
			return true;
		}
		if (Params->TryGetStringField(Alias, OutValue) && !OutValue.IsEmpty())
		{
			return true;
		}
		return false;
	}

	UClass* TryLoadFirstClass(UClass* BaseClass, std::initializer_list<const TCHAR*> ClassPaths)
	{
		for (const TCHAR* ClassPath : ClassPaths)
		{
			if (UClass* Loaded = StaticLoadClass(BaseClass, nullptr, ClassPath))
			{
				return Loaded;
			}
		}
		return nullptr;
	}
}

FMCPToolResult FMCPTool_PCGCreateGraph::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString GraphName;
	if (!TryGetStringAlias(Params, TEXT("graph_name"), TEXT("graphName"), GraphName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: graph_name (or graphName)"));
	}

	FString GraphPath;
	if (!TryGetStringAlias(Params, TEXT("graph_path"), TEXT("graphPath"), GraphPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: graph_path (or graphPath)"));
	}

	if (!GraphPath.StartsWith(TEXT("/Game")))
	{
		return FMCPToolResult::Error(TEXT("graph_path must start with '/Game'"));
	}

	GraphPath.RemoveFromEnd(TEXT("/"));

	const FString PackagePath = FString::Printf(TEXT("%s/%s"), *GraphPath, *GraphName);
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *GraphName);

	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath) || UEditorAssetLibrary::DoesAssetExist(PackagePath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("PCGGraph already exists: %s"), *ObjectPath));
	}

	UClass* GraphClass = TryLoadFirstClass(UObject::StaticClass(), {
		TEXT("/Script/PCG.PCGGraph")
	});
	if (!GraphClass)
	{
		return FMCPToolResult::Error(TEXT("PCGGraph class unavailable. Ensure PCG plugin is enabled."));
	}

	UClass* FactoryClass = TryLoadFirstClass(UFactory::StaticClass(), {
		TEXT("/Script/PCGEditor.PCGGraphFactory")
	});
	if (!FactoryClass)
	{
		return FMCPToolResult::Error(TEXT("PCGGraphFactory unavailable. Ensure PCG Editor plugin is enabled."));
	}

	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	if (!Factory)
	{
		return FMCPToolResult::Error(TEXT("Failed to instantiate PCGGraphFactory."));
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(GraphName, GraphPath, GraphClass, Factory);
	if (!NewAsset)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create PCGGraph: %s"), *ObjectPath));
	}

	UEditorAssetLibrary::SaveLoadedAsset(NewAsset, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("graphPath"), ObjectPath);
	Data->SetStringField(TEXT("graphName"), GraphName);
	Data->SetBoolField(TEXT("created"), true);

	return FMCPToolResult::Success(FString::Printf(TEXT("Created PCGGraph: %s"), *ObjectPath), Data);
}
