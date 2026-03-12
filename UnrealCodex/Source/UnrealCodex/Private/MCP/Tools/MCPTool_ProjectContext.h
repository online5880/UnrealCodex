// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Project context snapshot
 *
 * Provides a lightweight read-only snapshot of project metadata gathered by
 * FProjectContextManager.
 */
class FMCPTool_ProjectContext : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("project_context");
		Info.Description = TEXT(
			"Get a read-only project context snapshot. "
			"Use mode='summary' for compact metadata or mode='full' for expanded lists "
			"(source files, classes, and level actors with optional limits)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("mode"), TEXT("string"), TEXT("summary | full (default: summary)"), false, TEXT("summary")),
			FMCPToolParameter(TEXT("refresh"), TEXT("boolean"), TEXT("Force context refresh before returning data (default: false)"), false, TEXT("false")),
			FMCPToolParameter(TEXT("max_source_files"), TEXT("number"), TEXT("Max source files to return in full mode (default: 200)"), false, TEXT("200")),
			FMCPToolParameter(TEXT("max_classes"), TEXT("number"), TEXT("Max UCLASS entries to return in full mode (default: 200)"), false, TEXT("200")),
			FMCPToolParameter(TEXT("max_level_actors"), TEXT("number"), TEXT("Max level actors to return in full mode (default: 200)"), false, TEXT("200"))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
