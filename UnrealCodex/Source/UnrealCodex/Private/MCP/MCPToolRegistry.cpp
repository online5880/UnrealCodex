// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPToolRegistry.h"
#include "MCPTaskQueue.h"
#include "UnrealCodexModule.h"
#include "UnrealCodexConstants.h"
#include "ScriptPermissionDialog.h"
#include "Containers/Ticker.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	/**
	 * Built-in hook that adds consistent pre/post execution logging.
	 */
	class FMCPLoggingHook final : public IMCPToolHook
	{
	public:
		virtual bool BeforeExecute(const FMCPToolInfo& ToolInfo, const TSharedRef<FJsonObject>& Params, FMCPToolResult& /*OutResult*/) override
		{
			UE_LOG(LogUnrealCodex, Verbose, TEXT("[MCP Hook] BeforeExecute: %s (paramCount=%d)"), *ToolInfo.Name, Params->Values.Num());
			return true;
		}

		virtual void AfterExecute(const FMCPToolInfo& ToolInfo, const TSharedRef<FJsonObject>& Params, FMCPToolResult& InOutResult) override
		{
			UE_LOG(
				LogUnrealCodex,
				Verbose,
				TEXT("[MCP Hook] AfterExecute: %s success=%s message=%s (paramCount=%d)"),
				*ToolInfo.Name,
				InOutResult.bSuccess ? TEXT("true") : TEXT("false"),
				*InOutResult.Message,
				Params->Values.Num()
			);
		}
	};

	/** Validates required parameters declared by each tool. */
	class FMCPValidationHook final : public IMCPToolHook
	{
	public:
		virtual bool BeforeExecute(const FMCPToolInfo& ToolInfo, const TSharedRef<FJsonObject>& Params, FMCPToolResult& OutResult) override
		{
			for (const FMCPToolParameter& ParamDef : ToolInfo.Parameters)
			{
				if (ParamDef.bRequired && !Params->HasField(ParamDef.Name))
				{
					OutResult = FMCPToolResult::Error(FString::Printf(TEXT("Missing required parameter '%s' for tool '%s'"), *ParamDef.Name, *ToolInfo.Name));
					return false;
				}
			}

			return true;
		}
	};

	/**
	 * Safety hook for destructive tools.
	 *
	 * Behavior:
	 * - If confirm/force/__approved is true, proceed immediately.
	 * - Otherwise show a modal approval dialog to the user.
	 * - Deny execution when user rejects or the dialog can't be shown.
	 */
	class FMCPSafetyHook final : public IMCPToolHook
	{
	public:
		virtual bool BeforeExecute(const FMCPToolInfo& ToolInfo, const TSharedRef<FJsonObject>& Params, FMCPToolResult& OutResult) override
		{
			if (!ToolInfo.Annotations.bDestructiveHint)
			{
				return true;
			}

			const bool bApprovedByParam =
				GetBoolParam(Params, TEXT("confirm")) ||
				GetBoolParam(Params, TEXT("force")) ||
				GetBoolParam(Params, TEXT("__approved"));

			if (bApprovedByParam)
			{
				return true;
			}

			const bool bApprovedByUser = RequestUserApproval(ToolInfo, Params);
			if (!bApprovedByUser)
			{
				OutResult = FMCPToolResult::Error(FString::Printf(
					TEXT("Tool '%s' execution denied. Approval required for destructive operations."),
					*ToolInfo.Name
				));
				return false;
			}

			return true;
		}

	private:
		static bool GetBoolParam(const TSharedRef<FJsonObject>& Params, const FString& ParamName)
		{
			bool bValue = false;
			return Params->TryGetBoolField(ParamName, bValue) && bValue;
		}

		static FString BuildPreview(const FMCPToolInfo& ToolInfo, const TSharedRef<FJsonObject>& Params)
		{
			FString ParamsJson;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
			FJsonSerializer::Serialize(Params, Writer);

			return FString::Printf(
				TEXT("Tool: %s\n\nDescription:\n%s\n\nParameters:\n%s"),
				*ToolInfo.Name,
				*ToolInfo.Description,
				*ParamsJson
			);
		}

		static bool RequestUserApproval(const FMCPToolInfo& ToolInfo, const TSharedRef<FJsonObject>& Params)
		{
			const FString Preview = BuildPreview(ToolInfo, Params);
			const FString Description = FString::Printf(TEXT("Destructive MCP tool '%s' requested."), *ToolInfo.Name);

			if (IsInGameThread())
			{
				return FScriptPermissionDialog::Show(Preview, EScriptType::EditorUtility, Description);
			}

			// Non-game thread: dispatch to game thread and wait.
			TSharedPtr<bool, ESPMode::ThreadSafe> bApproved = MakeShared<bool, ESPMode::ThreadSafe>(false);
			TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> bCompleted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
			TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = MakeShareable(
				FPlatformProcess::GetSynchEventFromPool(),
				[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); }
			);

			FTSTicker::GetCoreTicker().AddTicker(TEXT("MCPTool_ApprovalDialog"), 0.0f,
				[bApproved, bCompleted, CompletionEvent, Preview, Description](float) -> bool
				{
					*bApproved = FScriptPermissionDialog::Show(Preview, EScriptType::EditorUtility, Description);
					*bCompleted = true;
					CompletionEvent->Trigger();
					return false;
				}
			);

			const uint32 TimeoutMs = 60000; // 60s for user decision
			const bool bSignaled = CompletionEvent->Wait(TimeoutMs);
			if (!bSignaled || !(*bCompleted))
			{
				UE_LOG(LogUnrealCodex, Warning, TEXT("[MCP Safety] Approval dialog timeout for tool '%s'"), *ToolInfo.Name);
				return false;
			}

			return *bApproved;
		}
	};
}

