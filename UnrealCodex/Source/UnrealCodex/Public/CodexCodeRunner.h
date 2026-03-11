// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ICodexRunner.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"

/**
 * Async runner for Codex-first CLI commands (cross-platform implementation)
 * Executes non-interactive CLI requests and captures output
 * Implements ICodexRunner interface for abstraction
 */
class UNREALCODEX_API FCodexCodeRunner : public ICodexRunner, public FRunnable
{
public:
	FCodexCodeRunner();
	virtual ~FCodexCodeRunner();

	// ICodexRunner interface
	virtual bool ExecuteAsync(
		const FCodexRequestConfig& Config,
		FOnCodexResponse OnComplete,
		FOnCodexProgress OnProgress = FOnCodexProgress()
	) override;

	virtual bool ExecuteSync(const FCodexRequestConfig& Config, FString& OutResponse) override;
	virtual void Cancel() override;
	virtual bool IsExecuting() const override { return bIsExecuting; }
	virtual bool IsAvailable() const override { return IsCodexAvailable(); }

	/** Check if the Codex CLI is available on this system */
	static bool IsCodexAvailable();

	/** Get the runtime CLI path (Codex-first) */
	static FString GetCodexPath();

	// FRunnable interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** Build stream-json NDJSON payload with text + base64 image content blocks */
	FString BuildStreamJsonPayload(const FString& TextPrompt, const TArray<FString>& ImagePaths);

	/** Parse stream-json NDJSON output to extract the response text */
	FString ParseStreamJsonOutput(const FString& RawOutput);

private:
	FString BuildCommandLine(const FCodexRequestConfig& Config);
	void ExecuteProcess();
	void CleanupHandles();

	/** Parse a single NDJSON line and emit structured events */
	void ParseAndEmitNdjsonLine(const FString& JsonLine);

	/** Buffer for accumulating incomplete NDJSON lines across read chunks */
	FString NdjsonLineBuffer;

	/** Accumulated text from assistant messages for the final response */
	FString AccumulatedResponseText;

	/** Create pipes for process stdout/stderr capture */
	bool CreateProcessPipes();

	/** Launch the Codex process with given command */
	bool LaunchProcess(const FString& FullCommand, const FString& WorkingDir);

	/** Read output from process until completion or cancellation */
	FString ReadProcessOutput();

	/** Report error to callback on game thread */
	void ReportError(const FString& ErrorMessage);

	/** Report completion to callback on game thread */
	void ReportCompletion(const FString& Output, bool bSuccess);

	FCodexRequestConfig CurrentConfig;
	FOnCodexResponse OnCompleteDelegate;
	FOnCodexProgress OnProgressDelegate;

	FRunnableThread* Thread;
	FThreadSafeCounter StopTaskCounter;
	TAtomic<bool> bIsExecuting;

	// Process handle (FProcHandle stored as void* for atomic exchange compatibility)
	FProcHandle ProcessHandle;

	// Pipe handles (UE cross-platform pipe handles)
	void* ReadPipe;
	void* WritePipe;
	void* StdInReadPipe;
	void* StdInWritePipe;

	// Temp file paths for prompts (to avoid command line length limits)
	FString SystemPromptFilePath;
	FString PromptFilePath;
};
