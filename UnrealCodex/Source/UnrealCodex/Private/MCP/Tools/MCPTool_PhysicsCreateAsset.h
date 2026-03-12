// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Create a PhysicsAsset (UMA parity: physics_create_asset.py)
 */
class FMCPTool_PhysicsCreateAsset : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("physics_create_asset");
		Info.Description = TEXT(
			"Create a new PhysicsAsset. "
			"Requires asset_name/asset_path (camelCase aliases supported). "
			"Optionally accepts skeletal_mesh_path to initialize against a skeletal mesh."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("asset_name"), TEXT("string"), TEXT("Name of the PhysicsAsset to create."), false),
			FMCPToolParameter(TEXT("assetName"), TEXT("string"), TEXT("Alias for asset_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("asset_path"), TEXT("string"), TEXT("Destination folder path (e.g. /Game/Physics)."), false),
			FMCPToolParameter(TEXT("assetPath"), TEXT("string"), TEXT("Alias for asset_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("skeletal_mesh_path"), TEXT("string"), TEXT("Optional skeletal mesh asset path."), false),
			FMCPToolParameter(TEXT("skeletalMeshPath"), TEXT("string"), TEXT("Alias for skeletal_mesh_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
