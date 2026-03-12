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
