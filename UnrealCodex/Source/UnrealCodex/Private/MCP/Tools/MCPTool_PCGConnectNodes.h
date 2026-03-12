// Copyright Natali Caggiano. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MCP/MCPToolBase.h"

/**
 * MCP Tool: Connect two nodes in a PCG graph (UMA parity: pcg_connect_nodes.py)
 */
class FMCPTool_PCGConnectNodes : public FMCPToolBase
{
public:
	virtual FMCPToolInfo GetInfo() const override
	{
		FMCPToolInfo Info;
		Info.Name = TEXT("pcg_connect_nodes");
		Info.Description = TEXT(
			"Connect two nodes in a PCGGraph by pin names. "
			"Requires graph_path/source_node/source_pin/target_node/target_pin (camelCase aliases supported)."
		);
		Info.Parameters = {
			FMCPToolParameter(TEXT("graph_path"), TEXT("string"), TEXT("PCGGraph asset path."), false),
			FMCPToolParameter(TEXT("graphPath"), TEXT("string"), TEXT("Alias for graph_path (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("source_node"), TEXT("string"), TEXT("Source node name/id."), false),
			FMCPToolParameter(TEXT("sourceNode"), TEXT("string"), TEXT("Alias for source_node (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("source_pin"), TEXT("string"), TEXT("Output pin label on source node."), false),
			FMCPToolParameter(TEXT("sourcePin"), TEXT("string"), TEXT("Alias for source_pin (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("target_node"), TEXT("string"), TEXT("Target node name/id."), false),
			FMCPToolParameter(TEXT("targetNode"), TEXT("string"), TEXT("Alias for target_node (UMA compatibility)."), false),
			FMCPToolParameter(TEXT("target_pin"), TEXT("string"), TEXT("Input pin label on target node."), false),
			FMCPToolParameter(TEXT("targetPin"), TEXT("string"), TEXT("Alias for target_pin (UMA compatibility)."), false)
		};
		Info.Annotations = FMCPToolAnnotations::Modifying();
		return Info;
	}

	virtual FMCPToolResult Execute(const TSharedRef<FJsonObject>& Params) override;
};
