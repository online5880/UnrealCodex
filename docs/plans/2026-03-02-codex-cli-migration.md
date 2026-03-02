# Codex CLI Migration Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Switch UnrealCodex from Claude Code CLI runtime to OpenAI Codex CLI runtime while keeping Unreal MCP functionality and session behavior intact.

**Architecture:** Keep the existing plugin/module shape and switch only runtime command construction, binary discovery, and user-facing text/docs. Maintain stdin payload flow and structured streaming parser to avoid large command-line argument limits and preserve progressive UI updates.

**Tech Stack:** Unreal Engine 5.7 C++, Node MCP bridge, OpenAI Codex CLI (`@openai/codex`)

---

### Task 1: Add Codex-first CLI discovery and errors

**Files:**
- Modify: `UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp`

**Step 1: Write the failing test/check definition**

Define a manual check for missing runtime message:

- Force a machine where `codex` is absent from PATH.
- Trigger prompt send from editor.
- Expected final error text should mention Codex installation command.

**Step 2: Run check to verify current behavior fails requirement**

Run:

```bash
rg "Claude CLI not found|@anthropic-ai/claude-code|where claude|which claude" "UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp"
```

Expected: matches found for Claude-only messages and lookup.

**Step 3: Write minimal implementation**

- Update `GetClaudePath()` implementation to:
  - Search `codex(.exe/.cmd)` first
  - Keep optional fallback search for `claude` binaries
- Update `IsClaudeAvailable()` behavior unchanged (path non-empty)
- Update all "not found" errors to Codex-first guidance:
  - `npm i -g @openai/codex`
- Keep function/class names unchanged for this phase.

**Step 4: Run check to verify it passes**

Run:

```bash
rg "codex|@openai/codex" "UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp"
```

Expected: Codex discovery and install guidance present.

**Step 5: Commit**

```bash
git add UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp
git commit -m "feat: detect and report OpenAI Codex CLI as default runtime"
```

### Task 2: Switch command building from Claude print mode to Codex exec mode

**Files:**
- Modify: `UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp`

**Step 1: Write the failing test/check definition**

Define required command traits:

- Command line contains `exec`
- Command line requests JSON event stream
- Existing stdin prompt payload path is still used

**Step 2: Run check to verify current behavior fails requirement**

Run:

```bash
rg "-p |--verbose|--output-format stream-json|--input-format stream-json|--dangerously-skip-permissions" "UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp"
```

Expected: Claude flags present.

**Step 3: Write minimal implementation**

- In `BuildCommandLine`:
  - Replace `-p` flow with `exec`
  - Add `--json`
  - Map skip-permission semantics to Codex equivalents where applicable
- Keep MCP config attachment and allowed tools mapping where still supported.
- Keep stdin write path for prompt payload in `ExecuteProcess`.

**Step 4: Run check to verify it passes**

Run:

```bash
rg "exec|--json" "UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp"
```

Expected: Codex exec/json flags present and old Claude-only print flags removed from active path.

**Step 5: Commit**

```bash
git add UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp
git commit -m "feat: run Codex in non-interactive exec JSON mode"
```

### Task 3: Normalize streaming parser for Codex JSON events with fallback

**Files:**
- Modify: `UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp`
- Modify: `UnrealClaude/Source/UnrealClaude/Public/IClaudeRunner.h` (only if event typing needs extension)

**Step 1: Write the failing test/check definition**

Define runtime expectations:

- Partial response chunks still reach `OnProgress`
- Final response text is non-empty on success

**Step 2: Run check to verify current behavior may fail with Codex event names**

Run:

```bash
rg "type == TEXT\(\"assistant\"\)|type == TEXT\(\"result\"\)|NDJSON" "UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp"
```

Expected: parser is tightly Claude-event-oriented.

**Step 3: Write minimal implementation**

- Generalize parser to accept Codex JSON event variants used by `codex exec --json`.
- Preserve legacy fallback parser for compatibility.
- Keep structured callbacks firing in the same UI contract shape.

**Step 4: Run check to verify it passes**

Manual runtime check in editor:

- Send prompt
- Confirm live streaming text updates
- Confirm final message rendered and persisted

Expected: no parser crash, non-empty final response.

**Step 5: Commit**

```bash
git add UnrealClaude/Source/UnrealClaude/Private/ClaudeCodeRunner.cpp UnrealClaude/Source/UnrealClaude/Public/IClaudeRunner.h
git commit -m "fix: parse Codex JSON event stream while preserving fallback behavior"
```

### Task 4: Update subsystem/module UX strings to Codex branding

**Files:**
- Modify: `UnrealClaude/Source/UnrealClaude/Private/ClaudeSubsystem.cpp`
- Modify: `UnrealClaude/Source/UnrealClaude/Private/UnrealClaudeModule.cpp`

