// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Read PCG graph information (UMA parity: pcg_get_info.py)
 */
class FMCPTool_PCGGetInfo : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("pcg_get_info");
		Info.Description = TEXT(
			"Read nodes and connections from a PCGGraph asset. "
			"Requires graph_path (or graphPath alias)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path (e.g. /Game/PCG/PCG_MyGraph.PCG_MyGraph)"), false),
			FMCPToolParameter(TEXT("graphPath"), TEXT("string"), TEXT("Alias for graph_path (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::ReadOnly();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
