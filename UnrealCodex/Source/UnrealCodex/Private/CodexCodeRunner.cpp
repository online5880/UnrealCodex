// Copyright Natali Caggiano. All Rights Reserved.

#include "CodexCodeRunner.h"
#include "UnrealCodexModule.h"
#include "UnrealCodexConstants.h"
#include "ProjectContext.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
#include "Async/Async.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"

FCodexCodeRunner::FCodexCodeRunner()
	: Thread(nullptr)
	, bIsExecuting(false)
	, ReadPipe(nullptr)
	, WritePipe(nullptr)
	, StdInReadPipe(nullptr)
	, StdInWritePipe(nullptr)
{
}

FCodexCodeRunner::~FCodexCodeRunner()
{
	// Signal stop FIRST (thread-safe) before touching anything
	StopTaskCounter.Set(1);

	// Wait for thread to exit BEFORE touching handles
	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	// NOW safe to cleanup handles (thread has exited)
	CleanupHandles();
}

void FCodexCodeRunner::CleanupHandles()
{
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
	}
	if (StdInReadPipe || StdInWritePipe)
	{
		FPlatformProcess::ClosePipe(StdInReadPipe, StdInWritePipe);
		StdInReadPipe = nullptr;
		StdInWritePipe = nullptr;
	}
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::CloseProc(ProcessHandle);
	}
}

bool FCodexCodeRunner::IsCodexAvailable()
{
	FString CodexPath = GetCodexPath();
	return !CodexPath.IsEmpty();
}

FString FCodexCodeRunner::GetCodexPath()
{
	// Cache the path to avoid repeated lookups and log spam
	static FString CachedCodexPath;
	static bool bHasSearched = false;

	if (bHasSearched && !CachedCodexPath.IsEmpty())
	{
		// Only return cached path if it's valid
		return CachedCodexPath;
	}
	// Allow re-search if previous search failed (CachedCodexPath is empty)
	bHasSearched = true;

	// Check common locations for the Codex CLI.
	TArray<FString> PossiblePaths;

#if PLATFORM_WINDOWS
	// User profile .local/bin (Codex binary install location)
	FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT(".local"), TEXT("bin"), TEXT("codex.exe")));
	}

	// npm global install location
	FString AppData = FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
	if (!AppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(AppData, TEXT("npm"), TEXT("codex.cmd")));
	}

	// Local AppData npm
	FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
	if (!LocalAppData.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(LocalAppData, TEXT("npm"), TEXT("codex.cmd")));
	}

	// User profile npm
	if (!UserProfile.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT("AppData"), TEXT("Roaming"), TEXT("npm"), TEXT("codex.cmd")));
		PossiblePaths.Add(FPaths::Combine(UserProfile, TEXT("AppData"), TEXT("Roaming"), TEXT("npm"), TEXT("codex.cmd")));
	}

	// Check PATH - try to find codex/codex variants
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(";"), true);

	for (const FString& Dir : PathDirs)
	{
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("codex.cmd")));
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("codex.exe")));
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("codex.cmd")));
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("codex.exe")));
	}
#else
	// Linux/Mac common user-local install locations
	FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
	if (!Home.IsEmpty())
	{
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("bin"), TEXT("codex")));
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".local"), TEXT("bin"), TEXT("codex")));
	}

	// Common system paths
	PossiblePaths.Add(TEXT("/usr/local/bin/codex"));
	PossiblePaths.Add(TEXT("/usr/bin/codex"));
	PossiblePaths.Add(TEXT("/usr/local/bin/codex"));
	PossiblePaths.Add(TEXT("/usr/bin/codex"));

	// npm global install locations
	if (!Home.IsEmpty())
	{
		// npm default global prefix on Linux
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".npm-global"), TEXT("bin"), TEXT("codex")));
		PossiblePaths.Add(FPaths::Combine(Home, TEXT(".npm-global"), TEXT("bin"), TEXT("codex")));
	}

	// Check PATH
	FString PathEnv = FPlatformMisc::GetEnvironmentVariable(TEXT("PATH"));
	TArray<FString> PathDirs;
	PathEnv.ParseIntoArray(PathDirs, TEXT(":"), true);

	for (const FString& Dir : PathDirs)
	{
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("codex")));
		PossiblePaths.Add(FPaths::Combine(Dir, TEXT("codex")));
	}
#endif

	// Check each path
	for (const FString& Path : PossiblePaths)
	{
		if (IFileManager::Get().FileExists(*Path))
		{
			const FString BinaryName = FPaths::GetCleanFilename(Path).ToLower();
			if (BinaryName.Contains(TEXT("codex")))
			{
				UE_LOG(LogUnrealCodex, Log, TEXT("Found Codex CLI at: %s"), *Path);
				CachedCodexPath = Path;
				return CachedCodexPath;
			}
		}
	}

	// Try using 'where' (Windows) or 'which' (Linux/Mac) for Codex fallback.
	FString WhereOutput;
	FString WhereErrors;
	int32 ReturnCode;

#if PLATFORM_WINDOWS
	if (FPlatformProcess::ExecProcess(TEXT("where"), TEXT("codex"), &ReturnCode, &WhereOutput, &WhereErrors) && ReturnCode == 0)
	{
		WhereOutput.TrimStartAndEndInline();
		TArray<FString> Lines;
		WhereOutput.ParseIntoArrayLines(Lines);
		if (Lines.Num() > 0)
		{
			UE_LOG(LogUnrealCodex, Log, TEXT("Found Codex CLI via where: %s"), *Lines[0]);
			CachedCodexPath = Lines[0];
			return CachedCodexPath;
		}
	}