// Include all tool implementations
#include "Tools/MCPTool_SpawnActor.h"
#include "Tools/MCPTool_GetLevelActors.h"
#include "Tools/MCPTool_SetProperty.h"
#include "Tools/MCPTool_RunConsoleCommand.h"
#include "Tools/MCPTool_DeleteActors.h"
#include "Tools/MCPTool_MoveActor.h"
#include "Tools/MCPTool_GetOutputLog.h"
#include "Tools/MCPTool_ExecuteScript.h"
#include "Tools/MCPTool_CleanupScripts.h"
#include "Tools/MCPTool_GetScriptHistory.h"
#include "Tools/MCPTool_CaptureViewport.h"
#include "Tools/MCPTool_BlueprintQuery.h"
#include "Tools/MCPTool_BlueprintModify.h"
#include "Tools/MCPTool_AnimBlueprintModify.h"
#include "Tools/MCPTool_AssetSearch.h"
#include "Tools/MCPTool_AssetDependencies.h"
#include "Tools/MCPTool_AssetReferencers.h"
#include "Tools/MCPTool_EnhancedInput.h"
#include "Tools/MCPTool_Character.h"
#include "Tools/MCPTool_CharacterData.h"
#include "Tools/MCPTool_Material.h"
#include "Tools/MCPTool_Asset.h"
#include "Tools/MCPTool_OpenLevel.h"
#include "Tools/MCPTool_ProjectContext.h"
#include "Tools/MCPTool_ProjectInspect.h"
#include "Tools/MCPTool_PCGGetInfo.h"
#include "Tools/MCPTool_PCGAddNode.h"
#include "Tools/MCPTool_PCGConnectNodes.h"
#include "Tools/MCPTool_PCGCreateGraph.h"
#include "Tools/MCPTool_PhysicsGetInfo.h"
#include "Tools/MCPTool_PhysicsCreateAsset.h"
#include "Tools/MCPTool_PhysicsCreateMaterial.h"
#include "Tools/MCPTool_PhysicsSetProfile.h"
#include "Tools/MCPTool_PhysicsSetConstraint.h"
#include "Tools/MCPTool_SourceControl.h"
#include "Tools/MCPTool_SourceControlOps.h"
#include "Tools/MCPTool_SourceControlUmaAliases.h"
#include "Tools/MCPTool_SequencerTextureUma.h"

