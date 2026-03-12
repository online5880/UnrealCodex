// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SourceControlOps.h"

namespace
{
	FMCPToolResult ExecuteDelegatedSourceControl(const TSharedRef<FJsonObject>& Params, const FString& Action)
	{
		FString FilePath;
		if (!Params->TryGetStringField(TEXT("file"), FilePath) || FilePath.IsEmpty())
		{
			return FMCPToolResult::Error(TEXT("Missing required parameter: file"));
		}

		TSharedRef<FJsonObject> ForwardParams = MakeShared<FJsonObject>();
		ForwardParams->SetStringField(TEXT("action"), Action);
		ForwardParams->SetStringField(TEXT("file"), FilePath);

		FMCPTool_SourceControl DelegatedTool;
		return DelegatedTool.Execute(ForwardParams);
	}
}

FMCPToolResult FMCPTool_SourceControlStatus::Execute(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteDelegatedSourceControl(Params, TEXT("status"));
}

FMCPToolResult FMCPTool_SourceControlCheckout::Execute(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteDelegatedSourceControl(Params, TEXT("checkout"));
}

FMCPToolResult FMCPTool_SourceControlDiff::Execute(const TSharedRef<FJsonObject>& Params)
{
	return ExecuteDelegatedSourceControl(Params, TEXT("diff"));
}
