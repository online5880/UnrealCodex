// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SourceControl.h"

#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlState.h"
#include "SourceControlOperations.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

FMCPToolResult FMCPTool_SourceControl::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Action;
	FString FilePath;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("action"), Action, ParamError))
	{
		return ParamError.GetValue();
	}
	if (!ExtractRequiredString(Params, TEXT("file"), FilePath, ParamError))
	{
		return ParamError.GetValue();
	}

	Action = Action.ToLower();
	const FString AbsolutePath = ResolveToAbsolutePath(FilePath);

	if (!FPaths::FileExists(AbsolutePath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("File not found: %s"), *AbsolutePath));
	}

	if (Action == TEXT("status"))
	{
		return RunStatus(AbsolutePath);
	}
	if (Action == TEXT("checkout"))
	{
		return RunCheckout(AbsolutePath);
	}
	if (Action == TEXT("diff"))
	{
		return RunDiff(AbsolutePath);
	}

	return FMCPToolResult::Error(FString::Printf(TEXT("Unsupported source_control action: %s"), *Action));
}

FString FMCPTool_SourceControl::ResolveToAbsolutePath(const FString& InPath) const
{
	if (FPaths::IsRelative(InPath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), InPath));
	}
	return FPaths::ConvertRelativePathToFull(InPath);
}

FString FMCPTool_SourceControl::ToProjectRelativePath(const FString& AbsolutePath) const
{
	FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	FString NormalizedAbs = FPaths::ConvertRelativePathToFull(AbsolutePath);

	FPaths::NormalizeDirectoryName(ProjectDir);
	FPaths::NormalizeFilename(NormalizedAbs);

	if (NormalizedAbs.StartsWith(ProjectDir))
	{
		FString Relative = NormalizedAbs.RightChop(ProjectDir.Len());
		while (Relative.StartsWith(TEXT("/")) || Relative.StartsWith(TEXT("\\")))
		{
			Relative.RightChopInline(1, false);
		}
		return Relative;
	}

	return NormalizedAbs;
}

FMCPToolResult FMCPTool_SourceControl::RunStatus(const FString& AbsolutePath) const
{
	if (!ISourceControlModule::Get().IsEnabled())
	{
		return FMCPToolResult::Error(TEXT("Source control is not enabled in this editor session."));
	}

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	const FSourceControlOperationRef UpdateOp = ISourceControlOperation::Create<FUpdateStatus>();
	const ECommandResult::Type UpdateResult = Provider.Execute(UpdateOp, AbsolutePath, EConcurrency::Synchronous);
	if (UpdateResult != ECommandResult::Succeeded)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to query source control status for: %s"), *AbsolutePath));
	}

	FSourceControlStatePtr State = Provider.GetState(AbsolutePath, EStateCacheUsage::Use);
	if (!State.IsValid())
	{
		return FMCPToolResult::Error(TEXT("Source control state is unavailable for this file."));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("file"), ToProjectRelativePath(AbsolutePath));
	Data->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Data->SetBoolField(TEXT("is_source_controlled"), State->IsSourceControlled());
	Data->SetBoolField(TEXT("is_checked_out"), State->IsCheckedOut());
	Data->SetBoolField(TEXT("is_added"), State->IsAdded());
	Data->SetBoolField(TEXT("is_deleted"), State->IsDeleted());
	Data->SetBoolField(TEXT("is_modified"), State->IsModified());
	Data->SetBoolField(TEXT("can_checkout"), State->CanCheckout());
	Data->SetBoolField(TEXT("is_current"), State->IsCurrent());
	Data->SetStringField(TEXT("display_name"), State->GetDisplayName().ToString());

	return FMCPToolResult::Success(TEXT("Source control status retrieved."), Data);
}

FMCPToolResult FMCPTool_SourceControl::RunCheckout(const FString& AbsolutePath) const
{
	if (!ISourceControlModule::Get().IsEnabled())
	{
		return FMCPToolResult::Error(TEXT("Source control is not enabled in this editor session."));
	}

	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	const FSourceControlOperationRef CheckoutOp = ISourceControlOperation::Create<FCheckOut>();
	const ECommandResult::Type CheckoutResult = Provider.Execute(CheckoutOp, AbsolutePath, EConcurrency::Synchronous);
	if (CheckoutResult != ECommandResult::Succeeded)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Checkout failed for file: %s"), *AbsolutePath));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("file"), ToProjectRelativePath(AbsolutePath));
	Data->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Data->SetBoolField(TEXT("checked_out"), true);
	return FMCPToolResult::Success(TEXT("File checked out successfully."), Data);
}

FMCPToolResult FMCPTool_SourceControl::RunDiff(const FString& AbsolutePath) const
{
	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	const FString RelativePath = ToProjectRelativePath(AbsolutePath);

	FString StdOut;
	FString StdErr;
	int32 ReturnCode = -1;
	const FString Args = FString::Printf(TEXT("-C \"%s\" diff -- \"%s\""), *ProjectDir, *RelativePath);
	const bool bExecOk = FPlatformProcess::ExecProcess(TEXT("git"), *Args, &ReturnCode, &StdOut, &StdErr);

	if (!bExecOk)
	{
		return FMCPToolResult::Error(TEXT("Failed to execute git for diff. Ensure git is available in PATH."));
	}

	if (ReturnCode != 0)
	{
		const FString ErrorText = StdErr.IsEmpty()
			? FString::Printf(TEXT("git diff failed with exit code %d"), ReturnCode)
			: StdErr;
		return FMCPToolResult::Error(ErrorText);
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("file"), RelativePath);
	Data->SetStringField(TEXT("diff"), StdOut);
	Data->SetNumberField(TEXT("length"), StdOut.Len());

	if (StdOut.IsEmpty())
	{
		return FMCPToolResult::Success(TEXT("No local diff for file."), Data);
	}

	return FMCPToolResult::Success(TEXT("Diff retrieved."), Data);
}
