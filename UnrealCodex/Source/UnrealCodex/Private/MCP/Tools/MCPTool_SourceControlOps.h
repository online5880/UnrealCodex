// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"
#include "MCPTool_SourceControl.h"

/**
 * Thin wrapper tools for explicit source control operations.
 *
 * These improve LLM ergonomics versus a single action-multiplexed tool.
 */
class FMCPTool_SourceControlStatus : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("source_control_status");
		Info.Description = TEXT("Get source control status for a file.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Project-relative or absolute file path"), true)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

class FMCPTool_SourceControlCheckout : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("source_control_checkout");
		Info.Description = TEXT("Check out a file from source control.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Project-relative or absolute file path"), true)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		Info.Annotations.bDestructiveHint = false;
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

class FMCPTool_SourceControlDiff : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("source_control_diff");
		Info.Description = TEXT("Get git diff for a file in the project repository.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Project-relative or absolute file path"), true)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
