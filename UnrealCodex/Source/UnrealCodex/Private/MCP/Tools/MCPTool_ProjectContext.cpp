// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_ProjectContext.h"

#include "ProjectContext.h"

namespace
{
	template <typename T>
	int32 ClampListLimit(T Value)
	{
		return FMath::Clamp(static_cast<int32>(Value), 1, 5000);
	}

	TSharedPtr<FJsonObject> BuildSummaryData(const FProjectContext& Ctx)
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("project_name"), Ctx.ProjectName);
		Data->SetStringField(TEXT("project_path"), Ctx.ProjectPath);
		Data->SetStringField(TEXT("source_path"), Ctx.SourcePath);
		Data->SetStringField(TEXT("engine_version"), Ctx.EngineVersion);
		Data->SetStringField(TEXT("current_level"), Ctx.CurrentLevelName);
		Data->SetNumberField(TEXT("asset_count"), Ctx.AssetCount);
		Data->SetNumberField(TEXT("blueprint_count"), Ctx.BlueprintCount);
		Data->SetNumberField(TEXT("cpp_class_count"), Ctx.CppClassCount);
		Data->SetNumberField(TEXT("source_file_count"), Ctx.SourceFiles.Num());
		Data->SetNumberField(TEXT("uclass_count"), Ctx.UClasses.Num());
		Data->SetNumberField(TEXT("level_actor_count"), Ctx.LevelActors.Num());
		Data->SetStringField(TEXT("gathered_at"), Ctx.GatheredAt.ToIso8601());
		return Data;
	}
}

FMCPToolResult FMCPTool_ProjectContext::Execute(const TSharedRef<FJsonObject>& Params)
{
	const FString Mode = ExtractOptionalString(Params, TEXT("mode"), TEXT("summary")).ToLower();
	const bool bRefresh = ExtractOptionalBool(Params, TEXT("refresh"), false);

	FProjectContextManager& Manager = FProjectContextManager::Get();
	const FProjectContext& Ctx = Manager.GetContext(bRefresh);

	if (Mode != TEXT("summary") && Mode != TEXT("full"))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid mode '%s'. Expected 'summary' or 'full'."), *Mode));
	}

	if (Mode == TEXT("summary"))
	{
		return FMCPToolResult::Success(TEXT("Project context summary retrieved."), BuildSummaryData(Ctx));
	}

	const int32 MaxSourceFiles = ClampListLimit(ExtractOptionalNumber<int32>(Params, TEXT("max_source_files"), 200));
	const int32 MaxClasses = ClampListLimit(ExtractOptionalNumber<int32>(Params, TEXT("max_classes"), 200));
	const int32 MaxLevelActors = ClampListLimit(ExtractOptionalNumber<int32>(Params, TEXT("max_level_actors"), 200));

	TSharedPtr<FJsonObject> Data = BuildSummaryData(Ctx);
	Data->SetStringField(TEXT("mode"), TEXT("full"));

	// Source files
	TArray<TSharedPtr<FJsonValue>> SourceFiles;
	const int32 SourceLimit = FMath::Min(MaxSourceFiles, Ctx.SourceFiles.Num());
	for (int32 i = 0; i < SourceLimit; ++i)
	{
		SourceFiles.Add(MakeShared<FJsonValueString>(Ctx.SourceFiles[i]));
	}
	Data->SetArrayField(TEXT("source_files"), SourceFiles);
	Data->SetBoolField(TEXT("source_files_truncated"), SourceLimit < Ctx.SourceFiles.Num());

	// UClasses
	TArray<TSharedPtr<FJsonValue>> Classes;
	const int32 ClassLimit = FMath::Min(MaxClasses, Ctx.UClasses.Num());
	for (int32 i = 0; i < ClassLimit; ++i)
	{
		const FUClassInfo& ClassInfo = Ctx.UClasses[i];
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("class_name"), ClassInfo.ClassName);
		Obj->SetStringField(TEXT("parent_class"), ClassInfo.ParentClass);
		Obj->SetStringField(TEXT("file_path"), ClassInfo.FilePath);
		Obj->SetBoolField(TEXT("is_blueprint"), ClassInfo.bIsBlueprint);
		Classes.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Data->SetArrayField(TEXT("uclasses"), Classes);
	Data->SetBoolField(TEXT("uclasses_truncated"), ClassLimit < Ctx.UClasses.Num());

	// Level actors
	TArray<TSharedPtr<FJsonValue>> Actors;
	const int32 ActorLimit = FMath::Min(MaxLevelActors, Ctx.LevelActors.Num());
	for (int32 i = 0; i < ActorLimit; ++i)
	{
		const FLevelActorInfo& Actor = Ctx.LevelActors[i];
		TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Actor.Name);
		Obj->SetStringField(TEXT("label"), Actor.Label);
		Obj->SetStringField(TEXT("class_name"), Actor.ClassName);
		Obj->SetNumberField(TEXT("location_x"), Actor.Location.X);
		Obj->SetNumberField(TEXT("location_y"), Actor.Location.Y);
		Obj->SetNumberField(TEXT("location_z"), Actor.Location.Z);
		Actors.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Data->SetArrayField(TEXT("level_actors"), Actors);
	Data->SetBoolField(TEXT("level_actors_truncated"), ActorLimit < Ctx.LevelActors.Num());

	return FMCPToolResult::Success(TEXT("Project context full snapshot retrieved."), Data);
}