// Task queue tools
#include "Tools/MCPTool_TaskSubmit.h"
#include "Tools/MCPTool_TaskStatus.h"
#include "Tools/MCPTool_TaskResult.h"
#include "Tools/MCPTool_TaskList.h"
#include "Tools/MCPTool_TaskCancel.h"

FMCPToolRegistry::FMCPToolRegistry()
{
	HookManager = MakeShared<FMCPToolHookManager>();
	HookManager->RegisterHook(MakeShared<FMCPLoggingHook>());
	HookManager->RegisterHook(MakeShared<FMCPValidationHook>());
	HookManager->RegisterHook(MakeShared<FMCPSafetyHook>());

	RegisterBuiltinTools();
}

FMCPToolRegistry::~FMCPToolRegistry()
{
	StopTaskQueue();
	Tools.Empty();
}

void FMCPToolRegistry::StartTaskQueue()
{
	if (TaskQueue.IsValid())
	{
		TaskQueue->Start();
	}
}

void FMCPToolRegistry::StopTaskQueue()
{
	if (TaskQueue.IsValid())
	{
		TaskQueue->Shutdown();
	}
}

void FMCPToolRegistry::RegisterBuiltinTools()
{
	UE_LOG(LogUnrealCodex, Log, TEXT("Registering MCP tools..."));

	// Register all built-in tools
	RegisterTool(MakeShared<FMCPTool_SpawnActor>());
	RegisterTool(MakeShared<FMCPTool_GetLevelActors>());
	RegisterTool(MakeShared<FMCPTool_SetProperty>());
	RegisterTool(MakeShared<FMCPTool_RunConsoleCommand>());
	RegisterTool(MakeShared<FMCPTool_DeleteActors>());
	RegisterTool(MakeShared<FMCPTool_MoveActor>());
	RegisterTool(MakeShared<FMCPTool_GetOutputLog>());

	// Script execution tools
	RegisterTool(MakeShared<FMCPTool_ExecuteScript>());
	RegisterTool(MakeShared<FMCPTool_CleanupScripts>());
	RegisterTool(MakeShared<FMCPTool_GetScriptHistory>());

	// Viewport capture
	RegisterTool(MakeShared<FMCPTool_CaptureViewport>());

	// Blueprint tools
	RegisterTool(MakeShared<FMCPTool_BlueprintQuery>());
	RegisterTool(MakeShared<FMCPTool_BlueprintModify>());
	RegisterTool(MakeShared<FMCPTool_AnimBlueprintModify>());

	// Asset tools
	RegisterTool(MakeShared<FMCPTool_AssetSearch>());
	RegisterTool(MakeShared<FMCPTool_AssetDependencies>());
	RegisterTool(MakeShared<FMCPTool_AssetReferencers>());

	// Enhanced Input tools
	RegisterTool(MakeShared<FMCPTool_EnhancedInput>());

	// Character tools
	RegisterTool(MakeShared<FMCPTool_Character>());
	RegisterTool(MakeShared<FMCPTool_CharacterData>());

	// Material and Asset tools
	RegisterTool(MakeShared<FMCPTool_Material>());
	RegisterTool(MakeShared<FMCPTool_Asset>());

	// Level management tools
	RegisterTool(MakeShared<FMCPTool_OpenLevel>());

	// Project context tools
	RegisterTool(MakeShared<FMCPTool_ProjectContext>());
	RegisterTool(MakeShared<FMCPTool_ProjectInspect>());

	// PCG / Physics tools
	RegisterTool(MakeShared<FMCPTool_PCGGetInfo>());
	RegisterTool(MakeShared<FMCPTool_PCGAddNode>());
	RegisterTool(MakeShared<FMCPTool_PCGConnectNodes>());
	RegisterTool(MakeShared<FMCPTool_PCGCreateGraph>());
	RegisterTool(MakeShared<FMCPTool_PhysicsGetInfo>());
	RegisterTool(MakeShared<FMCPTool_PhysicsCreateAsset>());
	RegisterTool(MakeShared<FMCPTool_PhysicsCreateMaterial>());
	RegisterTool(MakeShared<FMCPTool_PhysicsSetProfile>());
	RegisterTool(MakeShared<FMCPTool_PhysicsSetConstraint>());

	// Source control tools
	RegisterTool(MakeShared<FMCPTool_SourceControl>());
	RegisterTool(MakeShared<FMCPTool_SourceControlStatus>());
	RegisterTool(MakeShared<FMCPTool_SourceControlCheckout>());
	RegisterTool(MakeShared<FMCPTool_SourceControlDiff>());
	RegisterTool(MakeShared<FMCPTool_SourcecontrolStatus>());
	RegisterTool(MakeShared<FMCPTool_SourcecontrolCheckout>());
	RegisterTool(MakeShared<FMCPTool_SourcecontrolDiff>());

	// UMA compatibility wrappers: sequencer/texture tools
	RegisterTool(MakeShared<FMCPTool_SequencerGetInfo>());
	RegisterTool(MakeShared<FMCPTool_SequencerCreate>());
	RegisterTool(MakeShared<FMCPTool_SequencerAddBinding>());
	RegisterTool(MakeShared<FMCPTool_SequencerAddTrack>());
	RegisterTool(MakeShared<FMCPTool_SequencerOpen>());
	RegisterTool(MakeShared<FMCPTool_SequencerImportFbx>());
	RegisterTool(MakeShared<FMCPTool_SequencerExportFbx>());
	RegisterTool(MakeShared<FMCPTool_SequencerSetKeyframe>());
	RegisterTool(MakeShared<FMCPTool_TextureCreateRenderTarget>());
	RegisterTool(MakeShared<FMCPTool_TextureImport>());
	RegisterTool(MakeShared<FMCPTool_TextureGetInfo>());
	RegisterTool(MakeShared<FMCPTool_TextureListTextures>());

	// Create and register async task queue tools
	// Task queue takes a raw pointer since the registry always outlives it
	TaskQueue = MakeShared<FMCPTaskQueue>(this);

	// Wire up execute_script to use the task queue for async execution
	// This allows script execution to handle permission dialogs without timing out
	if (TSharedPtr<IMCPTool>* ExecuteScriptToolPtr = Tools.Find(TEXT("execute_script")))
	{
		if (FMCPTool_ExecuteScript* ExecuteScriptTool = static_cast<FMCPTool_ExecuteScript*>(ExecuteScriptToolPtr->Get()))
		{
			ExecuteScriptTool->SetTaskQueue(TaskQueue);
			UE_LOG(LogUnrealCodex, Log, TEXT("  Wired up execute_script to task queue for async execution"));
		}
	}

	RegisterTool(MakeShared<FMCPTool_TaskSubmit>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskStatus>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskResult>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskList>(TaskQueue));
	RegisterTool(MakeShared<FMCPTool_TaskCancel>(TaskQueue));

	UE_LOG(LogUnrealCodex, Log, TEXT("Registered %d MCP tools"), Tools.Num());
}

