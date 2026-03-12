// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * UMA compatibility wrappers for sourcecontrol_* tool names.
 * Delegates to the canonical `source_control` tool.
 */
class FMCPTool_SourcecontrolStatus : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sourcecontrol_status");
		Info.Description = TEXT("UMA alias for source_control_status. Get source control status for a file.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("file_path"), TEXT("string"), TEXT("Project-relative or absolute file path."), false),
			FMCPToolParameter(TEXT("filePath"), TEXT("string"), TEXT("Alias for file_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Alias for file_path/source_control parameter."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

class FMCPTool_SourcecontrolCheckout : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sourcecontrol_checkout");
		Info.Description = TEXT("UMA alias for source_control_checkout. Check out a file from source control.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("file_path"), TEXT("string"), TEXT("Project-relative or absolute file path."), false),
			FMCPToolParameter(TEXT("filePath"), TEXT("string"), TEXT("Alias for file_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Alias for file_path/source_control parameter."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		Info.Annotations.bDestructiveHint = false;
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

class FMCPTool_SourcecontrolDiff : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sourcecontrol_diff");
		Info.Description = TEXT("UMA alias for source_control_diff. Get diff for a file.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("file_path"), TEXT("string"), TEXT("Project-relative or absolute file path."), false),
			FMCPToolParameter(TEXT("filePath"), TEXT("string"), TEXT("Alias for file_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Alias for file_path/source_control parameter."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
