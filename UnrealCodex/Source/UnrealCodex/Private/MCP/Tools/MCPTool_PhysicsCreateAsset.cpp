// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PhysicsCreateAsset.h"

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

	void TrySetFactoryMesh(UFactory* Factory, UObject* SkeletalMeshObj)
	{
		if (!Factory || !SkeletalMeshObj)
		{
			return;
		}

		const FName CandidateProps[] = {
			TEXT("TargetSkeletalMesh"),
			TEXT("target_skeletal_mesh"),
			TEXT("PreviewSkeletalMesh")
		};

		for (const FName PropName : CandidateProps)
		{
			if (FObjectProperty* ObjProp = FindFProperty<FObjectProperty>(Factory->GetClass(), PropName))
			{
				ObjProp->SetObjectPropertyValue_InContainer(Factory, SkeletalMeshObj);
				return;
			}
		}
	}
}

FMCPToolResult FMCPTool_PhysicsCreateAsset::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString AssetName;
	if (!TryGetStringAlias(Params, TEXT("asset_name"), TEXT("assetName"), AssetName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: asset_name (or assetName)"));
	}

	FString AssetPath;
	if (!TryGetStringAlias(Params, TEXT("asset_path"), TEXT("assetPath"), AssetPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: asset_path (or assetPath)"));
	}

	if (!AssetPath.StartsWith(TEXT("/Game")))
	{
		return FMCPToolResult::Error(TEXT("asset_path must start with '/Game'"));
	}

	AssetPath.RemoveFromEnd(TEXT("/"));

	const FString PackagePath = FString::Printf(TEXT("%s/%s"), *AssetPath, *AssetName);
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);

	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath) || UEditorAssetLibrary::DoesAssetExist(PackagePath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("PhysicsAsset already exists: %s"), *ObjectPath));
	}

	UClass* PhysicsAssetClass = TryLoadFirstClass(UObject::StaticClass(), {
		TEXT("/Script/Engine.PhysicsAsset")
	});
	if (!PhysicsAssetClass)
	{
		return FMCPToolResult::Error(TEXT("PhysicsAsset class unavailable."));
	}

	UClass* FactoryClass = TryLoadFirstClass(UFactory::StaticClass(), {
		TEXT("/Script/UnrealEd.PhysicsAssetFactory")
	});
	if (!FactoryClass)
	{
		return FMCPToolResult::Error(TEXT("PhysicsAssetFactory unavailable in this editor build."));
	}

	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	if (!Factory)
	{
		return FMCPToolResult::Error(TEXT("Failed to instantiate PhysicsAssetFactory."));
	}

	FString SkeletalMeshPath;
	if (TryGetStringAlias(Params, TEXT("skeletal_mesh_path"), TEXT("skeletalMeshPath"), SkeletalMeshPath))
	{
		if (UObject* SkeletalMeshObj = StaticLoadObject(UObject::StaticClass(), nullptr, *SkeletalMeshPath))
		{
			TrySetFactoryMesh(Factory, SkeletalMeshObj);
		}
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, AssetPath, PhysicsAssetClass, Factory);
	if (!NewAsset)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create PhysicsAsset: %s"), *ObjectPath));
	}

	UEditorAssetLibrary::SaveLoadedAsset(NewAsset, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("assetName"), AssetName);
	Data->SetStringField(TEXT("assetPath"), AssetPath);
	Data->SetStringField(TEXT("objectPath"), NewAsset->GetPathName());

	return FMCPToolResult::Success(FString::Printf(TEXT("Created PhysicsAsset: %s"), *NewAsset->GetPathName()), Data);
}
