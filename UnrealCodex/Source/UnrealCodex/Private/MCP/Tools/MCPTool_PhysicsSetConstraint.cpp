// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PhysicsSetConstraint.h"

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

	template<typename NumericType>
	bool TryGetNumberAlias(const TSharedRef<FJsonObject>& Params, const TCHAR* Primary, const TCHAR* Alias, NumericType& OutValue)
	{
		double Temp = 0.0;
		if (Params->TryGetNumberField(Primary, Temp) || Params->TryGetNumberField(Alias, Temp))
		{
			OutValue = static_cast<NumericType>(Temp);
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

	bool TrySetNumericOnStruct(UStruct* StructType, void* StructData, const TArray<FName>& CandidateNames, double Value)
	{
		if (!StructType || !StructData)
		{
			return false;
		}

		for (const FName FieldName : CandidateNames)
		{
			if (FDoubleProperty* DoubleProp = FindFProperty<FDoubleProperty>(StructType, FieldName))
			{
				DoubleProp->SetPropertyValue_InContainer(StructData, Value);
				return true;
			}
			if (FFloatProperty* FloatProp = FindFProperty<FFloatProperty>(StructType, FieldName))
			{
				FloatProp->SetPropertyValue_InContainer(StructData, static_cast<float>(Value));
				return true;
			}
			if (FIntProperty* IntProp = FindFProperty<FIntProperty>(StructType, FieldName))
			{
				IntProp->SetPropertyValue_InContainer(StructData, static_cast<int32>(Value));
				return true;
			}
		}
		return false;
	}

	bool TrySetBoolOnStruct(UStruct* StructType, void* StructData, const TArray<FName>& CandidateNames, bool bValue)
	{
		if (!StructType || !StructData)
		{
			return false;
		}

		for (const FName FieldName : CandidateNames)
		{
			if (FBoolProperty* BoolProp = FindFProperty<FBoolProperty>(StructType, FieldName))
			{
				BoolProp->SetPropertyValue_InContainer(StructData, bValue);
				return true;
			}
		}
		return false;
	}

	bool TryReadConstraintName(UObject* ConstraintObj, FString& OutConstraintName)
	{
		if (!ConstraintObj)
		{
			return false;
		}

		if (FNameProperty* JointNameProp = FindFProperty<FNameProperty>(ConstraintObj->GetClass(), TEXT("JointName")))
		{
			OutConstraintName = JointNameProp->GetPropertyValue_InContainer(ConstraintObj).ToString();
			if (!OutConstraintName.IsEmpty())
			{
				return true;
			}
		}

		if (FStructProperty* DefaultInstanceProp = FindFProperty<FStructProperty>(ConstraintObj->GetClass(), TEXT("DefaultInstance")))
		{
			void* DefaultInstanceData = DefaultInstanceProp->ContainerPtrToValuePtr<void>(ConstraintObj);
			if (FNameProperty* StructJointNameProp = FindFProperty<FNameProperty>(DefaultInstanceProp->Struct, TEXT("JointName")))
			{
				OutConstraintName = StructJointNameProp->GetPropertyValue_InContainer(DefaultInstanceData).ToString();
				if (!OutConstraintName.IsEmpty())
				{
					return true;
				}
			}
		}

		return false;
	}
}

FMCPToolResult FMCPTool_PhysicsSetConstraint::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString PhysicsAssetPath;
	if (!TryGetStringAlias(Params, TEXT("physics_asset_path"), TEXT("physicsAssetPath"), PhysicsAssetPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: physics_asset_path (or physicsAssetPath)"));
	}

	FString ConstraintName;
	if (!TryGetStringAlias(Params, TEXT("constraint_name"), TEXT("constraintName"), ConstraintName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: constraint_name (or constraintName)"));
	}

	UObject* PhysicsAssetObj = StaticLoadObject(UObject::StaticClass(), nullptr, *PhysicsAssetPath);
	if (!PhysicsAssetObj)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to load PhysicsAsset: %s"), *PhysicsAssetPath));
	}

	FArrayProperty* ConstraintArrayProp = FindFProperty<FArrayProperty>(PhysicsAssetObj->GetClass(), TEXT("ConstraintSetup"));
	if (!ConstraintArrayProp)
	{
		ConstraintArrayProp = FindFProperty<FArrayProperty>(PhysicsAssetObj->GetClass(), TEXT("ConstraintSetups"));
	}
	if (!ConstraintArrayProp)
	{
		return FMCPToolResult::Error(TEXT("PhysicsAsset does not expose ConstraintSetup array in this engine build."));
	}

	FObjectProperty* ConstraintObjProp = CastField<FObjectProperty>(ConstraintArrayProp->Inner);
	if (!ConstraintObjProp)
	{
		return FMCPToolResult::Error(TEXT("ConstraintSetup array has unexpected inner type."));
	}

	void* ArrayPtr = ConstraintArrayProp->ContainerPtrToValuePtr<void>(PhysicsAssetObj);
	FScriptArrayHelper ConstraintArrayHelper(ConstraintArrayProp, ArrayPtr);

	double LinearLimit = 0.0;
	double Swing1Limit = 0.0;
	double Swing2Limit = 0.0;
	double TwistLimit = 0.0;
	bool bHasLinear = TryGetNumberAlias(Params, TEXT("linear_limit"), TEXT("linearLimit"), LinearLimit);
	bool bHasSwing1 = TryGetNumberAlias(Params, TEXT("swing1_limit"), TEXT("swing1Limit"), Swing1Limit);
	bool bHasSwing2 = TryGetNumberAlias(Params, TEXT("swing2_limit"), TEXT("swing2Limit"), Swing2Limit);
	bool bHasTwist = TryGetNumberAlias(Params, TEXT("twist_limit"), TEXT("twistLimit"), TwistLimit);

	bool bDisableCollision = false;
	bool bParentDominates = false;
	bool bHasDisableCollision = TryGetBoolAlias(Params, TEXT("disable_collision"), TEXT("disableCollision"), bDisableCollision);
	bool bHasParentDominates = TryGetBoolAlias(Params, TEXT("parent_dominates"), TEXT("parentDominates"), bParentDominates);

	if (!bHasLinear && !bHasSwing1 && !bHasSwing2 && !bHasTwist && !bHasDisableCollision && !bHasParentDominates)
	{
		return FMCPToolResult::Error(TEXT("No constraint settings provided. Set at least one limit/flag parameter."));
	}

	int32 UpdatedCount = 0;
	for (int32 Index = 0; Index < ConstraintArrayHelper.Num(); ++Index)
	{
		UObject* ConstraintObj = ConstraintObjProp->GetObjectPropertyValue(ConstraintArrayHelper.GetRawPtr(Index));
		if (!ConstraintObj)
		{
			continue;
		}

		FString FoundName;
		if (!TryReadConstraintName(ConstraintObj, FoundName) || !FoundName.Equals(ConstraintName, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FStructProperty* DefaultInstanceProp = FindFProperty<FStructProperty>(ConstraintObj->GetClass(), TEXT("DefaultInstance"));
		if (!DefaultInstanceProp)
		{
			continue;
		}
		void* DefaultInstanceData = DefaultInstanceProp->ContainerPtrToValuePtr<void>(ConstraintObj);
		UStruct* DefaultInstanceStruct = DefaultInstanceProp->Struct;
		if (!DefaultInstanceData || !DefaultInstanceStruct)
		{
			continue;
		}

		bool bAnySet = false;
		if (bHasLinear)
		{
			bAnySet |= TrySetNumericOnStruct(DefaultInstanceStruct, DefaultInstanceData, { TEXT("LinearLimitSize") }, LinearLimit);
		}
		if (bHasSwing1)
		{
			bAnySet |= TrySetNumericOnStruct(DefaultInstanceStruct, DefaultInstanceData, { TEXT("Swing1LimitAngle") }, Swing1Limit);
		}
		if (bHasSwing2)
		{
			bAnySet |= TrySetNumericOnStruct(DefaultInstanceStruct, DefaultInstanceData, { TEXT("Swing2LimitAngle") }, Swing2Limit);
		}
		if (bHasTwist)
		{
			bAnySet |= TrySetNumericOnStruct(DefaultInstanceStruct, DefaultInstanceData, { TEXT("TwistLimitAngle") }, TwistLimit);
		}
		if (bHasDisableCollision)
		{
			bAnySet |= TrySetBoolOnStruct(DefaultInstanceStruct, DefaultInstanceData, { TEXT("bDisableCollision") }, bDisableCollision);
		}
		if (bHasParentDominates)
		{
			bAnySet |= TrySetBoolOnStruct(DefaultInstanceStruct, DefaultInstanceData, { TEXT("bParentDominates") }, bParentDominates);
		}

		if (bAnySet)
		{
			ConstraintObj->Modify();
			ConstraintObj->PostEditChange();
			ConstraintObj->MarkPackageDirty();
			++UpdatedCount;
		}
	}

	if (UpdatedCount == 0)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Constraint '%s' not found or writable fields unavailable."), *ConstraintName));
	}

	PhysicsAssetObj->Modify();
	PhysicsAssetObj->PostEditChange();
	PhysicsAssetObj->MarkPackageDirty();
	UEditorAssetLibrary::SaveLoadedAsset(PhysicsAssetObj, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("physicsAssetPath"), PhysicsAssetPath);
	Data->SetStringField(TEXT("constraintName"), ConstraintName);
	Data->SetNumberField(TEXT("updatedConstraints"), UpdatedCount);
	if (bHasLinear) Data->SetNumberField(TEXT("linearLimit"), LinearLimit);
	if (bHasSwing1) Data->SetNumberField(TEXT("swing1Limit"), Swing1Limit);
	if (bHasSwing2) Data->SetNumberField(TEXT("swing2Limit"), Swing2Limit);
	if (bHasTwist) Data->SetNumberField(TEXT("twistLimit"), TwistLimit);
	if (bHasDisableCollision) Data->SetBoolField(TEXT("disableCollision"), bDisableCollision);
	if (bHasParentDominates) Data->SetBoolField(TEXT("parentDominates"), bParentDominates);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Updated constraint '%s' in %d template(s)."), *ConstraintName, UpdatedCount),
		Data);
}