void FMCPToolRegistry::RegisterTool(TSharedPtr<IMCPTool> Tool)
{
	if (!Tool.IsValid())
	{
		return;
	}

	FMCPToolInfo Info = Tool->GetInfo();
	if (Info.Name.IsEmpty())
	{
		UE_LOG(LogUnrealCodex, Warning, TEXT("Cannot register tool with empty name"));
		return;
	}

	if (Tools.Contains(Info.Name))
	{
		UE_LOG(LogUnrealCodex, Warning, TEXT("Tool '%s' is already registered, replacing"), *Info.Name);
	}

	Tools.Add(Info.Name, Tool);
	UE_LOG(LogUnrealCodex, Log, TEXT("  Registered tool: %s"), *Info.Name);
}

void FMCPToolRegistry::UnregisterTool(const FString& ToolName)
{
	if (Tools.Remove(ToolName) > 0)
	{
		InvalidateToolCache();
		UE_LOG(LogUnrealCodex, Log, TEXT("Unregistered tool: %s"), *ToolName);
	}
}

void FMCPToolRegistry::InvalidateToolCache()
{
	bCacheValid = false;
	CachedToolInfo.Empty();
}

TArray<FMCPToolInfo> FMCPToolRegistry::GetAllTools() const
{
	// Return cached result if valid
	if (bCacheValid)
	{
		return CachedToolInfo;
	}

	// Rebuild cache
	CachedToolInfo.Empty(Tools.Num());
	for (const auto& Pair : Tools)
	{
		if (Pair.Value.IsValid())
		{
			CachedToolInfo.Add(Pair.Value->GetInfo());
		}
	}
	bCacheValid = true;

	return CachedToolInfo;
}

