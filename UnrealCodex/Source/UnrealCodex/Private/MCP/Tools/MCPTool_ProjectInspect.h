// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Project inspection utilities (read-only)
 *
 * Supports:
 * - plugins: list enabled project/engine plugins
 * - settings: key gameplay map/settings snapshot from config
 * - dependencies: module dependency snapshot from *.Build.cs files
 * - class_hierarchy: UCLASS inheritance snapshot from project context
 */
class FMCPTool_ProjectInspect : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("project_inspect");
		Info.Description = TEXT(
			"Read-only project inspection. "
			"Use action='plugins' to list enabled plugins, action='settings' for key config values, "
			"action='dependencies' for module dependencies, or action='class_hierarchy' for UCLASS inheritance."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("action"), TEXT("string"), TEXT("plugins | settings | dependencies | class_hierarchy"), true),
			FMCPToolParameter(TEXT("force_refresh"), TEXT("boolean"), TEXT("When action=class_hierarchy, refresh project context before reading."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecutePlugins() const;
	FMCPToolResult ExecuteSettings() const;
	FMCPToolResult ExecuteDependencies() const;
	FMCPToolResult ExecuteClassHierarchy(const TSharedRef<FJsonObject>& Params) const;
};
