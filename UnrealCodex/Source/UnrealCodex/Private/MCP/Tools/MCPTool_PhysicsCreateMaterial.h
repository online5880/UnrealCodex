// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Create a PhysicalMaterial asset (UMA parity: physics_create_material.py)
 */
class FMCPTool_PhysicsCreateMaterial : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("physics_create_material");
		Info.Description = TEXT(
			"Create a new PhysicalMaterial and optionally set friction/restitution. "
			"Requires material_name/material_path (camelCase aliases supported)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("material_name"), TEXT("string"), TEXT("Name of the PhysicalMaterial to create."), false),
			FMCPToolParameter(TEXT("materialName"), TEXT("string"), TEXT("Alias for material_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("material_path"), TEXT("string"), TEXT("Destination folder path (e.g. /Game/Physics)."), false),
			FMCPToolParameter(TEXT("materialPath"), TEXT("string"), TEXT("Alias for material_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("friction"), TEXT("number"), TEXT("Friction value (default: 0.7)."), false, TEXT("0.7")),
			FMCPToolParameter(TEXT("restitution"), TEXT("number"), TEXT("Restitution value (default: 0.3)."), false, TEXT("0.3"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
