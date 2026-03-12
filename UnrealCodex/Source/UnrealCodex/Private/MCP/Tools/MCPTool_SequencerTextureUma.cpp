// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SequencerTextureUma.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"
#include "UObject/Package.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "Tracks/MovieSceneSubTrack.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "SequencerTools.h"
#include "MovieSceneUserImportFBXSettings.h"
#include "MovieSceneBindingProxy.h"
#include "FbxExportOption.h"

namespace
{
	bool TryGetStringAlias(const TSharedRef<FJsonObject>& Params, const TArray<FString>& Keys, FString& OutValue)
	{
		for (const FString& Key : Keys)
		{
			if (Params->TryGetStringField(Key, OutValue) && !OutValue.IsEmpty())
			{
				return true;
			}
		}
		return false;
	}

	bool IsTextureAssetData(const FAssetData& AssetData)
	{
#if ENGINE_MAJOR_VERSION >= 5
		return AssetData.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName();
#else
		return AssetData.AssetClass == UTexture2D::StaticClass()->GetFName();
#endif
	}

	UClass* ResolveTrackClass(const FString& TrackType)
	{
		if (TrackType.Equals(TEXT("audio"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneAudioTrack::StaticClass();
		}
		if (TrackType.Equals(TEXT("event"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneEventTrack::StaticClass();
		}
		if (TrackType.Equals(TEXT("fade"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneFadeTrack::StaticClass();
		}
		if (TrackType.Equals(TEXT("level_visibility"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneLevelVisibilityTrack::StaticClass();
		}
		if (TrackType.Equals(TEXT("camera_cut"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneCameraCutTrack::StaticClass();
		}
		if (TrackType.Equals(TEXT("cinematic_shot"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneCinematicShotTrack::StaticClass();
		}
		if (TrackType.Equals(TEXT("sub"), ESearchCase::IgnoreCase))
		{
			return UMovieSceneSubTrack::StaticClass();
		}

		return nullptr;
	}

	UClass* ResolveSpawnableActorClass(const FString& ActorNameOrPath)
	{
		if (ActorNameOrPath.StartsWith(TEXT("/")))
		{
			if (UClass* DirectClass = LoadClass<AActor>(nullptr, *ActorNameOrPath))
			{
				return DirectClass;
			}

			if (UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ActorNameOrPath))
			{
				if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
				{
					return Blueprint->GeneratedClass;
				}
			}

			return nullptr;
		}

		return AActor::StaticClass();
	}
}

FMCPToolResult FMCPTool_SequencerGetInfo::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence not found: %s"), *SequencePath));
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to get MovieScene from sequence: %s"), *SequencePath));
	}

	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

	TArray<TSharedPtr<FJsonValue>> Tracks;
	for (UMovieSceneTrack* Track : MovieScene->GetMasterTracks())
	{
		if (!Track)
		{
			continue;
		}

		TSharedPtr<FJsonObject> TrackObj = MakeShared<FJsonObject>();
		TrackObj->SetStringField(TEXT("name"), Track->GetDisplayName().ToString());
		TrackObj->SetStringField(TEXT("class"), Track->GetClass()->GetName());
		Tracks.Add(MakeShared<FJsonValueObject>(TrackObj));
	}

	TArray<TSharedPtr<FJsonValue>> Bindings;
	for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
	{
		TSharedPtr<FJsonObject> BindingObj = MakeShared<FJsonObject>();
		BindingObj->SetStringField(TEXT("name"), Binding.GetName());
		BindingObj->SetStringField(TEXT("id"), Binding.GetObjectGuid().ToString(EGuidFormats::DigitsWithHyphens));
		BindingObj->SetStringField(
			TEXT("type"),
			MovieScene->FindSpawnable(Binding.GetObjectGuid()) != nullptr ? TEXT("spawnable") : TEXT("possessable"));
		Bindings.Add(MakeShared<FJsonValueObject>(BindingObj));
	}

	TSharedPtr<FJsonObject> FrameRangeObj = MakeShared<FJsonObject>();
	FrameRangeObj->SetNumberField(TEXT("start"), PlaybackRange.GetLowerBoundValue().Value);
	FrameRangeObj->SetNumberField(TEXT("end"), PlaybackRange.GetUpperBoundValue().Value);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("objectPath"), Sequence->GetPathName());
	ResultData->SetArrayField(TEXT("tracks"), Tracks);
	ResultData->SetArrayField(TEXT("bindings"), Bindings);
	ResultData->SetObjectField(TEXT("frameRange"), FrameRangeObj);
	ResultData->SetStringField(
		TEXT("playRate"),
		FString::Printf(TEXT("%d/%d"), DisplayRate.Numerator, DisplayRate.Denominator));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved sequence info: %s"), *Sequence->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_SequencerCreate::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequenceName;
	if (!TryGetStringAlias(Params, { TEXT("sequence_name"), TEXT("sequenceName") }, SequenceName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_name (or sequenceName)"));
	}

	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	if (!FPackageName::IsValidPath(SequencePath))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid sequence path: %s"), *SequencePath));
	}

	const FString PackageName = SequencePath / SequenceName;
	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid package name: %s"), *PackageName));
	}

	const FString ObjectPath = PackageName + TEXT(".") + SequenceName;
	if (LoadObject<ULevelSequence>(nullptr, *ObjectPath) != nullptr)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence already exists: %s"), *ObjectPath));
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
	}

	ULevelSequence* NewSequence = NewObject<ULevelSequence>(Package, *SequenceName, RF_Public | RF_Standalone);
	if (!NewSequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create Level Sequence '%s' in '%s'"), *SequenceName, *SequencePath));
	}

	FAssetRegistryModule::AssetCreated(NewSequence);
	Package->MarkPackageDirty();

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequenceName"), SequenceName);
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("objectPath"), NewSequence->GetPathName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Created Level Sequence: %s"), *NewSequence->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_SequencerAddBinding::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	FString ActorName;
	if (!TryGetStringAlias(Params, { TEXT("actor_name"), TEXT("actorName") }, ActorName))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: actor_name (or actorName)"));
	}

	FString BindingType = ExtractOptionalString(Params, TEXT("binding_type"));
	if (BindingType.IsEmpty())
	{
		BindingType = ExtractOptionalString(Params, TEXT("bindingType"));
	}
	if (BindingType.IsEmpty())
	{
		BindingType = TEXT("possessable");
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence not found: %s"), *SequencePath));
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to get MovieScene from sequence: %s"), *SequencePath));
	}

	FGuid BindingGuid;
	if (BindingType.Equals(TEXT("possessable"), ESearchCase::IgnoreCase))
	{
		BindingGuid = MovieScene->AddPossessable(ActorName, AActor::StaticClass());
	}
	else if (BindingType.Equals(TEXT("spawnable"), ESearchCase::IgnoreCase))
	{
		UClass* ActorClass = ResolveSpawnableActorClass(ActorName);
		if (!ActorClass)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Failed to resolve spawnable actor class from '%s'"), *ActorName));
		}

		UObject* ObjectTemplate = ActorClass->GetDefaultObject();
		if (!ObjectTemplate)
		{
			return FMCPToolResult::Error(FString::Printf(TEXT("Failed to create default object for class '%s'"), *ActorClass->GetName()));
		}

		BindingGuid = MovieScene->AddSpawnable(ActorClass->GetName(), *ObjectTemplate);
	}
	else
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("Invalid binding_type '%s'. Must be 'possessable' or 'spawnable'."), *BindingType));
	}

	if (!BindingGuid.IsValid())
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("Failed to add %s binding for '%s'"), *BindingType.ToLower(), *ActorName));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("actorName"), ActorName);
	ResultData->SetStringField(TEXT("bindingType"), BindingType.ToLower());
	ResultData->SetStringField(TEXT("bindingId"), BindingGuid.ToString(EGuidFormats::DigitsWithHyphens));

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added %s binding to sequence: %s"), *BindingType.ToLower(), *Sequence->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_SequencerAddTrack::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	FString TrackType;
	if (!TryGetStringAlias(Params, { TEXT("track_type"), TEXT("trackType") }, TrackType))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: track_type (or trackType)"));
	}

	FString ObjectPath = ExtractOptionalString(Params, TEXT("object_path"));
	if (ObjectPath.IsEmpty())
	{
		ObjectPath = ExtractOptionalString(Params, TEXT("objectPath"));
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence not found: %s"), *SequencePath));
	}

	UMovieScene* MovieScene = Sequence->GetMovieScene();
	if (!MovieScene)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to get MovieScene from sequence: %s"), *SequencePath));
	}

	UClass* TrackClass = ResolveTrackClass(TrackType);
	if (!TrackClass)
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("Unknown track_type '%s'. Valid types: audio, event, fade, level_visibility, camera_cut, cinematic_shot, sub"), *TrackType));
	}

	UMovieSceneTrack* Track = MovieScene->AddMasterTrack(TrackClass);
	if (!Track)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Failed to add track of type '%s'"), *TrackType));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("trackType"), TrackType);
	ResultData->SetStringField(TEXT("trackClass"), TrackClass->GetName());
	ResultData->SetStringField(TEXT("objectPath"), ObjectPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Added track '%s' to sequence: %s"), *TrackClass->GetName(), *Sequence->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_TextureGetInfo::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString TexturePath;
	if (!TryGetStringAlias(Params, { TEXT("texture_path"), TEXT("texturePath") }, TexturePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: texture_path (or texturePath)"));
	}

	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
	if (!Texture)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Texture not found at path '%s'"), *TexturePath));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("texturePath"), TexturePath);
	ResultData->SetNumberField(TEXT("width"), Texture->GetSizeX());
	ResultData->SetNumberField(TEXT("height"), Texture->GetSizeY());
	ResultData->SetStringField(TEXT("compressionSettings"), UEnum::GetValueAsString(Texture->CompressionSettings));
	ResultData->SetStringField(TEXT("lodGroup"), UEnum::GetValueAsString(Texture->LODGroup));
	ResultData->SetNumberField(TEXT("maxTextureSize"), Texture->MaxTextureSize);
	ResultData->SetNumberField(TEXT("lodBias"), Texture->LODBias);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Retrieved texture info: %s"), *Texture->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_TextureListTextures::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString Directory = ExtractOptionalString(Params, TEXT("directory"));
	if (Directory.IsEmpty())
	{
		Directory = TEXT("/Game");
	}

	const FString NameFilter = ExtractOptionalString(Params, TEXT("filter"));

	if (!Directory.StartsWith(TEXT("/")) || !FPackageName::IsValidPath(Directory))
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Invalid directory path: %s"), *Directory));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssetsByPath(*Directory, Assets, true, false);

	TArray<TSharedPtr<FJsonValue>> TextureItems;
	for (const FAssetData& AssetData : Assets)
	{
		if (!IsTextureAssetData(AssetData))
		{
			continue;
		}

		const FString AssetName = AssetData.AssetName.ToString();
		if (!NameFilter.IsEmpty() && !AssetName.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		TSharedPtr<FJsonObject> TextureObj = MakeShared<FJsonObject>();
		TextureObj->SetStringField(TEXT("assetName"), AssetName);
		TextureObj->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
		TextureObj->SetStringField(TEXT("objectPath"), AssetData.GetObjectPathString());
		TextureItems.Add(MakeShared<FJsonValueObject>(TextureObj));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("directory"), Directory);
	ResultData->SetStringField(TEXT("filter"), NameFilter);
	ResultData->SetNumberField(TEXT("count"), TextureItems.Num());
	ResultData->SetArrayField(TEXT("textures"), TextureItems);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Found %d texture(s) in %s"), TextureItems.Num(), *Directory),
		ResultData);
}

FMCPToolResult FMCPTool_SequencerOpen::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence not found: %s"), *SequencePath));
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
	if (!AssetEditorSubsystem)
	{
		return FMCPToolResult::Error(TEXT("Failed to acquire AssetEditorSubsystem"));
	}

	AssetEditorSubsystem->OpenEditorForAsset(Sequence);

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("objectPath"), Sequence->GetPathName());

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Opened sequence: %s"), *Sequence->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_SequencerImportFbx::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	FString FbxPath;
	if (!TryGetStringAlias(Params, { TEXT("fbx_path"), TEXT("fbxPath") }, FbxPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: fbx_path (or fbxPath)"));
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence not found: %s"), *SequencePath));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("Failed to get editor world for FBX import"));
	}

	UMovieSceneUserImportFBXSettings* ImportOptions = NewObject<UMovieSceneUserImportFBXSettings>();
	if (ImportOptions)
	{
		ImportOptions->bReduceKeys = false;
	}

	TArray<FMovieSceneBindingProxy> Bindings;
	const bool bSuccess = USequencerToolsFunctionLibrary::ImportFBX(World, Sequence, Bindings, ImportOptions, FbxPath);
	if (!bSuccess)
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("FBX import failed for sequence '%s' from '%s'"), *SequencePath, *FbxPath));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("fbxPath"), FbxPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Imported FBX into sequence: %s"), *Sequence->GetName()),
		ResultData);
}

