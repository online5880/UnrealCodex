// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Update constraint settings in a PhysicsAsset (UMA parity: physics_set_constraint.py)
 */
class FMCPTool_PhysicsSetConstraint : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("physics_set_constraint");
		Info.Description = TEXT(
			"Set limits/flags for a named constraint in a PhysicsAsset. "
			"Requires physics_asset_path and constraint_name (camelCase aliases supported)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("physics_asset_path"), TEXT("string"), TEXT("Path to PhysicsAsset."), false),
			FMCPToolParameter(TEXT("physicsAssetPath"), TEXT("string"), TEXT("Alias for physics_asset_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("constraint_name"), TEXT("string"), TEXT("Constraint/joint name to update."), false),
			FMCPToolParameter(TEXT("constraintName"), TEXT("string"), TEXT("Alias for constraint_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("linear_limit"), TEXT("number"), TEXT("Linear limit size."), false),
			FMCPToolParameter(TEXT("linearLimit"), TEXT("number"), TEXT("Alias for linear_limit."), false),
			FMCPToolParameter(TEXT("swing1_limit"), TEXT("number"), TEXT("Swing1 limit angle."), false),
			FMCPToolParameter(TEXT("swing1Limit"), TEXT("number"), TEXT("Alias for swing1_limit."), false),
			FMCPToolParameter(TEXT("swing2_limit"), TEXT("number"), TEXT("Swing2 limit angle."), false),
			FMCPToolParameter(TEXT("swing2Limit"), TEXT("number"), TEXT("Alias for swing2_limit."), false),
			FMCPToolParameter(TEXT("twist_limit"), TEXT("number"), TEXT("Twist limit angle."), false),
			FMCPToolParameter(TEXT("twistLimit"), TEXT("number"), TEXT("Alias for twist_limit."), false),
			FMCPToolParameter(TEXT("disable_collision"), TEXT("boolean"), TEXT("Disable collision between constrained bodies."), false),
			FMCPToolParameter(TEXT("disableCollision"), TEXT("boolean"), TEXT("Alias for disable_collision."), false),
			FMCPToolParameter(TEXT("parent_dominates"), TEXT("boolean"), TEXT("Whether parent dominates simulation."), false),
			FMCPToolParameter(TEXT("parentDominates"), TEXT("boolean"), TEXT("Alias for parent_dominates."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
