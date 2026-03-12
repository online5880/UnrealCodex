// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PhysicsSetProfile.h"

#include "EditorAssetLibrary.h"
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

	bool TryGetBoolAlias(const TSharedRef<FJsonObject>& Params, const TCHAR* Primary, const TCHAR* Alias, bool& OutValue)
	{
		if (Params->TryGetBoolField(Primary, OutValue))
		{
			return true;
		}
		if (Params->TryGetBoolField(Alias, OutValue))
		{
			return true;
		}
		return false;
	}

	bool TrySetNameProperty(UObject* Obj, const TArray<FName>& PropertyCandidates, const FName Value)
	{
		if (!Obj)
		{
			return false;
		}

		for (const FName PropName : PropertyCandidates)
		{
			if (FNameProperty* NameProp = FindFProperty<FNameProperty>(Obj->GetClass(), PropName))
			{
				NameProp->SetPropertyValue_InContainer(Obj, Value);
				return true;
			}
			if (FStrProperty* StrProp = FindFProperty<FStrProperty>(Obj->GetClass(), PropName))
			{
				StrProp->SetPropertyValue_InContainer(Obj, Value.ToString());
				return true;
			}
		}

		return false;
	}

	int32 ApplyProfileToBodySetups(UObject* PhysicsAssetObj, const FName ProfileName, const FString& BodyNameFilter, const bool bApplyToAllBodies)
	{
		if (!PhysicsAssetObj)
		{
			return 0;
		}

		FArrayProperty* BodySetupsProp = FindFProperty<FArrayProperty>(PhysicsAssetObj->GetClass(), TEXT("SkeletalBodySetups"));
		if (!BodySetupsProp)
		{
			return 0;
		}
		FObjectProperty* InnerObjProp = CastField<FObjectProperty>(BodySetupsProp->Inner);
		if (!InnerObjProp)
		{
			return 0;
		}

		void* ArrayPtr = BodySetupsProp->ContainerPtrToValuePtr<void>(PhysicsAssetObj);
		FScriptArrayHelper ArrayHelper(BodySetupsProp, ArrayPtr);

		int32 Updated = 0;
		for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
		{
			UObject* BodySetupObj = InnerObjProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
			if (!BodySetupObj)
			{
				continue;
			}

			bool bMatchesFilter = true;
			if (!BodyNameFilter.IsEmpty())
			{
				bMatchesFilter = false;
				if (FNameProperty* BoneNameProp = FindFProperty<FNameProperty>(BodySetupObj->GetClass(), TEXT("BoneName")))
				{
					const FName BoneName = BoneNameProp->GetPropertyValue_InContainer(BodySetupObj);
					bMatchesFilter = BoneName.ToString().Equals(BodyNameFilter, ESearchCase::IgnoreCase);
				}
			}
			else if (!bApplyToAllBodies)
			{
				bMatchesFilter = false;
			}

			if (!bMatchesFilter)
			{
				continue;
			}

			const bool bSet = TrySetNameProperty(
				BodySetupObj,
				{ TEXT("CollisionProfileName"), TEXT("ProfileName") },
				ProfileName
			);

			if (bSet)
			{
				BodySetupObj->Modify();
				BodySetupObj->PostEditChange();
				BodySetupObj->MarkPackageDirty();
				++Updated;
			}
		}

		return Updated;
	}
}

FMCPToolResult FMCPTool_PhysicsSetProfile::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString PhysicsAssetPath;
	if (!TryGetStringAlias(Params, TEXT("physics_asset_path"), TEXT("physicsAssetPath"), PhysicsAssetPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: physics_asset_path (or physicsAssetPath)"));
	}

	FString ProfileNameString;
	if (!TryGetStringAlias(Params, TEXT("profile_name"), TEXT("profileName"), ProfileNameString))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: profile_name (or profileName)"));
	}
	const FName ProfileName(*ProfileNameString);

	FString BodyName;
	TryGetStringAlias(Params, TEXT("body_name"), TEXT("bodyName"), BodyName);

	bool bApplyToAllBodies = BodyName.IsEmpty();
	TryGetBoolAlias(Params, TEXT("apply_to_all_bodies"), TEXT("applyToAllBodies"), bApplyToAllBodies);

	UObject* PhysicsAssetObj = StaticLoadObject(UObject::StaticClass(), nullptr, *PhysicsAssetPath);
	if (!PhysicsAssetObj)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load PhysicsAsset: %s"), *PhysicsAssetPath));
	}

	int32 UpdatedTargets = 0;

	if (TrySetNameProperty(
		PhysicsAssetObj,
		{ TEXT("CurrentConstraintProfileName"), TEXT("ConstraintProfile"), TEXT("CollisionProfileName"), TEXT("ProfileName") },
		ProfileName))
	{
		++UpdatedTargets;
	}

	UpdatedTargets += ApplyProfileToBodySetups(PhysicsAssetObj, ProfileName, BodyName, bApplyToAllBodies);

	if (UpdatedTargets == 0)
	{
		return FMCPToolResult::Error(TEXT("No writable profile fields found on PhysicsAsset/body setups for requested operation."));
	}

	PhysicsAssetObj->Modify();
	PhysicsAssetObj->PostEditChange();
	PhysicsAssetObj->MarkPackageDirty();
	UEditorAssetLibrary::SaveLoadedAsset(PhysicsAssetObj, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("physicsAssetPath"), PhysicsAssetPath);
	Data->SetStringField(TEXT("profileName"), ProfileName.ToString());
	Data->SetStringField(TEXT("bodyName"), BodyName);
	Data->SetBoolField(TEXT("applyToAllBodies"), bApplyToAllBodies);
	Data->SetNumberField(TEXT("updatedTargets"), UpdatedTargets);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Applied profile '%s' to %d target(s)."), *ProfileName.ToString(), UpdatedTargets),
		Data);
}
