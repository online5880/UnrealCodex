// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SourceControlUmaAliases.h"

#include "MCPTool_SourceControl.h"

namespace
{
	bool TryGetPathAlias(const TSharedRef<FJsonObject>& Params, FString& OutFilePath)
	{
		if (Params->TryGetStringField(TEXT("file_path"), OutFilePath) && !OutFilePath.IsEmpty())
		{
			return true;
		}
		if (Params->TryGetStringField(TEXT("filePath"), OutFilePath) && !OutFilePath.IsEmpty())
		{
			return true;
		}
		if (Params->TryGetStringField(TEXT("file"), OutFilePath) && !OutFilePath.IsEmpty())
		{
			return true;
		}
		return false;
	}

	FMCPToolResult ExecuteDelegatedSourceControl(const TSharedRef<FJsonObject>& Params, const FString& Action)
	{
		FString FilePath;
		if (!TryGetPathAlias(Params, FilePath))
		{
			return FMCPToolResult::Error(TEXT("Missing required parameter: file_path (or filePath/file)"));
		}

		TSharedRef<FJsonObject> ForwardParams = MakeShared<FJsonObject>();
		ForwardParams->SetStringField(TEXT("action"), Action);
		ForwardParams->SetStringField(TEXT("file"), FilePath);

		FMCPTool_SourceControl DelegatedTool;
		return DelegatedTool.Execute(ForwardParams);
	}
}

FMCPToolResult FMCPTool_SourcecontrolStatus::Execute(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteDelegatedSourceControl(Params, TEXT("status"));
}

FMCPToolResult FMCPTool_SourcecontrolCheckout::Execute(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteDelegatedSourceControl(Params, TEXT("checkout"));
}

FMCPToolResult FMCPTool_SourcecontrolDiff::Execute(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteDelegatedSourceControl(Params, TEXT("diff"));
}
