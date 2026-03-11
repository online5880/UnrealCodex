# macOS Installation Guide for UnrealCodex

Tested on **Apple Silicon** (M4 Max), **UE 5.7.0**, **macOS 15**.

## What You Need

- **Unreal Engine 5.7** installed and working
- Codex CLI access via your ChatGPT account or API key configuration

## Step 1: Install Node.js

The plugin needs Node.js to run its MCP bridge. If you already have it, skip to Step 2.

**Option A** — Download the installer from [nodejs.org](https://nodejs.org/) (pick the LTS version).

**Option B** — If you have Homebrew:

```bash
brew install node
```

Verify it worked:

```bash
node --version   # should print v18 or higher
npm --version    # should print a version number
```

## Step 2: Install the Codex CLI

Open Terminal and run:

```bash
npm i -g @openai/codex
```

Then authenticate:

```bash
codex login
```

This opens a browser window to sign in with your ChatGPT account. API key mode also works if configured separately.

## Step 3: Install the Plugin

### Prebuilt (recommended)

1. Download the latest macOS package from the [UnrealCodex releases page](https://github.com/online5880/UnrealCodex/releases) if one is available
2. Unzip it — you should see a folder called `UnrealCodex`
3. Copy the `UnrealCodex` folder into your project's `Plugins` directory:

```
YourProject/
  Plugins/
    UnrealCodex/      <-- put it here
      Binaries/
      Resources/
      Source/
      UnrealCodex.uplugin
```

4. Install the MCP bridge dependencies:

```bash
cd YourProject/Plugins/UnrealCodex/Resources/mcp-bridge
npm install
```

### From source (if you prefer to build yourself)

```bash
# Clone the repo (includes the MCP bridge submodule)
git clone --recurse-submodules https://github.com/online5880/UnrealCodex.git
cd UnrealCodex

# Install MCP bridge dependencies
cd UnrealCodex/Resources/mcp-bridge
npm install
cd ../../..

# Build (replace with your UE 5.7 path)
/Users/Shared/Epic\ Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin \
  -Plugin="$(pwd)/UnrealCodex/UnrealCodex.uplugin" \
  -Package="$(pwd)/BuiltPlugin" \
  -TargetPlatforms=Mac -Rocket

# Copy to your project
cp -R BuiltPlugin YourProject/Plugins/UnrealCodex
```

## Step 4: Launch

1. Open your project in Unreal Engine 5.7
2. Go to **Tools > Codex Assistant** (or look for it in the menu bar)
3. The status bar at the bottom should show a green **Ready** indicator
4. Type a message and press Enter or click Send

## Clipboard Image Paste

You can paste images directly into the chat:
- Copy an image to your clipboard (screenshot, Preview, Photoshop, etc.)
- Click the **Paste** button in the chat panel

No additional tools or dependencies are needed on macOS.

## Troubleshooting

**Plugin doesn't appear in the editor:**
Make sure the `UnrealCodex` folder is directly inside `Plugins/` (not nested like `Plugins/UnrealCodex/UnrealCodex/`).

**Codex hangs at "Thinking...":**
Run `codex --version` and `codex login` in Terminal to check authentication. If it asks you to log in, do so, then restart the editor.

**"Codex CLI not found" in the Output Log:**
The plugin looks for the `codex` binary in standard locations. Run `which codex` in Terminal to verify it's installed. If it's in an unusual location, make sure it's on your `PATH`.
