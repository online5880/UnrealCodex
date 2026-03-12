// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Source control operations (status/checkout/diff)
 */
class FMCPTool_SourceControl : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("source_control");
		Info.Description = TEXT(
			"Run source control operations for project files. "
			"Supported actions: 'status', 'checkout', 'diff'. "
			"For 'diff', this tool uses git diff in the project directory when available."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("action"), TEXT("string"), TEXT("Operation to run: status | checkout | diff"), true),
			FMCPToolParameter(TEXT("file"), TEXT("string"), TEXT("Project-relative or absolute file path"), true),
			FMCPToolParameter(TEXT("max_chars"), TEXT("number"), TEXT("For diff action: max number of characters to return (default: 12000, max: 100000)"), false, TEXT("12000"))
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		Info.Annotations.bDestructiveHint = false;
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;

private:
	FString ResolveToAbsolutePath(const FString& InPath) const;
	FString ToProjectRelativePath(const FString& AbsolutePath) const;
	FMCPToolResult RunStatus(const FString& AbsolutePath) const;
	FMCPToolResult RunCheckout(const FString& AbsolutePath) const;
	FMCPToolResult RunDiff(const FString& AbsolutePath, int32 MaxChars) const;

	static FString BuildStatusSummary(
		bool bIsSourceControlled,
		bool bIsCheckedOut,
		bool bIsAdded,
		bool bIsDeleted,
		bool bIsModified,
		bool bIsCurrent
	);
};
