// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_PhysicsGetInfo.h"

#include "EditorAssetLibrary.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

FMCPToolResult FMCPTool_PhysicsGetInfo::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString AssetPath = ExtractOptionalString(Params, TEXT("physics_asset_path"));
	if (AssetPath.IsEmpty())
	{
		AssetPath = ExtractOptionalString(Params, TEXT("physicsAssetPath"));
	}
	if (AssetPath.IsEmpty())
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: physics_asset_path (or physicsAssetPath)."));
	}

	UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(AssetPath);
	if (!LoadedAsset)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("PhysicsAsset not found: %s"), *AssetPath));
	}

	UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(LoadedAsset);
	if (!PhysicsAsset)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Asset is not a PhysicsAsset: %s (class=%s)"), *AssetPath, *LoadedAsset->GetClass()->GetName()));
	}

	TArray<TSharedPtr<FJsonValue>> BodiesArray;
	for (USkeletalBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		TSharedPtr<FJsonObject> BodyObj = MakeShared<FJsonObject>();
		BodyObj->SetStringField(TEXT("boneName"), BodySetup->BoneName.ToString());
		BodyObj->SetStringField(TEXT("collisionEnabled"), LexToString(BodySetup->DefaultInstance.GetCollisionEnabled()));
		BodiesArray.Add(MakeShared<FJsonValueObject>(BodyObj));
	}

	TArray<TSharedPtr<FJsonValue>> ConstraintsArray;
	for (UPhysicsConstraintTemplate* ConstraintTemplate : PhysicsAsset->ConstraintSetup)
	{
		if (!ConstraintTemplate)
		{
			continue;
		}

		const FConstraintInstance& ConstraintInstance = ConstraintTemplate->DefaultInstance;

		TSharedPtr<FJsonObject> ConstraintObj = MakeShared<FJsonObject>();
		ConstraintObj->SetStringField(TEXT("jointName"), ConstraintInstance.JointName.ToString());
		ConstraintsArray.Add(MakeShared<FJsonValueObject>(ConstraintObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("physicsAssetPath"), AssetPath);
	Data->SetArrayField(TEXT("bodies"), BodiesArray);
	Data->SetNumberField(TEXT("bodyCount"), BodiesArray.Num());
	Data->SetArrayField(TEXT("constraints"), ConstraintsArray);
	Data->SetNumberField(TEXT("constraintCount"), ConstraintsArray.Num());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved physics asset info: %s (%d bodies, %d constraints)"), *PhysicsAsset->GetName(), BodiesArray.Num(), ConstraintsArray.Num()),
		Data);
}
