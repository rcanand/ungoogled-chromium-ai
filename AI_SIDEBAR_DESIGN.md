# AI Sidebar Design Document

## Overview

An integrated AI chat sidebar for ungoogled-chromium supporting multiple AI backends with web search integration and page context awareness.

## Important Note: iOS Support

**ungoogled-chromium is desktop-only** (Linux, Windows, macOS). iOS is not supported in this repository. This implementation will work on:
- **macOS**: Full support including MLX models (text, image, audio)
- **Linux/Windows**: Ollama and OpenRouter support
- **iOS**: Not available (ungoogled-chromium doesn't have iOS builds)

For macOS users, this provides the Apple ecosystem integration via MLX.

## Architecture

### Component Structure

```
components/ai_sidebar/
├── backend/
│   ├── ai_backend_interface.h           # Abstract interface for AI backends
│   ├── mlx_backend.cc/h                 # MLX integration (macOS only)
│   ├── ollama_backend.cc/h              # Ollama integration (all platforms)
│   ├── openrouter_backend.cc/h          # OpenRouter API integration
│   ├── model_manager.cc/h               # Model discovery and management
│   └── web_search_service.cc/h          # Web search integration
├── ui/
│   └── webui/
│       ├── ai_sidebar_ui.cc/h           # WebUI configuration
│       ├── ai_sidebar_page_handler.cc/h # Backend message handler
│       └── resources/
│           ├── ai_sidebar.html          # Main UI
│           ├── ai_sidebar.css           # Styles
│           ├── ai_sidebar.js            # Frontend logic
│           └── model_selector.js        # Model dropdown component
├── common/
│   ├── ai_sidebar_prefs.cc/h            # Preferences storage
│   └── message_types.h                  # Message type definitions
└── BUILD.gn
```

### Backend Interfaces

#### 1. MLX Backend (macOS only)

```cpp
class MLXBackend : public AIBackendInterface {
 public:
  // Text generation using MLX
  void GenerateText(const std::string& prompt,
                   const std::string& model,
                   ResultCallback callback);

  // Image understanding using MLX vision models
  void AnalyzeImage(const std::vector<uint8_t>& image_data,
                   const std::string& prompt,
                   const std::string& model,
                   ResultCallback callback);

  // Audio transcription/understanding
  void ProcessAudio(const std::vector<uint8_t>& audio_data,
                   const std::string& model,
                   ResultCallback callback);

  // List available MLX models
  std::vector<ModelInfo> ListModels();
};
```

Models discovered from:
- `~/.cache/huggingface/hub/` (downloaded models)
- User-configured MLX model paths

#### 2. Ollama Backend (all platforms)

```cpp
class OllamaBackend : public AIBackendInterface {
 public:
  // Connect to local Ollama instance (default: localhost:11434)
  bool Connect(const std::string& endpoint);

  // Generate text using Ollama models
  void GenerateText(const std::string& prompt,
                   const std::string& model,
                   ResultCallback callback);

  // List available Ollama models via API
  std::vector<ModelInfo> ListModels();

  // Stream response support
  void GenerateTextStreaming(const std::string& prompt,
                            const std::string& model,
                            StreamCallback callback);
};
```

API endpoint: `http://localhost:11434/api/*`

#### 3. OpenRouter Backend (all platforms)

```cpp
class OpenRouterBackend : public AIBackendInterface {
 public:
  // Initialize with API key
  void SetAPIKey(const std::string& api_key);

  // Generate text using OpenRouter models
  void GenerateText(const std::string& prompt,
                   const std::string& model,
                   ResultCallback callback);

  // Fetch available models from OpenRouter
  std::vector<ModelInfo> FetchModels();

  // Support for vision models
  void GenerateWithImage(const std::string& prompt,
                        const std::vector<uint8_t>& image_data,
                        const std::string& model,
                        ResultCallback callback);
};
```

API endpoint: `https://openrouter.ai/api/v1/*`

### Model Manager

```cpp
class ModelManager {
 public:
  // Aggregate models from all backends
  std::vector<ModelInfo> GetAllAvailableModels();

  // Filter models by capability (text, image, audio)
  std::vector<ModelInfo> FilterByCapability(ModelCapability cap);

  // Search models by name/provider
  std::vector<ModelInfo> SearchModels(const std::string& query);

  // Get model details
  ModelInfo GetModelInfo(const std::string& model_id);

  // Refresh model lists from all backends
  void RefreshModels();
};

struct ModelInfo {
  std::string id;              // e.g., "mlx:llama-3-8b"
  std::string display_name;     // e.g., "Llama 3 8B (MLX)"
  std::string provider;         // "mlx", "ollama", "openrouter"
  ModelCapability capabilities; // TEXT, IMAGE, AUDIO
  size_t context_length;
  std::string description;
};
```

### Web Search Integration

```cpp
class WebSearchService {
 public:
  // Perform web search using browser's search engine
  void Search(const std::string& query, SearchCallback callback);

  // Open search results in new tabs
  void OpenSearchResultsInTabs(const std::vector<std::string>& urls,
                               Browser* browser);

  // Extract search results as context
  std::string ExtractSearchResults(const std::string& query);
};
```

Uses Chromium's existing search engine configuration and tab management.

### Page Context Extraction

```cpp
class PageContextExtractor {
 public:
  // Extract visible text from current page
  std::string ExtractPageText(content::WebContents* web_contents);

  // Extract page metadata (title, URL, description)
  PageMetadata ExtractMetadata(content::WebContents* web_contents);

  // Extract selected text if available
  std::string ExtractSelection(content::WebContents* web_contents);

  // Capture screenshot of current page
  std::vector<uint8_t> CaptureScreenshot(content::WebContents* web_contents);
};
```

## UI Design

### Sidebar Layout

```
┌─────────────────────────────────────┐
│ AI Chat                        [×]  │
├─────────────────────────────────────┤
│                                     │
│  Chat Messages                      │
│  ┌─────────────────────────────┐   │
│  │ User: Hello                 │   │
│  │ Assistant: Hi there!        │   │
│  └─────────────────────────────┘   │
│                                     │
├─────────────────────────────────────┤
│ [Search icon] Search models...     │
│ ┌─────────────────────────────────┐│
│ │ 📦 MLX Models                   ││
│ │   • Llama 3 8B (Text)           ││
│ │   • LLaVA (Vision)              ││
│ │ 🐋 Ollama Models                ││
│ │   • llama2 (Text)               ││
│ │   • codellama (Text)            ││
│ │ 🌐 OpenRouter Models            ││
│ │   • GPT-4 (Text)                ││
│ │   • Claude 3 (Text, Vision)     ││
│ └─────────────────────────────────┘│
│                                     │
│ ☑ Use current page as context      │
│ ☑ Enable web search                │
│                                     │
│ ┌─────────────────────────────────┐│
│ │ Type your message...            ││
│ └─────────────────────────────────┘│
│                            [Send]   │
└─────────────────────────────────────┘
```

### Features

1. **Per-Message Model Selection**: User can change model for each message
2. **Searchable Dropdown**: Fuzzy search across all available models
3. **Model Grouping**: Group by provider (MLX, Ollama, OpenRouter)
4. **Context Toggle**: Enable/disable page context per message
5. **Web Search**: Automatic web search when needed
6. **Multi-Modal Support**:
   - Attach images for vision models
   - Record audio for audio models
   - Text input for all models

### WebUI Implementation

**HTML Structure** (`ai_sidebar.html`):
```html
<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="ai_sidebar.css">
  <script type="module" src="ai_sidebar.js"></script>
</head>
<body>
  <div id="ai-sidebar">
    <div class="header">
      <h2>AI Chat</h2>
      <button id="close-btn">×</button>
    </div>

    <div id="chat-container">
      <!-- Messages rendered here -->
    </div>

    <div class="model-selector">
      <input type="text" id="model-search" placeholder="Search models...">
      <div id="model-dropdown" class="hidden">
        <!-- Populated dynamically -->
      </div>
    </div>

    <div class="options">
      <label>
        <input type="checkbox" id="use-page-context">
        Use current page as context
      </label>
      <label>
        <input type="checkbox" id="enable-web-search">
        Enable web search
      </label>
    </div>

    <div class="input-area">
      <textarea id="message-input" placeholder="Type your message..."></textarea>
      <button id="send-btn">Send</button>
    </div>
  </div>
</body>
</html>
```

**JavaScript Logic** (`ai_sidebar.js`):
```javascript
class AISidebar {
  constructor() {
    this.selectedModel = null;
    this.models = [];
    this.chatHistory = [];

    this.init();
  }

  async init() {
    await this.loadModels();
    this.setupEventListeners();
    this.renderModelDropdown();
  }

  async loadModels() {
    // Request models from backend
    const response = await chrome.send('getAvailableModels');
    this.models = response.models;
  }

  async sendMessage(message) {
    const context = document.getElementById('use-page-context').checked
      ? await this.getPageContext()
      : null;

    const webSearch = document.getElementById('enable-web-search').checked;

    const response = await chrome.send('sendMessage', {
      message: message,
      model: this.selectedModel,
      context: context,
      webSearch: webSearch
    });

    this.appendMessage('assistant', response.text);
  }

  filterModels(query) {
    return this.models.filter(model =>
      model.display_name.toLowerCase().includes(query.toLowerCase()) ||
      model.provider.toLowerCase().includes(query.toLowerCase())
    );
  }
}
```

## Integration Points

### 1. Browser View Integration

Modify `chrome/browser/ui/views/frame/browser_view.cc`:
```cpp
void BrowserView::AddedToWidget() {
  // Existing code...

  // Add AI sidebar
  ai_sidebar_ = AddChildView(std::make_unique<AISidebarView>(browser_.get()));
  ai_sidebar_->SetVisible(false); // Hidden by default
}
```

### 2. Toolbar Button

Add toggle button in `chrome/browser/ui/views/toolbar/toolbar_view.cc`:
```cpp
void ToolbarView::Init() {
  // Existing code...

  ai_sidebar_button_ = AddChildView(std::make_unique<ToolbarButton>(
      base::BindRepeating(&ToolbarView::OnAISidebarButtonPressed,
                         base::Unretained(this))));
  ai_sidebar_button_->SetTooltipText(u"Toggle AI Chat");
}
```

### 3. Command Registration

Add command in `chrome/browser/ui/browser_command_controller.cc`:
```cpp
void BrowserCommandController::InitCommandState() {
  // Existing code...

  command_updater_.UpdateCommandEnabled(IDC_TOGGLE_AI_SIDEBAR, true);
}
```

### 4. Preferences

Store in `chrome/browser/prefs/browser_prefs.cc`:
```cpp
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Existing code...

  registry->RegisterStringPref(prefs::kAISidebarSelectedModel, "");
  registry->RegisterStringPref(prefs::kAISidebarOpenRouterAPIKey, "");
  registry->RegisterStringPref(prefs::kAISidebarOllamaEndpoint,
                              "http://localhost:11434");
  registry->RegisterBooleanPref(prefs::kAISidebarUsePageContext, true);
  registry->RegisterBooleanPref(prefs::kAISidebarEnableWebSearch, true);
}
```

## Build Configuration

### MLX Dependencies (macOS only)

```gn
if (is_mac) {
  deps += [
    "//third_party/mlx",
  ]
}
```

### Network Dependencies (all platforms)

```gn
deps += [
  "//services/network/public/cpp",
  "//net",
]
```

## Security Considerations

1. **API Key Storage**: Store OpenRouter API keys in encrypted preferences
2. **Network Isolation**: Use Chromium's network service for all requests
3. **Content Security Policy**: Restrict WebUI to prevent XSS
4. **Model Validation**: Validate model paths before loading
5. **User Consent**: Require explicit consent for page context sharing

## Testing Strategy

1. **Unit Tests**: Test each backend independently
2. **Integration Tests**: Test model discovery and selection
3. **UI Tests**: Test WebUI interaction flows
4. **Platform Tests**: Test MLX on macOS, Ollama on all platforms
5. **Performance Tests**: Test large context handling

## Implementation Phases

### Phase 1: Core Infrastructure
- Component structure
- Backend interfaces
- Model manager
- Basic WebUI

### Phase 2: Backend Implementations
- Ollama integration
- OpenRouter integration
- MLX integration (macOS)

### Phase 3: UI Features
- Model selector
- Chat interface
- Page context extraction

### Phase 4: Advanced Features
- Web search integration
- Multi-modal support (images, audio)
- Streaming responses

### Phase 5: Polish
- Error handling
- Preferences UI
- Documentation

## File Structure Summary

```
components/ai_sidebar/                     # New component
chrome/browser/ui/views/sidebar/          # UI views
chrome/browser/resources/ai_sidebar/      # WebUI resources
chrome/browser/ui/webui/ai_sidebar_ui.cc  # WebUI handler
patches/extra/ungoogled-chromium/         # New patches:
  - add-ai-sidebar-component.patch
  - add-ai-sidebar-ui.patch
  - add-ai-sidebar-toolbar-button.patch
  - add-ai-sidebar-webui.patch
```

## Next Steps

1. Create component directory structure
2. Implement backend interfaces
3. Create WebUI resources
4. Generate patches for all changes
5. Update build configuration
6. Create documentation for building and running