#else
	if (FPlatformProcess::ExecProcess(TEXT("/bin/sh"), TEXT("-c 'which codex 2>/dev/null'"), &ReturnCode, &WhereOutput, &WhereErrors) && ReturnCode == 0)
	{
		WhereOutput.TrimStartAndEndInline();
		TArray<FString> Lines;
		WhereOutput.ParseIntoArrayLines(Lines);
		if (Lines.Num() > 0)
		{
			UE_LOG(LogUnrealCodex, Log, TEXT("Found Codex CLI via which: %s"), *Lines[0]);
			CachedCodexPath = Lines[0];
			return CachedCodexPath;
		}
	}
#endif

	UE_LOG(LogUnrealCodex, Warning, TEXT("Codex CLI not found. Install with: npm i -g @openai/codex"));

	// CachedCodexPath remains empty if not found
	return CachedCodexPath;
}

bool FCodexCodeRunner::ExecuteAsync(
	const FCodexRequestConfig& Config,
	FOnCodexResponse OnComplete,
	FOnCodexProgress OnProgress)
{
	// Use atomic compare-exchange for thread-safe check-and-set
	bool Expected = false;
	if (!bIsExecuting.CompareExchange(Expected, true))
	{
		UE_LOG(LogUnrealCodex, Warning, TEXT("Runtime is already executing a request"));
		return false;
	}

	if (!IsCodexAvailable())
	{
		bIsExecuting = false;
		OnComplete.ExecuteIfBound(TEXT("Codex CLI not found. Install with: npm i -g @openai/codex"), false);
		return false;
	}

	// Clean up old thread if exists (from previous completed execution)
	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	CurrentConfig = Config;
	OnCompleteDelegate = OnComplete;
	OnProgressDelegate = OnProgress;

	// Start the execution thread
	Thread = FRunnableThread::Create(this, TEXT("CodexCodeRunner"), 0, TPri_Normal);

	if (!Thread)
	{
		bIsExecuting = false;
		return false;
	}
	return true;
}

bool FCodexCodeRunner::ExecuteSync(const FCodexRequestConfig& Config, FString& OutResponse)
{
	if (!IsCodexAvailable())
	{
		OutResponse = TEXT("Codex CLI not found. Install with: npm i -g @openai/codex");
		return false;
	}

	FString CodexPath = GetCodexPath();
	FString CommandLine = BuildCommandLine(Config);

	UE_LOG(LogUnrealCodex, Log, TEXT("Executing Codex runtime: %s %s"), *CodexPath, *CommandLine);

	FString StdOut;
	FString StdErr;
	int32 ReturnCode;

	// Set working directory
	FString WorkingDir = Config.WorkingDirectory;
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = FPaths::ProjectDir();
	}

	bool bSuccess = FPlatformProcess::ExecProcess(
		*CodexPath,
		*CommandLine,
		&ReturnCode,
		&StdOut,
		&StdErr,
		*WorkingDir
	);

	if (bSuccess && ReturnCode == 0)
	{
		OutResponse = StdOut;
		return true;
	}
	else
	{
		OutResponse = StdErr.IsEmpty() ? StdOut : StdErr;
		UE_LOG(LogUnrealCodex, Error, TEXT("Codex execution failed: %s"), *OutResponse);
		return false;
	}
}

// Get the plugin directory path
static FString GetPluginDirectory()
{
	// Try engine plugins directly (preferred new name)
	FString EnginePluginPathCodex = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("UnrealCodex"));
	if (FPaths::DirectoryExists(EnginePluginPathCodex))
	{
		return EnginePluginPathCodex;
	}

	// Try engine plugins directly (manual install location)
	FString EnginePluginPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("UnrealCodex"));
	if (FPaths::DirectoryExists(EnginePluginPath))
	{
		return EnginePluginPath;
	}

	// Try engine Marketplace plugins with preferred new name
	FString MarketplacePluginPathCodex = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Marketplace"), TEXT("UnrealCodex"));
	if (FPaths::DirectoryExists(MarketplacePluginPathCodex))
	{
		return MarketplacePluginPathCodex;
	}

	// Try engine Marketplace plugins (Epic marketplace location)
	FString MarketplacePluginPath = FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("Marketplace"), TEXT("UnrealCodex"));
	if (FPaths::DirectoryExists(MarketplacePluginPath))
	{
		return MarketplacePluginPath;
	}

	// Try project plugins
	FString ProjectPluginPath = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealCodex"));
	if (FPaths::DirectoryExists(ProjectPluginPath))
	{
		return ProjectPluginPath;
	}

	FString ProjectPluginPathCodex = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("UnrealCodex"));
	if (FPaths::DirectoryExists(ProjectPluginPathCodex))
	{
		return ProjectPluginPathCodex;
	}

	UE_LOG(LogUnrealCodex, Warning, TEXT("Could not find plugin directory. Checked: %s, %s, %s, %s, %s, %s"),
		*EnginePluginPathCodex, *EnginePluginPath, *MarketplacePluginPathCodex, *MarketplacePluginPath, *ProjectPluginPathCodex, *ProjectPluginPath);
	return FString();
}

