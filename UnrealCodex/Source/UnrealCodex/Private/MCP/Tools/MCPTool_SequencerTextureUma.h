// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/** UMA-compatible sequencer_get_info tool. */
class FMCPTool_SequencerGetInfo : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sequencer_get_info");
		Info.Description = TEXT("Get summary information about a Level Sequence asset.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("sequence_path"), TEXT("string"), TEXT("Level Sequence asset path (e.g. /Game/Cinematics/MySequence.MySequence)."), false),
			FMCPToolParameter(TEXT("sequencePath"), TEXT("string"), TEXT("Alias for sequence_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

/** UMA-compatible sequencer_create tool. */
class FMCPTool_SequencerCreate : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sequencer_create");
		Info.Description = TEXT("Create a new Level Sequence asset.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("sequence_name"), TEXT("string"), TEXT("Name for the new Level Sequence asset."), false),
			FMCPToolParameter(TEXT("sequenceName"), TEXT("string"), TEXT("Alias for sequence_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("sequence_path"), TEXT("string"), TEXT("Package folder path for the asset (e.g. /Game/Cinematics)."), false),
			FMCPToolParameter(TEXT("sequencePath"), TEXT("string"), TEXT("Alias for sequence_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Destructive();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

/** UMA-compatible sequencer_add_binding tool. */
class FMCPTool_SequencerAddBinding : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sequencer_add_binding");
		Info.Description = TEXT("Add a possessable or spawnable binding to a Level Sequence.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("sequence_path"), TEXT("string"), TEXT("Level Sequence asset path."), false),
			FMCPToolParameter(TEXT("sequencePath"), TEXT("string"), TEXT("Alias for sequence_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("actor_name"), TEXT("string"), TEXT("Actor label for possessable, or Actor class path for spawnable."), false),
			FMCPToolParameter(TEXT("actorName"), TEXT("string"), TEXT("Alias for actor_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("binding_type"), TEXT("string"), TEXT("Binding type: possessable or spawnable (default possessable)."), false, TEXT("possessable")),
			FMCPToolParameter(TEXT("bindingType"), TEXT("string"), TEXT("Alias for binding_type (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Destructive();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

/** UMA-compatible sequencer_add_track tool. */
class FMCPTool_SequencerAddTrack : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("sequencer_add_track");
		Info.Description = TEXT("Add a master track to a Level Sequence.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("sequence_path"), TEXT("string"), TEXT("Level Sequence asset path."), false),
			FMCPToolParameter(TEXT("sequencePath"), TEXT("string"), TEXT("Alias for sequence_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("track_type"), TEXT("string"), TEXT("Track type (audio, event, fade, level_visibility, camera_cut, cinematic_shot, sub)."), false),
			FMCPToolParameter(TEXT("trackType"), TEXT("string"), TEXT("Alias for track_type (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("object_path"), TEXT("string"), TEXT("Optional object path passthrough for UMA compatibility."), false),
			FMCPToolParameter(TEXT("objectPath"), TEXT("string"), TEXT("Alias for object_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Destructive();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

/** UMA-compatible texture_get_info tool. */
class FMCPTool_TextureGetInfo : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("texture_get_info");
		Info.Description = TEXT("Get Texture2D information such as resolution and compression settings.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("texture_path"), TEXT("string"), TEXT("Texture2D asset path (e.g. /Game/Textures/T_Albedo.T_Albedo)."), false),
			FMCPToolParameter(TEXT("texturePath"), TEXT("string"), TEXT("Alias for texture_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};

/** UMA-compatible texture_list_textures tool. */
class FMCPTool_TextureListTextures : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("texture_list_textures");
		Info.Description = TEXT("List Texture2D assets under a directory with optional name filtering.");
		Info.Parameters = {
			FMCPToolParameter(TEXT("directory"), TEXT("string"), TEXT("Package path to search (default: /Game)."), false, TEXT("/Game")),
			FMCPToolParameter(TEXT("filter"), TEXT("string"), TEXT("Optional substring filter on texture asset name."), false, TEXT(""))
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