FMCPToolResult FMCPToolRegistry::ExecuteTool(const FString& ToolName, const TSharedRef<FJsonObject>& Params)
{
	TSharedPtr<IMCPTool>* FoundTool = Tools.Find(ToolName);
	if (!FoundTool || !FoundTool->IsValid())
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Tool '%s' not found"), *ToolName));
	}

	const FMCPToolInfo ToolInfo = (*FoundTool)->GetInfo();

	// Pre-execution hooks can block execution and return an explicit result.
	FMCPToolResult Result;
	if (HookManager.IsValid() && !HookManager->RunBeforeHooks(ToolInfo, Params, Result))
	{
		UE_LOG(LogUnrealCodex, Warning, TEXT("Tool '%s' blocked by pre-execution hook: %s"), *ToolInfo.Name, *Result.Message);
		return Result;
	}

	UE_LOG(LogUnrealCodex, Log, TEXT("Executing MCP tool: %s"), *ToolName);

	const double StartSeconds = FPlatformTime::Seconds();

	// Execute on game thread to ensure safe access to engine objects

	if (IsInGameThread())
	{
		Result = (*FoundTool)->Execute(Params);
	}
	else
	{
		// If called from non-game thread, dispatch to game thread and wait with timeout
		// Use shared pointers for all state to avoid use-after-free if timeout occurs
		TSharedPtr<FMCPToolResult> SharedResult = MakeShared<FMCPToolResult>();
		TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = MakeShareable(FPlatformProcess::GetSynchEventFromPool(),
			[](FEvent* Event) { FPlatformProcess::ReturnSynchEventToPool(Event); });
		TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> bTaskCompleted = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);

		// Use FTSTicker to dispatch to game thread at a safe point between subsystem ticks.
		// AsyncTask(GameThread) can fire during streaming manager iteration, causing
		// re-entrancy into LevelRenderAssetManagersLock (assertion crash).
		FTSTicker::GetCoreTicker().AddTicker(TEXT("MCPTool_Execute"), 0.0f,
			[SharedResult, FoundTool, Params, CompletionEvent, bTaskCompleted](float) -> bool
		{
			*SharedResult = (*FoundTool)->Execute(Params);
			*bTaskCompleted = true;
			CompletionEvent->Trigger();
			return false; // One-shot, don't reschedule
		});

		// Wait with timeout to prevent indefinite hangs
		const uint32 TimeoutMs = UnrealCodexConstants::MCPServer::GameThreadTimeoutMs;
		const bool bSignaled = CompletionEvent->Wait(TimeoutMs);

		if (!bSignaled || !(*bTaskCompleted))
		{
			UE_LOG(LogUnrealCodex, Error, TEXT("Tool '%s' execution timed out after %d ms"), *ToolName, TimeoutMs);
			return FMCPToolResult::Error(FString::Printf(TEXT("Tool execution timed out after %d seconds"), TimeoutMs / 1000));
		}

		// Copy result from shared storage
		Result = *SharedResult;
	}

	if (HookManager.IsValid())
	{
		HookManager->RunAfterHooks(ToolInfo, Params, Result);
	}

	const double DurationMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	UE_LOG(LogUnrealCodex, Log, TEXT("Tool '%s' execution %s in %.2f ms: %s"),
		*ToolName,
		Result.bSuccess ? TEXT("succeeded") : TEXT("failed"),
		DurationMs,
		*Result.Message);

	return Result;
}

bool FMCPToolRegistry::HasTool(const FString& ToolName) const
{
	return Tools.Contains(ToolName);
}

void FMCPToolRegistry::RegisterHook(const TSharedPtr<IMCPToolHook>& Hook)
{
	if (!HookManager.IsValid())
	{
		HookManager = MakeShared<FMCPToolHookManager>();
	}

	HookManager->RegisterHook(Hook);
}