FString FCodexCodeRunner::BuildCommandLine(const FCodexRequestConfig& Config)
{
	FString CommandLine;

	// Codex non-interactive mode
	CommandLine += TEXT("exec --json --skip-git-repo-check ");

	// Skip permissions if requested
	if (Config.bSkipPermissions)
	{
		CommandLine += TEXT("--dangerously-bypass-approvals-and-sandbox ");
	}

	// MCP config for editor tools
	FString PluginDir = GetPluginDirectory();
	if (!PluginDir.IsEmpty())
	{
		FString MCPBridgePath = FPaths::Combine(PluginDir, TEXT("Resources"), TEXT("mcp-bridge"), TEXT("index.js"));
		FPaths::NormalizeFilename(MCPBridgePath);
		MCPBridgePath = FPaths::ConvertRelativePathToFull(MCPBridgePath);

		if (FPaths::FileExists(MCPBridgePath))
		{
			const FString NormalizedBridgePath = MCPBridgePath.Replace(TEXT("\\"), TEXT("/"));
			CommandLine += TEXT("-c \"mcp_servers.unrealcodex.command=\\\"node\\\"\" ");
			CommandLine += FString::Printf(TEXT("-c \"mcp_servers.unrealcodex.args=[\\\"%s\\\"]\" "), *NormalizedBridgePath);
			CommandLine += FString::Printf(TEXT("-c \"mcp_servers.unrealcodex.env={UNREAL_MCP_URL=\\\"http://localhost:%d\\\"}\" "), UnrealCodexConstants::MCPServer::DefaultPort);
			UE_LOG(LogUnrealCodex, Log, TEXT("Injected Codex MCP server config for bridge: %s"), *NormalizedBridgePath);
		}
		else
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("MCP bridge not found at: %s"), *MCPBridgePath);
		}
	}

	// Image attachments for Codex CLI
	if (Config.AttachedImagePaths.Num() > 0)
	{
		TArray<FString> ExistingImages;
		for (const FString& ImagePath : Config.AttachedImagePaths)
		{
			if (ImagePath.IsEmpty())
			{
				continue;
			}

			const FString FullImagePath = FPaths::ConvertRelativePathToFull(ImagePath);
			if (FPaths::FileExists(FullImagePath))
			{
				ExistingImages.Add(FullImagePath.Replace(TEXT("\\"), TEXT("/")));
			}
		}

		if (ExistingImages.Num() > 0)
		{
			CommandLine += FString::Printf(TEXT("--image \"%s\" "), *FString::Join(ExistingImages, TEXT(",")));
		}
	}

	// Write prompts to files to avoid command line length limits (Error 206)
	FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealCodex"));
	IFileManager::Get().MakeDirectory(*TempDir, true);

	// System prompt - write to file
	if (!Config.SystemPrompt.IsEmpty())
	{
		FString SystemPromptPath = FPaths::Combine(TempDir, TEXT("system-prompt.txt"));
		if (FFileHelper::SaveStringToFile(Config.SystemPrompt, *SystemPromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			SystemPromptFilePath = SystemPromptPath;
			UE_LOG(LogUnrealCodex, Log, TEXT("System prompt written to: %s (%d chars)"), *SystemPromptPath, Config.SystemPrompt.Len());
		}
	}

	// User prompt - write to file
	FString PromptPath = FPaths::Combine(TempDir, TEXT("prompt.txt"));
	if (FFileHelper::SaveStringToFile(Config.Prompt, *PromptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		PromptFilePath = PromptPath;
		UE_LOG(LogUnrealCodex, Log, TEXT("Prompt written to: %s (%d chars)"), *PromptPath, Config.Prompt.Len());
	}

	// Prompt will be piped through stdin when using '-' placeholder.
	CommandLine += TEXT("- ");

	return CommandLine;
}

FString FCodexCodeRunner::BuildStreamJsonPayload(const FString& TextPrompt, const TArray<FString>& ImagePaths)
{
	using namespace UnrealCodexConstants::ClipboardImage;

	// Pre-compute expected directory once for all images
	FString ExpectedDir = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealCodex"), TEXT("screenshots")));

	// Build content blocks array
	TArray<TSharedPtr<FJsonValue>> ContentArray;

	// Text content block (system context + user message)
	if (!TextPrompt.IsEmpty())
	{
		TSharedPtr<FJsonObject> TextBlock = MakeShared<FJsonObject>();
		TextBlock->SetStringField(TEXT("type"), TEXT("text"));
		TextBlock->SetStringField(TEXT("text"), TextPrompt);
		ContentArray.Add(MakeShared<FJsonValueObject>(TextBlock));
	}

	// Image content blocks (base64-encoded PNGs)
	int32 EncodedCount = 0;
	int64 TotalImageBytes = 0;
	const int32 MaxCount = FMath::Min(ImagePaths.Num(), MaxImagesPerMessage);

	for (int32 i = 0; i < MaxCount; ++i)
	{
		const FString& ImagePath = ImagePaths[i];
		if (ImagePath.IsEmpty())
		{
			continue;
		}

		FString FullImagePath = FPaths::ConvertRelativePathToFull(ImagePath);

		if (FullImagePath.Contains(TEXT("..")))
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("Rejecting image path with traversal: %s"), *FullImagePath);
			continue;
		}
		if (!FullImagePath.StartsWith(ExpectedDir))
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("Rejecting image path outside screenshots directory: %s"), *FullImagePath);
			continue;
		}
		if (!FPaths::FileExists(FullImagePath))
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("Attached image file no longer exists: %s"), *FullImagePath);
			continue;
		}

		// Check per-file size
		const int64 FileSize = IFileManager::Get().FileSize(*FullImagePath);
		if (FileSize > MaxImageFileSize)
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("Image file too large for base64 encoding: %s (%lld bytes, max %lld)"),
				*FullImagePath, FileSize, MaxImageFileSize);
			continue;
		}

		// Check total payload size
		if (TotalImageBytes + FileSize > MaxTotalImagePayloadSize)
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("Skipping image (total payload would exceed %lld bytes): %s"),
				MaxTotalImagePayloadSize, *FullImagePath);
			continue;
		}

		// Load and base64 encode the PNG
		TArray<uint8> ImageData;
		if (!FFileHelper::LoadFileToArray(ImageData, *FullImagePath))
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("Failed to load image file for base64 encoding: %s"), *FullImagePath);
			continue;
		}

		FString Base64ImageData = FBase64::Encode(ImageData);
		TotalImageBytes += FileSize;

		TSharedPtr<FJsonObject> Source = MakeShared<FJsonObject>();
		Source->SetStringField(TEXT("type"), TEXT("base64"));
		Source->SetStringField(TEXT("media_type"), TEXT("image/png"));
		Source->SetStringField(TEXT("data"), Base64ImageData);

		TSharedPtr<FJsonObject> ImageBlock = MakeShared<FJsonObject>();
		ImageBlock->SetStringField(TEXT("type"), TEXT("image"));
		ImageBlock->SetObjectField(TEXT("source"), Source);
		ContentArray.Add(MakeShared<FJsonValueObject>(ImageBlock));

		EncodedCount++;
		UE_LOG(LogUnrealCodex, Log, TEXT("Base64 encoded image [%d]: %s (%d bytes -> %d chars)"),
			EncodedCount, *FullImagePath, ImageData.Num(), Base64ImageData.Len());
	}

	if (EncodedCount > 0)
	{
		UE_LOG(LogUnrealCodex, Log, TEXT("Encoded %d image(s), total %lld bytes"), EncodedCount, TotalImageBytes);
	}

	// Build the inner message object: {"role":"user","content":[...]}
	TSharedPtr<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(TEXT("role"), TEXT("user"));
	Message->SetArrayField(TEXT("content"), ContentArray);

	// Build the outer SDKUserMessage envelope: {"type":"user","message":{...}}
	TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
	Envelope->SetStringField(TEXT("type"), TEXT("user"));
	Envelope->SetObjectField(TEXT("message"), Message);

	// Serialize to condensed JSON (single line for NDJSON)
	FString JsonLine;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonLine);
	FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);
	Writer->Close();

	// NDJSON requires newline termination
	JsonLine += TEXT("\n");

	UE_LOG(LogUnrealCodex, Log, TEXT("Built stream-json payload: %d chars (images: %d)"),
		JsonLine.Len(), EncodedCount);

	return JsonLine;
}