**Step 1: Write the failing test/check definition**

Define visible-string expectations:

- Startup diagnostics mention Codex
- Not-found/install text mentions Codex CLI package

**Step 2: Run check to verify current behavior fails requirement**

Run:

```bash
rg "Claude CLI|@anthropic-ai/claude-code|Claude Code" "UnrealClaude/Source/UnrealClaude/Private/ClaudeSubsystem.cpp" "UnrealClaude/Source/UnrealClaude/Private/UnrealClaudeModule.cpp"
```

Expected: Claude branding present.

**Step 3: Write minimal implementation**

- Replace user-visible runtime references with Codex wording.
- Keep class names unchanged in this phase.

**Step 4: Run check to verify it passes**

Run:

```bash
rg "Codex|@openai/codex" "UnrealClaude/Source/UnrealClaude/Private/ClaudeSubsystem.cpp" "UnrealClaude/Source/UnrealClaude/Private/UnrealClaudeModule.cpp"
```

Expected: Codex references present and coherent.

**Step 5: Commit**

```bash
git add UnrealClaude/Source/UnrealClaude/Private/ClaudeSubsystem.cpp UnrealClaude/Source/UnrealClaude/Private/UnrealClaudeModule.cpp
git commit -m "chore: align runtime-facing plugin messaging to Codex"
```

### Task 5: Update root installation and troubleshooting documentation

**Files:**
- Modify: `README.md`
- Modify: `INSTALL_LINUX.md`

**Step 1: Write the failing test/check definition**

Define doc requirements:

- Install/auth examples use Codex CLI
- Troubleshooting references Codex binary checks

**Step 2: Run check to verify current behavior fails requirement**

Run:

```bash
rg "@anthropic-ai/claude-code|claude auth login|claude --version|where claude" README.md INSTALL_LINUX.md
```

Expected: Claude commands present.

**Step 3: Write minimal implementation**

- Replace install/auth/verify snippets with Codex equivalents:
  - `npm i -g @openai/codex`
  - `codex login`
  - `codex --version`
  - `codex exec \"Hello\"`
- Preserve Unreal build/install instructions unchanged.

**Step 4: Run check to verify it passes**

Run:

```bash
rg "@openai/codex|codex login|codex --version|codex exec" README.md INSTALL_LINUX.md
```

Expected: Codex commands present.

**Step 5: Commit**

```bash
git add README.md INSTALL_LINUX.md
git commit -m "docs: migrate setup and troubleshooting instructions to Codex CLI"
```

### Task 6: Update bridge docs and run repository sanity checks

**Files:**
- Modify: `UnrealClaude/Resources/mcp-bridge/README.md`

**Step 1: Write the failing test/check definition**

Define final acceptance checks:

- Bridge docs no longer instruct Claude-specific CLI setup
- No high-priority runtime docs point to Anthropic package for primary path

**Step 2: Run check to verify current behavior fails requirement**

Run:

```bash
rg "Claude Code|@anthropic-ai/claude-code|claude " "UnrealClaude/Resources/mcp-bridge/README.md" README.md
```

Expected: Claude-oriented setup text found.

**Step 3: Write minimal implementation**

- Update bridge README wording/examples to Codex runtime assumptions.
- Keep MCP architecture and endpoints unchanged.

**Step 4: Run checks to verify pass**

Run:

```bash
rg "@openai/codex|codex" README.md INSTALL_LINUX.md "UnrealClaude/Resources/mcp-bridge/README.md"
```

And run:

```bash
git diff --stat
```

Expected: Codex docs references present; diff limited to planned files.

**Step 5: Commit**

```bash
git add UnrealClaude/Resources/mcp-bridge/README.md
git commit -m "docs: align MCP bridge guide with Codex CLI usage"
```

### Task 7: End-to-end manual validation in Unreal Editor

**Files:**
- N/A (verification task)

**Step 1: Runtime availability check**

Run:

```bash
codex --version
```

Expected: version string output.

**Step 2: Non-interactive smoke test**

Run:

```bash
codex exec --skip-git-repo-check --json "Reply with OK"
```

Expected: JSON event output and final OK-like response.

**Step 3: Unreal plugin smoke test**

- Open UE editor with plugin enabled.
- Open assistant panel.
- Send prompt.
- Verify streamed response and final persisted output.

**Step 4: MCP smoke test**

- Trigger one low-risk MCP tool from assistant flow (for example status/log query).
- Verify tool call appears and result is rendered.

**Step 5: Commit verification notes**

```bash
git add -A
git commit -m "test: verify Codex runtime migration end-to-end" 
```
