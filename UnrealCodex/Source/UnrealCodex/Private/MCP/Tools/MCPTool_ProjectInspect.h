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
			"Use action='plugins' to list enabled plugins, or action='settings' "
			"to return key project config values."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("action"), TEXT("string"), TEXT("plugins | settings"), true)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FMCPToolResult ExecutePlugins() const;
	FMCPToolResult ExecuteSettings() const;
};
