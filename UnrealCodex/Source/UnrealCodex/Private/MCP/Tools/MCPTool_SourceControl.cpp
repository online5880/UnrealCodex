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
	const int32 MaxChars = FMath::Clamp(ExtractOptionalNumber<int32>(Params, TEXT("max_chars"), 12000), 100, 100000);
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
		return RunDiff(AbsolutePath, MaxChars);
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

	const bool bIsSourceControlled = State->IsSourceControlled();
	const bool bIsCheckedOut = State->IsCheckedOut();
	const bool bIsAdded = State->IsAdded();
	const bool bIsDeleted = State->IsDeleted();
	const bool bIsModified = State->IsModified();
	const bool bIsCurrent = State->IsCurrent();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action"), TEXT("status"));
	Data->SetStringField(TEXT("file"), ToProjectRelativePath(AbsolutePath));
	Data->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Data->SetBoolField(TEXT("is_source_controlled"), bIsSourceControlled);
	Data->SetBoolField(TEXT("is_checked_out"), bIsCheckedOut);
	Data->SetBoolField(TEXT("is_added"), bIsAdded);
	Data->SetBoolField(TEXT("is_deleted"), bIsDeleted);
	Data->SetBoolField(TEXT("is_modified"), bIsModified);
	Data->SetBoolField(TEXT("can_checkout"), State->CanCheckout());
	Data->SetBoolField(TEXT("is_current"), bIsCurrent);
	Data->SetStringField(TEXT("display_name"), State->GetDisplayName().ToString());
	Data->SetStringField(TEXT("summary"), BuildStatusSummary(bIsSourceControlled, bIsCheckedOut, bIsAdded, bIsDeleted, bIsModified, bIsCurrent));

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
	Data->SetStringField(TEXT("action"), TEXT("checkout"));
	Data->SetStringField(TEXT("file"), ToProjectRelativePath(AbsolutePath));
	Data->SetStringField(TEXT("provider"), Provider.GetName().ToString());
	Data->SetBoolField(TEXT("checked_out"), true);
	Data->SetStringField(TEXT("summary"), TEXT("Checked out successfully."));
	return FMCPToolResult::Success(TEXT("File checked out successfully."), Data);
}

FMCPToolResult FMCPTool_SourceControl::RunDiff(const FString& AbsolutePath, int32 MaxChars) const
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

	const int32 FullLength = StdOut.Len();
	const bool bTruncated = FullLength > MaxChars;
	const FString DiffText = bTruncated ? StdOut.Left(MaxChars) : StdOut;

	int32 AddedLines = 0;
	int32 RemovedLines = 0;
	TArray<FString> Lines;
	StdOut.ParseIntoArrayLines(Lines, true);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("+++")) || Line.StartsWith(TEXT("---")))
		{
			continue;
		}
		if (Line.StartsWith(TEXT("+")))
		{
			++AddedLines;
		}
		else if (Line.StartsWith(TEXT("-")))
		{
			++RemovedLines;
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action"), TEXT("diff"));
	Data->SetStringField(TEXT("file"), RelativePath);
	Data->SetStringField(TEXT("diff"), DiffText);
	Data->SetNumberField(TEXT("length"), FullLength);
	Data->SetNumberField(TEXT("returned_length"), DiffText.Len());
	Data->SetBoolField(TEXT("truncated"), bTruncated);
	Data->SetNumberField(TEXT("max_chars"), MaxChars);
	Data->SetNumberField(TEXT("added_lines"), AddedLines);
	Data->SetNumberField(TEXT("removed_lines"), RemovedLines);

	if (StdOut.IsEmpty())
	{
		Data->SetStringField(TEXT("summary"), TEXT("No local diff."));
		return FMCPToolResult::Success(TEXT("No local diff for file."), Data);
	}

	Data->SetStringField(TEXT("summary"), FString::Printf(TEXT("Diff retrieved (+%d/-%d)%s"), AddedLines, RemovedLines, bTruncated ? TEXT(" [truncated]") : TEXT("")));
	return FMCPToolResult::Success(TEXT("Diff retrieved."), Data);
}

FString FMCPTool_SourceControl::BuildStatusSummary(
	bool bIsSourceControlled,
	bool bIsCheckedOut,
	bool bIsAdded,
	bool bIsDeleted,
	bool bIsModified,
	bool bIsCurrent)
{
	if (!bIsSourceControlled)
	{
		return TEXT("Not under source control");
	}
	if (bIsDeleted)
	{
		return TEXT("Marked deleted");
	}
	if (bIsAdded)
	{
		return TEXT("Added (pending submit)");
	}
	if (bIsCheckedOut && bIsModified)
	{
		return TEXT("Checked out with local modifications");
	}
	if (bIsCheckedOut)
	{
		return TEXT("Checked out");
	}
	if (bIsModified)
	{
		return TEXT("Locally modified");
	}
	if (!bIsCurrent)
	{
		return TEXT("Out of date");
	}
	return TEXT("Up to date");
}