FString FCodexCodeRunner::ParseStreamJsonOutput(const FString& RawOutput)
{
	// Parse Codex JSONL first, then fall back to legacy Codex stream-json output.

	TArray<FString> Lines;
	RawOutput.ParseIntoArrayLines(Lines);

	// First pass: parse Codex item.completed events with agent_message payload.
	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
		{
			continue;
		}

		FString Type;
		if (!JsonObj->TryGetStringField(TEXT("type"), Type))
		{
			continue;
		}

		if (Type == TEXT("item.completed"))
		{
			const TSharedPtr<FJsonObject>* ItemObj;
			if (JsonObj->TryGetObjectField(TEXT("item"), ItemObj))
			{
				FString ItemType;
				(*ItemObj)->TryGetStringField(TEXT("type"), ItemType);
				if (ItemType == TEXT("agent_message"))
				{
					FString MessageText;
					if ((*ItemObj)->TryGetStringField(TEXT("text"), MessageText) && !MessageText.IsEmpty())
					{
						return MessageText;
					}
				}
			}
		}

		if (Type == TEXT("result"))
		{
			FString ResultText;
			if (JsonObj->TryGetStringField(TEXT("result"), ResultText))
			{
				UE_LOG(LogUnrealCodex, Log, TEXT("Parsed stream-json result: %d chars"), ResultText.Len());
				return ResultText;
			}
		}
	}

	// Fallback: accumulate text from assistant content blocks
	FString AccumulatedText;
	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
		{
			continue;
		}

		FString Type;
		if (!JsonObj->TryGetStringField(TEXT("type"), Type))
		{
			continue;
		}

		if (Type == TEXT("assistant"))
		{
			const TSharedPtr<FJsonObject>* MessageObj;
			if (JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
			{
				const TArray<TSharedPtr<FJsonValue>>* ContentArray;
				if ((*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
				{
					for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
					{
						const TSharedPtr<FJsonObject>* ContentObj;
						if (ContentValue->TryGetObject(ContentObj))
						{
							FString ContentType;
							if ((*ContentObj)->TryGetStringField(TEXT("type"), ContentType) && ContentType == TEXT("text"))
							{
								FString Text;
								if ((*ContentObj)->TryGetStringField(TEXT("text"), Text))
								{
									AccumulatedText += Text;
								}
							}
						}
					}
				}
			}
		}
	}

	if (!AccumulatedText.IsEmpty())
	{
		UE_LOG(LogUnrealCodex, Log, TEXT("Parsed stream-json from assistant blocks: %d chars"), AccumulatedText.Len());
		return AccumulatedText;
	}

	// Last resort: return raw output for debugging and forward-compatibility.
	UE_LOG(LogUnrealCodex, Warning, TEXT("Failed to parse stream-json output (%d chars). Raw output logged below:"), RawOutput.Len());
	UE_LOG(LogUnrealCodex, Warning, TEXT("%s"), *RawOutput.Left(2000));
	FString FallbackOutput = RawOutput;
	FallbackOutput.TrimStartAndEndInline();
	return FallbackOutput;
}

void FCodexCodeRunner::ParseAndEmitNdjsonLine(const FString& JsonLine)
{
	if (JsonLine.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonLine);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogUnrealCodex, Verbose, TEXT("NDJSON: Non-JSON line (skipping): %.200s"), *JsonLine);
		return;
	}

	FString Type;
	if (!JsonObj->TryGetStringField(TEXT("type"), Type))
	{
		UE_LOG(LogUnrealCodex, Verbose, TEXT("NDJSON: Line missing 'type' field"));
		return;
	}

	UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON Event: type=%s"), *Type);

	if (Type == TEXT("thread.started"))
	{
		FString ThreadId;
		JsonObj->TryGetStringField(TEXT("thread_id"), ThreadId);
		if (CurrentConfig.OnStreamEvent.IsBound())
		{
			FCodexStreamEvent Event;
			Event.Type = ECodexStreamEventType::SessionInit;
			Event.SessionId = ThreadId;
			Event.RawJson = JsonLine;
			FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
			AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
			{
				EventDelegate.ExecuteIfBound(Event);
			});
		}
		return;
	}

	if (Type == TEXT("item.completed"))
	{
		const TSharedPtr<FJsonObject>* ItemObj;
		if (!JsonObj->TryGetObjectField(TEXT("item"), ItemObj))
		{
			return;
		}

		FString ItemType;
		(*ItemObj)->TryGetStringField(TEXT("type"), ItemType);
		if (ItemType == TEXT("agent_message"))
		{
			FString Text;
			if ((*ItemObj)->TryGetStringField(TEXT("text"), Text) && !Text.IsEmpty())
			{
				AccumulatedResponseText += Text;
				if (OnProgressDelegate.IsBound())
				{
					FOnCodexProgress ProgressCopy = OnProgressDelegate;
					AsyncTask(ENamedThreads::GameThread, [ProgressCopy, Text]()
					{
						ProgressCopy.ExecuteIfBound(Text);
					});
				}

				if (CurrentConfig.OnStreamEvent.IsBound())
				{
					FCodexStreamEvent Event;
					Event.Type = ECodexStreamEventType::TextContent;
					Event.Text = Text;
					Event.RawJson = JsonLine;
					FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
					AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
					{
						EventDelegate.ExecuteIfBound(Event);
					});
				}
			}
		}
		return;
	}

	if (Type == TEXT("system"))
	{
		// Session init event
		FString Subtype;
		JsonObj->TryGetStringField(TEXT("subtype"), Subtype);
		FString SessionId;
		JsonObj->TryGetStringField(TEXT("session_id"), SessionId);

		UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON System: subtype=%s, session_id=%s"), *Subtype, *SessionId);

		if (CurrentConfig.OnStreamEvent.IsBound())
		{
			FCodexStreamEvent Event;
			Event.Type = ECodexStreamEventType::SessionInit;
			Event.SessionId = SessionId;
			Event.RawJson = JsonLine;
			FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
			AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
			{
				EventDelegate.ExecuteIfBound(Event);
			});
		}
	}
	else if (Type == TEXT("assistant"))
	{
		// Assistant message with content blocks
		const TSharedPtr<FJsonObject>* MessageObj;
		if (!JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("NDJSON: assistant message missing 'message' field"));
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* ContentArray;
		if (!(*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
		{
			UE_LOG(LogUnrealCodex, Warning, TEXT("NDJSON: assistant message.content missing"));
			return;
		}

		for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
		{
			const TSharedPtr<FJsonObject>* ContentObj;
			if (!ContentValue->TryGetObject(ContentObj))
			{
				continue;
			}

			FString ContentType;
			if (!(*ContentObj)->TryGetStringField(TEXT("type"), ContentType))
			{
				continue;
			}

			if (ContentType == TEXT("text"))
			{
				FString Text;
				if ((*ContentObj)->TryGetStringField(TEXT("text"), Text))
				{
					UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON TextContent: %d chars"), Text.Len());
					AccumulatedResponseText += Text;

					// Fire old progress delegate for backward compat
					if (OnProgressDelegate.IsBound())
					{
						FOnCodexProgress ProgressCopy = OnProgressDelegate;
						AsyncTask(ENamedThreads::GameThread, [ProgressCopy, Text]()
						{
							ProgressCopy.ExecuteIfBound(Text);
						});
					}

					// Fire new structured event
					if (CurrentConfig.OnStreamEvent.IsBound())
					{
						FCodexStreamEvent Event;
						Event.Type = ECodexStreamEventType::TextContent;
						Event.Text = Text;
						FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
						AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
						{
							EventDelegate.ExecuteIfBound(Event);
						});
					}
				}
			}
			else if (ContentType == TEXT("tool_use"))
			{
				FString ToolName, ToolId;
				(*ContentObj)->TryGetStringField(TEXT("name"), ToolName);
				(*ContentObj)->TryGetStringField(TEXT("id"), ToolId);

				// Serialize input to string
				FString ToolInputStr;
				const TSharedPtr<FJsonObject>* InputObj;
				if ((*ContentObj)->TryGetObjectField(TEXT("input"), InputObj))
				{
					TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
						TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ToolInputStr);
					FJsonSerializer::Serialize((*InputObj).ToSharedRef(), Writer);
					Writer->Close();
				}

				UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON ToolUse: name=%s, id=%s, input=%d chars"),
					*ToolName, *ToolId, ToolInputStr.Len());

				if (CurrentConfig.OnStreamEvent.IsBound())
				{
					FCodexStreamEvent Event;
					Event.Type = ECodexStreamEventType::ToolUse;
					Event.ToolName = ToolName;
					Event.ToolCallId = ToolId;
					Event.ToolInput = ToolInputStr;
					Event.RawJson = JsonLine;
					FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
					AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
					{
						EventDelegate.ExecuteIfBound(Event);
					});
				}
			}
			else
			{
				UE_LOG(LogUnrealCodex, Verbose, TEXT("NDJSON: unknown content block type: %s"), *ContentType);
			}
		}
	}
	else if (Type == TEXT("user"))
	{
		// Tool result message
		const TSharedPtr<FJsonObject>* MessageObj;
		if (!JsonObj->TryGetObjectField(TEXT("message"), MessageObj))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* ContentArray;
		if (!(*MessageObj)->TryGetArrayField(TEXT("content"), ContentArray))
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& ContentValue : *ContentArray)
		{
			const TSharedPtr<FJsonObject>* ContentObj;
			if (!ContentValue->TryGetObject(ContentObj))
			{
				continue;
			}

			FString ContentType;
			if (!(*ContentObj)->TryGetStringField(TEXT("type"), ContentType))
			{
				continue;
			}

			if (ContentType == TEXT("tool_result"))
			{
				FString ToolUseId, ResultContent;
				(*ContentObj)->TryGetStringField(TEXT("tool_use_id"), ToolUseId);

				// content can be a string OR an array of content blocks
				if (!(*ContentObj)->TryGetStringField(TEXT("content"), ResultContent))
				{
					// Extract text from content block array: [{"type":"text","text":"..."},...]
					const TArray<TSharedPtr<FJsonValue>>* ResultArray;
					if ((*ContentObj)->TryGetArrayField(TEXT("content"), ResultArray))
					{
						for (const TSharedPtr<FJsonValue>& Block : *ResultArray)
						{
							const TSharedPtr<FJsonObject>* BlockObj;
							if (Block->TryGetObject(BlockObj))
							{
								FString BlockType;
								(*BlockObj)->TryGetStringField(TEXT("type"), BlockType);
								if (BlockType == TEXT("text"))
								{
									FString BlockText;
									if ((*BlockObj)->TryGetStringField(TEXT("text"), BlockText))
									{
										if (!ResultContent.IsEmpty())
										{
											ResultContent += TEXT("\n");
										}
										ResultContent += BlockText;
									}
								}
							}
						}
					}
				}

				UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON ToolResult: tool_use_id=%s, content=%d chars"),
					*ToolUseId, ResultContent.Len());

				if (CurrentConfig.OnStreamEvent.IsBound())
				{
					FCodexStreamEvent Event;
					Event.Type = ECodexStreamEventType::ToolResult;
					Event.ToolCallId = ToolUseId;
					Event.ToolResultContent = ResultContent;
					Event.RawJson = JsonLine;
					FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
					AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
					{
						EventDelegate.ExecuteIfBound(Event);
					});
				}
			}
		}
	}
	else if (Type == TEXT("result"))
	{
		// Final result message with stats
		FString ResultText, Subtype;
		JsonObj->TryGetStringField(TEXT("result"), ResultText);
		JsonObj->TryGetStringField(TEXT("subtype"), Subtype);
		bool bIsError = false;
		JsonObj->TryGetBoolField(TEXT("is_error"), bIsError);
		double DurationMs = 0.0;
		JsonObj->TryGetNumberField(TEXT("duration_ms"), DurationMs);
		double NumTurns = 0.0;
		JsonObj->TryGetNumberField(TEXT("num_turns"), NumTurns);
		double TotalCostUsd = 0.0;
		JsonObj->TryGetNumberField(TEXT("total_cost_usd"), TotalCostUsd);

		UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON Result: subtype=%s, is_error=%d, duration=%.0fms, turns=%.0f, cost=$%.4f, result=%d chars"),
			*Subtype, bIsError, DurationMs, NumTurns, TotalCostUsd, ResultText.Len());

		if (CurrentConfig.OnStreamEvent.IsBound())
		{
			FCodexStreamEvent Event;
			Event.Type = ECodexStreamEventType::Result;
			Event.ResultText = ResultText;
			Event.bIsError = bIsError;
			Event.DurationMs = static_cast<int32>(DurationMs);
			Event.NumTurns = static_cast<int32>(NumTurns);
			Event.TotalCostUsd = static_cast<float>(TotalCostUsd);
			Event.RawJson = JsonLine;
			FOnCodexStreamEvent EventDelegate = CurrentConfig.OnStreamEvent;
			AsyncTask(ENamedThreads::GameThread, [EventDelegate, Event]()
			{
				EventDelegate.ExecuteIfBound(Event);
			});
		}
	}
	else
	{
		UE_LOG(LogUnrealCodex, Verbose, TEXT("NDJSON: unhandled message type: %s"), *Type);
	}
}

