// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PhysicsCreateMaterial.h"

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

	void TrySetNumericProperty(UObject* Obj, const FName PropName, double Value)
	{
		if (!Obj)
		{
			return;
		}

		if (FDoubleProperty* DoubleProp = FindFProperty<FDoubleProperty>(Obj->GetClass(), PropName))
		{
			DoubleProp->SetPropertyValue_InContainer(Obj, Value);
			return;
		}
		if (FFloatProperty* FloatProp = FindFProperty<FFloatProperty>(Obj->GetClass(), PropName))
		{
			FloatProp->SetPropertyValue_InContainer(Obj, static_cast<float>(Value));
		}
	}
}

FMCPToolResult FMCPTool_PhysicsCreateMaterial::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString MaterialName;
	if (!TryGetStringAlias(Params, TEXT("material_name"), TEXT("materialName"), MaterialName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: material_name (or materialName)"));
	}

	FString MaterialPath;
	if (!TryGetStringAlias(Params, TEXT("material_path"), TEXT("materialPath"), MaterialPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: material_path (or materialPath)"));
	}

	if (!MaterialPath.StartsWith(TEXT("/Game")))
	{
		return FMCPToolResult::Error(TEXT("material_path must start with '/Game'"));
	}

	MaterialPath.RemoveFromEnd(TEXT("/"));

	double Friction = 0.7;
	double Restitution = 0.3;
	Params->TryGetNumberField(TEXT("friction"), Friction);
	Params->TryGetNumberField(TEXT("restitution"), Restitution);

	const FString PackagePath = FString::Printf(TEXT("%s/%s"), *MaterialPath, *MaterialName);
	const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *MaterialName);

	if (UEditorAssetLibrary::DoesAssetExist(ObjectPath) || UEditorAssetLibrary::DoesAssetExist(PackagePath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("PhysicalMaterial already exists: %s"), *ObjectPath));
	}

	UClass* PhysicalMaterialClass = TryLoadFirstClass(UObject::StaticClass(), {
		TEXT("/Script/PhysicsCore.PhysicalMaterial"),
		TEXT("/Script/Engine.PhysicalMaterial")
	});
	if (!PhysicalMaterialClass)
	{
		return FMCPToolResult::Error(TEXT("PhysicalMaterial class unavailable."));
	}

	UClass* FactoryClass = TryLoadFirstClass(UFactory::StaticClass(), {
		TEXT("/Script/UnrealEd.PhysicalMaterialFactoryNew")
	});
	if (!FactoryClass)
	{
		return FMCPToolResult::Error(TEXT("PhysicalMaterialFactoryNew unavailable in this editor build."));
	}

	UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
	if (!Factory)
	{
		return FMCPToolResult::Error(TEXT("Failed to instantiate PhysicalMaterialFactoryNew."));
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
	UObject* NewAsset = AssetToolsModule.Get().CreateAsset(MaterialName, MaterialPath, PhysicalMaterialClass, Factory);
	if (!NewAsset)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create PhysicalMaterial: %s"), *ObjectPath));
	}

	TrySetNumericProperty(NewAsset, TEXT("Friction"), Friction);
	TrySetNumericProperty(NewAsset, TEXT("Restitution"), Restitution);
	NewAsset->PostEditChange();
	NewAsset->MarkPackageDirty();
	UEditorAssetLibrary::SaveLoadedAsset(NewAsset, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("materialName"), MaterialName);
	Data->SetStringField(TEXT("materialPath"), MaterialPath);
	Data->SetStringField(TEXT("objectPath"), NewAsset->GetPathName());
	Data->SetNumberField(TEXT("friction"), Friction);
	Data->SetNumberField(TEXT("restitution"), Restitution);

	return FMCPToolResult::Success(FString::Printf(TEXT("Created PhysicalMaterial: %s"), *NewAsset->GetPathName()), Data);
}