FMCPToolResult FMCPTool_SequencerExportFbx::Execute(const TSharedRef<FJsonObject>& Params)
{
	FString SequencePath;
	if (!TryGetStringAlias(Params, { TEXT("sequence_path"), TEXT("sequencePath") }, SequencePath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: sequence_path (or sequencePath)"));
	}

	FString OutputPath;
	if (!TryGetStringAlias(Params, { TEXT("output_path"), TEXT("outputPath") }, OutputPath))
	{
		return FMCPToolResult::Error(TEXT("Missing required parameter: output_path (or outputPath)"));
	}

	ULevelSequence* Sequence = LoadObject<ULevelSequence>(nullptr, *SequencePath);
	if (!Sequence)
	{
		return FMCPToolResult::Error(FString::Printf(TEXT("Level Sequence not found: %s"), *SequencePath));
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FMCPToolResult::Error(TEXT("Failed to get editor world for FBX export"));
	}

	UFbxExportOption* ExportOptions = NewObject<UFbxExportOption>();
	TArray<FMovieSceneBindingProxy> Bindings;
	const bool bSuccess = USequencerToolsFunctionLibrary::ExportFBX(World, Sequence, Bindings, ExportOptions, OutputPath);
	if (!bSuccess)
	{
		return FMCPToolResult::Error(
			FString::Printf(TEXT("FBX export failed for sequence '%s'"), *SequencePath));
	}

	TSharedPtr<FJsonObject> ResultData = MakeShared<FJsonObject>();
	ResultData->SetStringField(TEXT("sequencePath"), SequencePath);
	ResultData->SetStringField(TEXT("outputPath"), OutputPath);

	return FMCPToolResult::Success(
		FString::Printf(TEXT("Exported FBX from sequence: %s"), *Sequence->GetName()),
		ResultData);
}