void FCodexCodeRunner::Cancel()
{
	// Signal stop first - ReadProcessOutput() checks this and will exit its loop
	StopTaskCounter.Set(1);

	// Terminate the process to unblock any pending pipe reads
	// Don't close pipes/handles here - ExecuteProcess() on the worker thread
	// will handle cleanup after ReadProcessOutput() returns
	if (ProcessHandle.IsValid())
	{
		FPlatformProcess::TerminateProc(ProcessHandle, true);
	}
}

bool FCodexCodeRunner::Init()
{
	// bIsExecuting is already set by ExecuteAsync (thread-safe)
	StopTaskCounter.Reset();
	NdjsonLineBuffer.Empty();
	AccumulatedResponseText.Empty();
	return true;
}

uint32 FCodexCodeRunner::Run()
{
	ExecuteProcess();
	return 0;
}

void FCodexCodeRunner::Stop()
{
	StopTaskCounter.Increment();
}

void FCodexCodeRunner::Exit()
{
	bIsExecuting = false;
}

bool FCodexCodeRunner::CreateProcessPipes()
{
	// Create stdout pipe (we read from ReadPipe, child writes to WritePipe)
	if (!FPlatformProcess::CreatePipe(ReadPipe, WritePipe, false))
	{
		UE_LOG(LogUnrealCodex, Error, TEXT("Failed to create stdout pipe"));
		return false;
	}

	// Create stdin pipe (child reads from StdInReadPipe, we write to StdInWritePipe)
	if (!FPlatformProcess::CreatePipe(StdInReadPipe, StdInWritePipe, true))
	{
		UE_LOG(LogUnrealCodex, Error, TEXT("Failed to create stdin pipe"));
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
		return false;
	}

	return true;
}

