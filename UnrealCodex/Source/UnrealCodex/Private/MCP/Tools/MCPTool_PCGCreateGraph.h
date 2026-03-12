// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Create a PCG graph asset (UMA parity: pcg_create_graph.py)
 */
class FMCPTool_PCGCreateGraph : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("pcg_create_graph");
		Info.Description = TEXT(
			"Create a new PCGGraph asset. "
			"Requires graph_name/graph_path (graphName/graphPath aliases supported)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("graph_name"), TEXT("string"), TEXT("Name for the new PCG graph asset."), false),
			FMCPToolParameter(TEXT("graphName"), TEXT("string"), TEXT("Alias for graph_name (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("graph_path"), TEXT("string"), TEXT("Destination folder path (e.g. /Game/PCG)."), false),
			FMCPToolParameter(TEXT("graphPath"), TEXT("string"), TEXT("Alias for graph_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
