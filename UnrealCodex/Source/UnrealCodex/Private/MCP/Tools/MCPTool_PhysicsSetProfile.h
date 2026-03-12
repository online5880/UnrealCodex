// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Set a physics profile on a PhysicsAsset / body setups (UMA parity: physics_set_profile.py)
 */
class FMCPTool_PhysicsSetProfile : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("physics_set_profile");
		Info.Description = TEXT(
			"Set physics/collision profile fields on a PhysicsAsset. "
			"Requires physics_asset_path and profile_name (camelCase aliases supported). "
			"Optionally filters by body_name."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to PhysicsAsset (e.g. /Game/Physics/PA_Character.PA_Character)."), false),
			FMCPToolParameter(TEXT("physicsAssetPath"), TEXT("string"), TEXT("Alias for physics_asset_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("profile_name"), TEXT("string"), TEXT("Profile name to apply."), false),
			FMCPToolParameter(TEXT("profileName"), TEXT("string"), TEXT("Alias for profile_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("body_name"), TEXT("string"), TEXT("Optional body (bone) name filter."), false),
			FMCPToolParameter(TEXT("bodyName"), TEXT("string"), TEXT("Alias for body_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("apply_to_all_bodies"), TEXT("boolean"), TEXT("Apply profile to all body setups (default: true when body_name omitted)."), false, TEXT("true")),
			FMCPToolParameter(TEXT("applyToAllBodies"), TEXT("boolean"), TEXT("Alias for apply_to_all_bodies (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
