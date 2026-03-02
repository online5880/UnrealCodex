// Copyright Natali Caggiano. All Rights Reserved.

#include "UnrealClaudeCommands.h"

#define LOCTEXT_NAMESPACE "UnrealClaude"

void FUnrealClaudeCommands::RegisterCommands()
{
	UI_COMMAND(
		OpenClaudePanel,
		"Codex Assistant",
		"Open the Codex AI Assistant panel for UE5.7 help",
		EUserInterfaceActionType::Button,
		FInputChord()
	);

	UI_COMMAND(
		QuickAsk,
		"Quick Ask Codex",
		"Quickly ask Codex a question",
		EUserInterfaceActionType::Button,
		FInputChord()
	);
}

#undef LOCTEXT_NAMESPACE