bool FCodexCodeRunner::LaunchProcess(const FString& FullCommand, const FString& WorkingDir)
{
	FString CodexPath = GetCodexPath();

	// FPlatformProcess::CreateProc takes the URL (executable) and Params separately
	FString Params = FullCommand;

	ProcessHandle = FPlatformProcess::CreateProc(
		*CodexPath,
		*Params,
		false,    // bLaunchDetached
		false,    // bLaunchHidden
		true,     // bLaunchReallyHidden
		nullptr,  // OutProcessID
		0,        // PriorityModifier
		*WorkingDir,
		WritePipe,    // PipeWriteChild - child's stdout goes here
		StdInReadPipe // PipeReadChild - child reads stdin from here
	);

	if (!ProcessHandle.IsValid())
	{
		UE_LOG(LogUnrealCodex, Error, TEXT("Failed to create Codex process"));
		UE_LOG(LogUnrealCodex, Error, TEXT("Runtime Path: %s"), *CodexPath);
		UE_LOG(LogUnrealCodex, Error, TEXT("Params: %s"), *Params);
		UE_LOG(LogUnrealCodex, Error, TEXT("Working directory: %s"), *WorkingDir);
		return false;
	}

	return true;
}

FString FCodexCodeRunner::ReadProcessOutput()
{
	FString FullOutput;

	// Reset NDJSON state for this request
	NdjsonLineBuffer.Empty();
	AccumulatedResponseText.Empty();

	while (!StopTaskCounter.GetValue())
	{
		// Read any available output from the pipe
		FString OutputChunk = FPlatformProcess::ReadPipe(ReadPipe);

		if (!OutputChunk.IsEmpty())
		{
			FullOutput += OutputChunk;

			// Parse NDJSON line-by-line: buffer chunks and split on newlines
			NdjsonLineBuffer += OutputChunk;

			int32 NewlineIdx;
			while (NdjsonLineBuffer.FindChar(TEXT('\n'), NewlineIdx))
			{
				FString CompleteLine = NdjsonLineBuffer.Left(NewlineIdx);
				CompleteLine.TrimEndInline();
				NdjsonLineBuffer.RightChopInline(NewlineIdx + 1);

				if (!CompleteLine.IsEmpty())
				{
					ParseAndEmitNdjsonLine(CompleteLine);
				}
			}
		}

		// Check if process has exited
		if (!FPlatformProcess::IsProcRunning(ProcessHandle))
		{
			// Process finished - read any remaining output
			FString RemainingOutput = FPlatformProcess::ReadPipe(ReadPipe);
			while (!RemainingOutput.IsEmpty())
			{
				FullOutput += RemainingOutput;
				NdjsonLineBuffer += RemainingOutput;
				RemainingOutput = FPlatformProcess::ReadPipe(ReadPipe);
			}

			// Parse all remaining buffered lines
			int32 FinalNewlineIdx;
			while (NdjsonLineBuffer.FindChar(TEXT('\n'), FinalNewlineIdx))
			{
				FString CompleteLine = NdjsonLineBuffer.Left(FinalNewlineIdx);
				CompleteLine.TrimEndInline();
				NdjsonLineBuffer.RightChopInline(FinalNewlineIdx + 1);

				if (!CompleteLine.IsEmpty())
				{
					ParseAndEmitNdjsonLine(CompleteLine);
				}
			}

			// Parse any final incomplete line (no trailing newline)
			NdjsonLineBuffer.TrimEndInline();
			if (!NdjsonLineBuffer.IsEmpty())
			{
				ParseAndEmitNdjsonLine(NdjsonLineBuffer);
				NdjsonLineBuffer.Empty();
			}

			break;
		}

		// Brief sleep to avoid busy-waiting
		FPlatformProcess::Sleep(0.01f);
	}

	return FullOutput;
}

