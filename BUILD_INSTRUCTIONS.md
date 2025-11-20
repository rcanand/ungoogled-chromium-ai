# Building and Running ungoogled-chromium with AI Sidebar

## Overview

This fork of ungoogled-chromium includes an integrated AI sidebar that supports:
- **Local MLX models** (text, image, audio) on macOS with Apple Silicon
- **Ollama models** (all platforms)
- **OpenRouter API models** (all platforms)

Key features:
- Switch models for each message in a chat
- Use current page as context
- Web search integration
- Multi-modal support (text, images, audio)

## Important Note: Platform Support

**ungoogled-chromium is desktop-only.** This implementation works on:
- ✅ **macOS** (including MLX support for Apple Silicon)
- ✅ **Linux**
- ✅ **Windows**
- ❌ **iOS** - Not supported (ungoogled-chromium doesn't have iOS builds)

For macOS users, the MLX integration provides local AI capabilities on Apple Silicon.

## Prerequisites

### All Platforms

1. **Chromium build dependencies**
   - Follow the official Chromium build instructions for your platform:
     - [Linux](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md)
     - [macOS](https://chromium.googlesource.com/chromium/src/+/main/docs/mac_build_instructions.md)
     - [Windows](https://chromium.googlesource.com/chromium/src/+/main/docs/windows_build_instructions.md)

2. **Python 3.8+**

3. **Git**

### macOS Only (for MLX support)

For local MLX model support on Apple Silicon:

```bash
# Install MLX and related libraries
pip3 install mlx mlx-lm mlx-vlm

# For vision model support
pip3 install Pillow
```

### Optional: Ollama (All Platforms)

For local model inference with Ollama:

1. Install Ollama from [ollama.ai](https://ollama.ai)
2. Pull some models:
   ```bash
   ollama pull llama2
   ollama pull codellama
   ollama pull llava  # Vision model
   ```

### Optional: OpenRouter API

For cloud-based models:
1. Sign up at [openrouter.ai](https://openrouter.ai)
2. Get your API key from the dashboard

## Building ungoogled-chromium with AI Sidebar

### Step 1: Get the Chromium Source

```bash
# Create a directory for Chromium
mkdir ~/chromium
cd ~/chromium

# Fetch the source (this takes a while and requires ~30GB of space)
fetch --nohooks chromium
cd src

# Checkout a specific version (match ungoogled-chromium version)
git checkout 142.0.7444.162

# Run hooks
gclient runhooks
```

### Step 2: Clone This Repository

```bash
cd ~/chromium
git clone https://github.com/YOUR_USERNAME/ungoogled-chromium-ai.git
cd ungoogled-chromium-ai
git checkout claude/add-ai-sidebar-chat-01Pzn9zdhMpCtuSGx4GcqT33
```

### Step 3: Apply Patches to Chromium Source

```bash
# Copy AI sidebar component to Chromium source
cp -r component-source/ai_sidebar ~/chromium/src/components/

# Apply ungoogled-chromium patches
cd ~/chromium/ungoogled-chromium-ai
python3 -m buildkit patches apply ~/chromium/src patches/series
```

### Step 4: Configure Build

Create `~/chromium/src/out/Default/args.gn`:

```gn
# ungoogled-chromium build flags
is_official_build = true
is_debug = false
symbol_level = 0
enable_nacl = false
enable_widevine = true
safe_browsing_mode = 0

# Enable component build for faster iteration (optional)
is_component_build = true

# macOS specific (if on macOS)
# target_cpu = "arm64"  # For Apple Silicon
# target_cpu = "x64"    # For Intel Mac

# Enable AI sidebar component
# (This would be added to the build configuration)
```

### Step 5: Generate Build Files

```bash
cd ~/chromium/src
gn gen out/Default
```

### Step 6: Build Chromium

```bash
# Full build (takes 1-2 hours on first build)
autoninja -C out/Default chrome

# For incremental builds after changes:
autoninja -C out/Default chrome
```

## Running ungoogled-chromium with AI Sidebar

### Launch the Browser

```bash
cd ~/chromium/src
out/Default/Chromium.app/Contents/MacOS/Chromium  # macOS
# OR
out/Default/chrome  # Linux
# OR
out\Default\chrome.exe  # Windows
```

### Enable the AI Sidebar

1. Open Chromium
2. Navigate to `chrome://flags`
3. Search for "AI Sidebar"
4. Enable the flag
5. Restart the browser

### Access the Sidebar

Click the AI icon in the toolbar or use the keyboard shortcut to open the sidebar.

## Configuring AI Backends

### 1. Configure Ollama (Local)

In the AI Sidebar:
1. Click the settings icon (⚙️)
2. Set "Ollama Endpoint" to `http://localhost:11434` (default)
3. Click "Save Settings"
4. Click the refresh icon to load available models

### 2. Configure OpenRouter (Cloud)

In the AI Sidebar:
1. Click the settings icon (⚙️)
2. Enter your OpenRouter API key
3. Click "Save Settings"
4. Click the refresh icon to load available models

### 3. Configure MLX (macOS Only)

In the AI Sidebar:
1. Click the settings icon (⚙️)
2. Set "MLX Model Directory" to your Hugging Face cache directory
   - Default: `~/.cache/huggingface/hub`
3. Click "Save Settings"

To download MLX models:

```bash
# Example: Download a Llama model converted for MLX
huggingface-cli download mlx-community/Llama-3-8B-Instruct-4bit

# For vision models:
huggingface-cli download mlx-community/llava-1.5-7b-4bit
```

## Using the AI Sidebar

### Basic Chat

1. **Select a Model**: Use the searchable dropdown to choose a model
   - MLX models (🍎): Local models on Apple Silicon
   - Ollama models (🦙): Local models via Ollama
   - OpenRouter models (☁️): Cloud models

2. **Type Your Message**: Enter your question or prompt

3. **Options**:
   - ☑ **Use current page as context**: Includes the current page's text in the prompt
   - ☑ **Enable web search**: Allows the AI to search the web (opens tabs as needed)
   - ☑ **Include page screenshot**: Attaches a screenshot (for vision models)

4. **Send**: Click the send button or press Enter

### Advanced Features

#### Switch Models Per Message

You can select a different model for each message in the conversation. This lets you:
- Use a fast model for simple questions
- Switch to a powerful model for complex reasoning
- Use vision models when you need image understanding

#### Vision Models

For models that support vision (marked with "Vision" badge):
1. Enable "Include page screenshot" to send the current page
2. Or click the image attachment button to upload an image
3. Ask questions about the image

#### Web Search Integration

When "Enable web search" is checked:
- The AI can request web searches
- Search results open in new tabs
- Results are provided as context to the AI

## Development and Debugging

### Incremental Development

After making changes to the AI sidebar code:

```bash
# Rebuild only the affected targets
cd ~/chromium/src
autoninja -C out/Default components/ai_sidebar

# Launch with logging
out/Default/chrome --enable-logging --v=1
```

### Viewing Logs

```bash
# macOS/Linux
tail -f ~/Library/Application\ Support/Chromium/chrome_debug.log  # macOS
tail -f ~/.config/chromium/chrome_debug.log  # Linux

# Windows
# Check: %LOCALAPPDATA%\Chromium\User Data\chrome_debug.log
```

### Debugging WebUI

1. Open the AI sidebar
2. Right-click in the sidebar
3. Select "Inspect"
4. Use Chrome DevTools to debug JavaScript and CSS

### Testing Backends

Test each backend independently:

```bash
# Test Ollama
curl http://localhost:11434/api/tags

# Test MLX (macOS)
python3 -c "import mlx.core as mx; import mlx_lm; print('MLX OK')"
```

## Performance Optimization

### For Faster Builds

```gn
# In args.gn
is_component_build = true  # Faster incremental builds
use_goma = true           # If you have Goma access
```

### For Smaller Binary

```gn
# In args.gn
is_official_build = true
symbol_level = 0
enable_nacl = false
```

## Troubleshooting

### Build Failures

1. **Missing dependencies**
   ```bash
   # Run install-build-deps again
   sudo ./build/install-build-deps.sh  # Linux
   ```

2. **GN errors**
   ```bash
   # Clean and regenerate
   rm -rf out/Default
   gn gen out/Default
   ```

### Ollama Connection Failed

```bash
# Check if Ollama is running
curl http://localhost:11434/api/tags

# Start Ollama if needed
ollama serve
```

### MLX Models Not Found (macOS)

```bash
# Check model directory
ls ~/.cache/huggingface/hub

# Set correct path in settings
# Or specify custom directory
```

### OpenRouter API Errors

1. Verify your API key is correct
2. Check your account credits at openrouter.ai
3. Ensure you're using the correct model IDs

## Known Limitations

1. **iOS Support**: Not available - ungoogled-chromium is desktop-only
2. **MLX Requirements**: Apple Silicon Mac required for MLX models
3. **Streaming**: Partial support - some models may not stream responses
4. **Model Discovery**: MLX models require manual installation via Hugging Face

## Architecture Notes

The AI sidebar is implemented as a Chromium component with:
- **Backend Layer**: Abstract interfaces for different AI providers
- **Model Manager**: Unified model discovery and request routing
- **WebUI Layer**: HTML/CSS/JS interface
- **Message Handler**: Bridges WebUI and C++ backend

Key files:
- `components/ai_sidebar/backend/`: AI backend implementations
- `components/ai_sidebar/ui/webui/`: WebUI interface
- `components/ai_sidebar/BUILD.gn`: Build configuration

## Contributing

When making changes:
1. Follow Chromium C++ style guide
2. Test on multiple platforms if possible
3. Update this documentation
4. Create patches for your changes

## Resources

- [Chromium Development](https://www.chromium.org/developers/)
- [ungoogled-chromium](https://github.com/ungoogled-software/ungoogled-chromium)
- [MLX Documentation](https://ml-explore.github.io/mlx/build/html/index.html)
- [Ollama Documentation](https://ollama.ai/docs)
- [OpenRouter API Docs](https://openrouter.ai/docs)

## License

This code is licensed under the BSD-style license found in the LICENSE file.
