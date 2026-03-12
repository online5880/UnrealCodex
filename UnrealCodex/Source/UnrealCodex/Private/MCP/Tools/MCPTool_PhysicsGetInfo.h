// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Read physics asset information (UMA parity: physics_get_info.py)
 */
class FMCPTool_PhysicsGetInfo : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("physics_get_info");
		Info.Description = TEXT(
			"Read bodies and constraints from a PhysicsAsset. "
			"Requires physics_asset_path (or physicsAssetPath alias)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("physics_asset_path"), TEXT("string"), TEXT("PhysicsAsset path (e.g. /Game/Characters/PA_Player.PA_Player)"), false),
			FMCPToolParameter(TEXT("physicsAssetPath"), TEXT("string"), TEXT("Alias for physics_asset_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
