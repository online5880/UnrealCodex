// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Add a node to a PCG graph (UMA parity: pcg_add_node.py)
 */
class FMCPTool_PCGAddNode : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("pcg_add_node");
		Info.Description = TEXT(
			"Add a node of the specified type to a PCGGraph asset. "
			"Requires graph_path (or graphPath alias) and node_type (or nodeType alias)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path (e.g. /Game/PCG/PCG_MyGraph.PCG_MyGraph)"), false),
			FMCPToolParameter(TEXT("graphPath"), TEXT("string"), TEXT("Alias for graph_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("node_type"), TEXT("string"), TEXT("Node class path (e.g. /Script/PCG.PCGSurfaceSamplerSettings)."), false),
			FMCPToolParameter(TEXT("nodeType"), TEXT("string"), TEXT("Alias for node_type (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("node_label"), TEXT("string"), TEXT("Optional display label (echoed in result for parity)."), false),
			FMCPToolParameter(TEXT("nodeLabel"), TEXT("string"), TEXT("Alias for node_label (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
