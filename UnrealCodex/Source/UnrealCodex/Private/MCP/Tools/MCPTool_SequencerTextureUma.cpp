// Copyright Natali Caggiano. All Rights Reserved.

#include "MCPTool_SequencerTextureUma.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "LevelSequence.h"
#include "Misc/PackageName.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieSceneTrack.h"
#include "Modules/ModuleManager.h"
#include "Runtime/Launch/Resources/Version.h"

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
