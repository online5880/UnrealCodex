# UnrealCodex Migration Design (Claude CLI -> OpenAI Codex CLI)

## Context

`online5880/UnrealCodex` is currently a near-identical fork of `Natfii/UnrealClaude` with the runtime and UX still centered on Claude Code.

Primary coupling points:

- Runtime process discovery and invocation in `UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp`
- Prompt assembly and defaults in `UnrealClaude/Source/UnrealClaude/Private/ClaudeSubsystem.cpp`
- User-visible labels and startup diagnostics in `UnrealClaude/Source/UnrealClaude/Private/UnrealClaudeModule.cpp`
- Install and troubleshooting docs in `README.md`, `INSTALL_LINUX.md`, and `UnrealClaude/Resources/mcp-bridge/README.md`

## Goal

Migrate the plugin to run with OpenAI Codex CLI as the default engine while minimizing breakage risk and preserving existing Unreal MCP workflows.

## Chosen Strategy

Stage-based migration (approved):

1. Stage 1: Runtime engine switch (functional migration)
2. Stage 2: Surface branding and docs switch (user-facing consistency)
3. Stage 3: Deep codebase/module renaming (optional hardening phase)

This design implements Stages 1 and 2 in this pass, and leaves Stage 3 as an intentional follow-up.

## Design Details

### Stage 1: Runtime engine switch

1. Replace executable discovery priority from `claude` to `codex`, with fallback to `claude` for temporary compatibility.
2. Switch non-interactive invocation from Claude print mode to Codex exec mode:
   - Use `codex exec`
   - Use JSON event stream mode for structured parsing
   - Keep stdin-fed prompt payload behavior so command-line length limits remain avoided
3. Keep MCP bridge plumbing intact (existing `--mcp-config` behavior remains); only runtime invocation changes.
4. Adapt parser naming and expectations from Claude NDJSON terminology to generic/Codex JSON events while retaining backward-tolerant fallbacks.

### Stage 2: UX and documentation alignment

1. Update startup logs and "CLI not found" messaging to reference Codex first.
2. Update README and Linux install docs:
   - Install command (`npm i -g @openai/codex`)
   - Auth flow (`codex login`)
   - Verification command (`codex --version`, non-interactive `codex exec` sample)
3. Update bridge README wording from Claude-oriented setup language to Codex-compatible setup language.

### Stage 3: deferred deep rename

Intentionally deferred:

- Module rename (`UnrealClaude` -> `UnrealCodex`)
- Class/file rename (`Claude*` -> `Codex*`)
- Potential `.uplugin` module and plugin folder rename

Reason: high blast radius for include paths, module metadata, build references, and end-user upgrade path.

## Data Flow (post migration)

1. Editor widget submits prompt to subsystem.
2. Subsystem builds system + project context.
3. Runner launches `codex exec` with configured flags and MCP config.
4. Prompt payload is written on stdin (text + optional images).
5. Runner parses JSON stream events and emits text/tool/result events to UI.
6. Final response is persisted in existing session history.

## Error Handling

- If Codex binary is missing, emit explicit install guidance for OpenAI Codex CLI.
- If JSON parsing fails, keep fallback parser and return actionable log context.
- If process launch fails, include command path and working directory in error message for rapid debugging.

## Verification Plan

1. Smoke run with Codex installed:
   - prompt send/receive works
   - partial stream updates appear
2. MCP validation:
   - status endpoint reachable
   - at least one editor tool invocation succeeds
3. Regression:
   - cancel behavior
   - session save/load
   - attached image request path still accepted
4. Docs sanity:
   - commands in README execute with Codex CLI semantics

## Non-Goals

- No module-level rename in this pass
- No MCP server protocol redesign
- No UI layout redesign
