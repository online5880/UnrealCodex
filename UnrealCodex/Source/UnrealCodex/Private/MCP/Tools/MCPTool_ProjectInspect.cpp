// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ProjectInspect.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ProjectContext.h"

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
	if (Action == TEXT("dependencies"))
	{
		return ExecuteDependencies();
	}
	if (Action == TEXT("class_hierarchy"))
	{
		return ExecuteClassHierarchy(Params);
	}

	return FMCPToolResult::Error(FString::Printf(TEXT("Unsupported action '%s'. Use plugins, settings, dependencies, or class_hierarchy."), *Action));
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

FMCPToolResult FMCPTool_ProjectInspect::ExecuteDependencies() const
{
	const FString SourceDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Source"));

	TArray<FString> BuildCsFiles;
	IFileManager::Get().FindFilesRecursive(BuildCsFiles, *SourceDir, TEXT("*.Build.cs"), true, false, false);

	TArray<TSharedPtr<FJsonValue>> ModulesArray;
	for (const FString& BuildCsPath : BuildCsFiles)
	{
		FString FileContent;
		if (!FFileHelper::LoadFileToString(FileContent, *BuildCsPath))
		{
			continue;
		}

		TArray<FString> PublicDeps;
		TArray<FString> PrivateDeps;

		auto ParseDependencyList = [&FileContent](const FString& Marker, TArray<FString>& OutDependencies)
		{
			int32 SearchPos = 0;
			while (true)
			{
				const int32 MarkerPos = FileContent.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchPos);
				if (MarkerPos == INDEX_NONE)
				{
					break;
				}

				const int32 OpenPos = FileContent.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, MarkerPos);
				const int32 ClosePos = OpenPos == INDEX_NONE ? INDEX_NONE : FileContent.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, OpenPos);
				if (OpenPos == INDEX_NONE || ClosePos == INDEX_NONE)
				{
					SearchPos = MarkerPos + Marker.Len();
					continue;
				}

				const FString Block = FileContent.Mid(OpenPos + 1, ClosePos - OpenPos - 1);
				TArray<FString> Lines;
				Block.ParseIntoArrayLines(Lines, true);
				for (FString Line : Lines)
				{
					Line.TrimStartAndEndInline();
					if (Line.IsEmpty())
					{
						continue;
					}
					if (Line.StartsWith(TEXT("//")))
					{
						continue;
					}
					Line.ReplaceInline(TEXT(","), TEXT(""));
					Line.ReplaceInline(TEXT("\""), TEXT(""));
					Line.ReplaceInline(TEXT("\'"), TEXT(""));
					Line.TrimStartAndEndInline();
					if (!Line.IsEmpty() && !OutDependencies.Contains(Line))
					{
						OutDependencies.Add(Line);
					}
				}

				SearchPos = ClosePos + 1;
			}
		};

		ParseDependencyList(TEXT("PublicDependencyModuleNames.AddRange"), PublicDeps);
		ParseDependencyList(TEXT("PrivateDependencyModuleNames.AddRange"), PrivateDeps);

		if (PublicDeps.Num() == 0 && PrivateDeps.Num() == 0)
		{
			continue;
		}

		FString RelativePath = BuildCsPath;
		FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectDir());

		TSharedPtr<FJsonObject> ModuleObj = MakeShared<FJsonObject>();
		ModuleObj->SetStringField(TEXT("build_file"), RelativePath);
		ModuleObj->SetStringField(TEXT("module_name"), FPaths::GetBaseFilename(BuildCsPath).Replace(TEXT(".Build"), TEXT("")));

		TArray<TSharedPtr<FJsonValue>> PublicArray;
		for (const FString& Dep : PublicDeps)
		{
			PublicArray.Add(MakeShared<FJsonValueString>(Dep));
		}
		ModuleObj->SetArrayField(TEXT("public_dependencies"), PublicArray);

		TArray<TSharedPtr<FJsonValue>> PrivateArray;
		for (const FString& Dep : PrivateDeps)
		{
			PrivateArray.Add(MakeShared<FJsonValueString>(Dep));
		}
		ModuleObj->SetArrayField(TEXT("private_dependencies"), PrivateArray);

		ModulesArray.Add(MakeShared<FJsonValueObject>(ModuleObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action"), TEXT("dependencies"));
	Data->SetArrayField(TEXT("modules"), ModulesArray);
	Data->SetNumberField(TEXT("count"), ModulesArray.Num());

	return FMCPToolResult::Success(FString::Printf(TEXT("Retrieved dependency snapshot for %d modules."), ModulesArray.Num()), Data);
}

FMCPToolResult FMCPTool_ProjectInspect::ExecuteClassHierarchy(const TSharedRef<FJsonObject>& Params) const
{
	bool bForceRefresh = false;
	if (Params->HasTypedField<EJson::Boolean>(TEXT("force_refresh")))
	{
		bForceRefresh = Params->GetBoolField(TEXT("force_refresh"));
	}

	const FProjectContext& Context = FProjectContextManager::Get().GetContext(bForceRefresh);

	TArray<TSharedPtr<FJsonValue>> ClassesArray;
	ClassesArray.Reserve(Context.UClasses.Num());

	for (const FUClassInfo& ClassInfo : Context.UClasses)
	{
		TSharedPtr<FJsonObject> ClassObj = MakeShared<FJsonObject>();
		ClassObj->SetStringField(TEXT("class_name"), ClassInfo.ClassName);
		ClassObj->SetStringField(TEXT("parent_class"), ClassInfo.ParentClass);
		ClassObj->SetStringField(TEXT("file_path"), ClassInfo.FilePath);
		ClassObj->SetBoolField(TEXT("is_blueprint"), ClassInfo.bIsBlueprint);
		ClassesArray.Add(MakeShared<FJsonValueObject>(ClassObj));
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("action"), TEXT("class_hierarchy"));
	Data->SetArrayField(TEXT("classes"), ClassesArray);
	Data->SetNumberField(TEXT("count"), ClassesArray.Num());
	Data->SetBoolField(TEXT("force_refresh"), bForceRefresh);

	return FMCPToolResult::Success(FString::Printf(TEXT("Retrieved class hierarchy snapshot (%d classes)."), ClassesArray.Num()), Data);
}