void FCodexCodeRunner::ReportError(const FString& ErrorMessage)
{
	FOnCodexResponse CompleteCopy = OnCompleteDelegate;
	FString Message = ErrorMessage;
	AsyncTask(ENamedThreads::GameThread, [CompleteCopy, Message]()
	{
		CompleteCopy.ExecuteIfBound(Message, false);
	});
}

void FCodexCodeRunner::ReportCompletion(const FString& Output, bool bSuccess)
{
	FOnCodexResponse CompleteCopy = OnCompleteDelegate;
	FString FinalOutput = Output;
	AsyncTask(ENamedThreads::GameThread, [CompleteCopy, FinalOutput, bSuccess]()
	{
		CompleteCopy.ExecuteIfBound(FinalOutput, bSuccess);
	});
}

void FCodexCodeRunner::ExecuteProcess()
{
	FString CodexPath = GetCodexPath();

	// Verify the path exists
	if (CodexPath.IsEmpty())
	{
		ReportError(TEXT("Codex CLI not found. Install with: npm i -g @openai/codex"));
		return;
	}

	if (!IFileManager::Get().FileExists(*CodexPath))
	{
		UE_LOG(LogUnrealCodex, Error, TEXT("Runtime path no longer exists: %s"), *CodexPath);
		ReportError(FString::Printf(TEXT("Codex CLI path invalid: %s"), *CodexPath));
		return;
	}

	FString CommandLine = BuildCommandLine(CurrentConfig);

	UE_LOG(LogUnrealCodex, Log, TEXT("Async executing Codex runtime: %s %s"), *CodexPath, *CommandLine);

	// Set working directory - convert to absolute path since FPaths::ProjectDir()
	// returns a relative path on macOS that external processes can't resolve
	FString WorkingDir = CurrentConfig.WorkingDirectory;
	if (WorkingDir.IsEmpty())
	{
		WorkingDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	}

	// Create pipes for stdout capture
	if (!CreateProcessPipes())
	{
		ReportError(TEXT("Failed to create pipe for Codex process"));
		return;
	}

	UE_LOG(LogUnrealCodex, Log, TEXT("Full command: %s %s"), *CodexPath, *CommandLine);
	UE_LOG(LogUnrealCodex, Log, TEXT("Working directory: %s"), *WorkingDir);

	if (!LaunchProcess(CommandLine, WorkingDir))
	{
		CleanupHandles();

		FString ErrorMsg = FString::Printf(
			TEXT("Failed to start Codex process.\n\n")
			TEXT("Runtime Path: %s\n")
			TEXT("Working Dir: %s\n\n")
			TEXT("Command (truncated): %.200s..."),
			*CodexPath,
			*WorkingDir,
			*CommandLine
		);
		ReportError(ErrorMsg);
		return;
	}

	// Write combined prompt text to stdin (Codex exec uses '-' placeholder)
	if (StdInWritePipe)
	{
		// Build the text portion of the prompt (system context + user message)
		FString TextPrompt;
		if (!SystemPromptFilePath.IsEmpty())
		{
			FString SystemPromptContent;
			if (FFileHelper::LoadFileToString(SystemPromptContent, *SystemPromptFilePath))
			{
				TextPrompt = FString::Printf(TEXT("[CONTEXT]\n%s\n[/CONTEXT]\n\n"), *SystemPromptContent);
			}
		}
		if (!PromptFilePath.IsEmpty())
		{
			FString PromptContent;
			if (FFileHelper::LoadFileToString(PromptContent, *PromptFilePath))
			{
				TextPrompt += PromptContent;
			}
		}

		FString StdinPayload = TextPrompt;
		if (!StdinPayload.EndsWith(TEXT("\n")))
		{
			StdinPayload += TEXT("\n");
		}

		// Write to stdin
		if (!StdinPayload.IsEmpty())
		{
			FTCHARToUTF8 Utf8Payload(*StdinPayload);
			int32 BytesWritten = 0;
			bool bWritten = FPlatformProcess::WritePipe(StdInWritePipe, (const uint8*)Utf8Payload.Get(), Utf8Payload.Length(), &BytesWritten);
			UE_LOG(LogUnrealCodex, Log, TEXT("Wrote to Codex stdin (success=%d, %d/%d bytes, images: %d, system: %d chars, user: %d chars)"),
				bWritten, BytesWritten, Utf8Payload.Length(), CurrentConfig.AttachedImagePaths.Num(),
				CurrentConfig.SystemPrompt.Len(), CurrentConfig.Prompt.Len());
		}

		// Close stdin write pipe to signal EOF to Codex runtime
		// We close the entire stdin pipe pair since child has the read end
		FPlatformProcess::ClosePipe(StdInReadPipe, StdInWritePipe);
		StdInReadPipe = nullptr;
		StdInWritePipe = nullptr;
	}

	// Clear temp file paths
	SystemPromptFilePath.Empty();
	PromptFilePath.Empty();

	// Read output until process completes (NDJSON events parsed during reading)
	FString FullOutput = ReadProcessOutput();

	// Use accumulated response text from parsed NDJSON events
	// Falls back to legacy ParseStreamJsonOutput if no events were parsed
	FString ResponseText = AccumulatedResponseText;
	if (ResponseText.IsEmpty() && !FullOutput.IsEmpty())
	{
		// Fallback: try legacy parsing in case NDJSON format wasn't as expected
		ResponseText = ParseStreamJsonOutput(FullOutput);
		UE_LOG(LogUnrealCodex, Log, TEXT("NDJSON parser produced no text, fell back to legacy parser (%d chars)"),
			ResponseText.Len());
	}

	// Get exit code
	int32 ExitCode = 0;
	FPlatformProcess::GetProcReturnCode(ProcessHandle, &ExitCode);

	// Cleanup handles
	if (ReadPipe || WritePipe)
	{
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		ReadPipe = nullptr;
		WritePipe = nullptr;
	}
	FPlatformProcess::CloseProc(ProcessHandle);

	// Report completion with parsed response text
	bool bSuccess = (ExitCode == 0) && !StopTaskCounter.GetValue();
	ReportCompletion(ResponseText, bSuccess);
}

// FCodexCodeSubsystem is now in CodexSubsystem.cpp
