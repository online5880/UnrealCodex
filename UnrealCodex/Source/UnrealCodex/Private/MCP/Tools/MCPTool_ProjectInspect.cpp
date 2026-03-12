// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ProjectInspect.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

FMCPToolResult FMCPTool_ProjectInspect::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Action;
	TOptional<FMCPToolResult> ParamError;
	if (!ExtractRequiredString(Params, TEXT("action"), Action, ParamError))
	{
		return ParamError.GetValue();
	}

	Action = Action.ToLower();
	if (Action == TEXT("plugins"))
	{
		return ExecutePlugins();
	}
	if (Action == TEXT("settings"))
	{
		return ExecuteSettings();
	}

	return FMCPToolResult::Error(FString::Printf(TEXT("Unsupported action '%s'. Use plugins or settings."), *Action));
}

FMCPToolResult FMCPTool_ProjectInspect::ExecutePlugins() const
{
	TArray<TSharedRef<IPlugin>> EnabledPlugins = IPluginManager::Get().GetEnabledPlugins();

	TArray<TSharedPtr<FJsonValue>> PluginsArray;
	PluginsArray.Reserve(EnabledPlugins.Num());

	for (const TSharedRef<IPlugin>& Plugin : EnabledPlugins)
	{
		const FPluginDescriptor& Desc = Plugin->GetDescriptor();
		TSharedPtr<FJsonObject> PluginObj = MakeShared<FJsonObject>();
		PluginObj->SetStringField(TEXT("name"), Plugin->GetName());
		PluginObj->SetStringField(TEXT("friendly_name"), Desc.FriendlyName);
		PluginObj->SetStringField(TEXT("version_name"), Desc.VersionName);
		PluginObj->SetNumberField(TEXT("version"), Desc.Version);
		PluginObj->SetStringField(TEXT("category"), Desc.Category);
		PluginObj->SetBoolField(TEXT("is_beta"), Desc.bIsBetaVersion);
		PluginObj->SetBoolField(TEXT("is_experimental"), Desc.bIsExperimentalVersion);
		PluginObj->SetBoolField(TEXT("is_installed"), Desc.bInstalled);
		PluginObj->SetBoolField(TEXT("is_enabled"), true);
		PluginObj->SetStringField(TEXT("base_dir"), Plugin->GetBaseDir());
		PluginsArray.Add(MakeShared<FJsonValueObject>(PluginObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action"), TEXT("plugins"));
	Data->SetArrayField(TEXT("plugins"), PluginsArray);
	Data->SetNumberField(TEXT("count"), EnabledPlugins.Num());

	return FMCPToolResult::Success(FString::Printf(TEXT("Retrieved %d enabled plugins."), EnabledPlugins.Num()), Data);
}

FMCPToolResult FMCPTool_ProjectInspect::ExecuteSettings() const
{
	FString GameDefaultMap;
	FString EditorStartupMap;
	FString GameInstanceClass;
	FString GlobalDefaultGameMode;

	GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), GameDefaultMap, GEngineIni);
	GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("EditorStartupMap"), EditorStartupMap, GEngineIni);
	GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameInstanceClass"), GameInstanceClass, GEngineIni);
	GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GlobalDefaultGameMode"), GlobalDefaultGameMode, GEngineIni);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action"), TEXT("settings"));
	Data->SetStringField(TEXT("project_name"), FApp::GetProjectName());
	Data->SetStringField(TEXT("project_dir"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Data->SetStringField(TEXT("engine_ini"), FPaths::ConvertRelativePathToFull(GEngineIni));
	Data->SetStringField(TEXT("game_ini"), FPaths::ConvertRelativePathToFull(GGameIni));
	Data->SetStringField(TEXT("game_default_map"), GameDefaultMap);
	Data->SetStringField(TEXT("editor_startup_map"), EditorStartupMap);
	Data->SetStringField(TEXT("game_instance_class"), GameInstanceClass);
	Data->SetStringField(TEXT("global_default_game_mode"), GlobalDefaultGameMode);

	return FMCPToolResult::Success(TEXT("Retrieved project settings snapshot."), Data);
}
