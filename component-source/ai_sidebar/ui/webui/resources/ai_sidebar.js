// AI Sidebar JavaScript

class AISidebar {
  constructor() {
    this.models = [];
    this.selectedModel = null;
    this.chatHistory = [];
    this.isLoading = false;
    this.attachedImage = null;

    this.init();
  }

  async init() {
    this.setupEventListeners();
    await this.loadSettings();
    await this.loadModels();
  }

  setupEventListeners() {
    // Model search and selection
    const modelSearch = document.getElementById('model-search');
    modelSearch.addEventListener('input', (e) => this.handleModelSearch(e));
    modelSearch.addEventListener('focus', () => this.showModelDropdown());
    modelSearch.addEventListener('blur', () => {
      // Delay to allow click events on dropdown items
      setTimeout(() => this.hideModelDropdown(), 200);
    });

    // Send message
    const sendBtn = document.getElementById('send-btn');
    const messageInput = document.getElementById('message-input');
    sendBtn.addEventListener('click', () => this.sendMessage());
    messageInput.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) {
        e.preventDefault();
        this.sendMessage();
      }
    });

    // Image attachment
    const attachImageBtn = document.getElementById('attach-image-btn');
    const imageInput = document.getElementById('image-input');
    attachImageBtn.addEventListener('click', () => imageInput.click());
    imageInput.addEventListener('change', (e) => this.handleImageAttachment(e));

    // Settings
    const settingsBtn = document.getElementById('settings-btn');
    const closeSettingsBtn = document.getElementById('close-settings-btn');
    const saveSettingsBtn = document.getElementById('save-settings-btn');
    settingsBtn.addEventListener('click', () => this.showSettings());
    closeSettingsBtn.addEventListener('click', () => this.hideSettings());
    saveSettingsBtn.addEventListener('click', () => this.saveSettings());

    // Refresh models
    const refreshBtn = document.getElementById('refresh-models-btn');
    refreshBtn.addEventListener('click', () => this.loadModels(true));
  }

  async loadSettings() {
    try {
      const settings = await chrome.send('getSettings');
      if (settings) {
        document.getElementById('openrouter-api-key').value =
            settings.openRouterApiKey || '';
        document.getElementById('ollama-endpoint').value =
            settings.ollamaEndpoint || 'http://localhost:11434';
        document.getElementById('mlx-model-dir').value =
            settings.mlxModelDir || '';
      }
    } catch (error) {
      console.error('Failed to load settings:', error);
    }
  }

  async saveSettings() {
    const settings = {
      openRouterApiKey: document.getElementById('openrouter-api-key').value,
      ollamaEndpoint: document.getElementById('ollama-endpoint').value,
      mlxModelDir: document.getElementById('mlx-model-dir').value,
    };

    try {
      await chrome.send('saveSettings', settings);
      this.hideSettings();
      // Reload models with new settings
      await this.loadModels(true);
    } catch (error) {
      console.error('Failed to save settings:', error);
      alert('Failed to save settings. Please try again.');
    }
  }

  showSettings() {
    document.getElementById('settings-modal').classList.remove('hidden');
  }

  hideSettings() {
    document.getElementById('settings-modal').classList.add('hidden');
  }

  async loadModels(refresh = false) {
    try {
      const response = await chrome.send('getAvailableModels', {refresh});
      this.models = response.models || [];
      this.renderModelDropdown();

      // Select first model if none selected
      if (!this.selectedModel && this.models.length > 0) {
        this.selectModel(this.models[0]);
      }
    } catch (error) {
      console.error('Failed to load models:', error);
      this.showError('Failed to load models. Please check your settings.');
    }
  }

  renderModelDropdown() {
    const dropdown = document.getElementById('model-dropdown');
    dropdown.innerHTML = '';

    if (this.models.length === 0) {
      dropdown.innerHTML = '<div class="model-item">No models available</div>';
      return;
    }

    // Group models by provider
    const groups = this.groupModelsByProvider(this.models);

    for (const [provider, models] of Object.entries(groups)) {
      const group = document.createElement('div');
      group.className = 'model-group';

      const header = document.createElement('div');
      header.className = 'model-group-header';
      header.textContent = this.getProviderLabel(provider);
      group.appendChild(header);

      models.forEach((model) => {
        const item = this.createModelItem(model);
        group.appendChild(item);
      });

      dropdown.appendChild(group);
    }
  }

  groupModelsByProvider(models) {
    const groups = {};
    models.forEach((model) => {
      const provider = model.provider || 'other';
      if (!groups[provider]) {
        groups[provider] = [];
      }
      groups[provider].push(model);
    });
    return groups;
  }

  getProviderLabel(provider) {
    const labels = {
      mlx: '📦 MLX Models (Local)',
      ollama: '🐋 Ollama Models (Local)',
      openrouter: '🌐 OpenRouter Models (Cloud)',
    };
    return labels[provider] || provider;
  }

  createModelItem(model) {
    const item = document.createElement('div');
    item.className = 'model-item';
    if (this.selectedModel && this.selectedModel.id === model.id) {
      item.classList.add('selected');
    }

    const icon = document.createElement('div');
    icon.className = 'model-icon';
    icon.textContent = this.getModelIcon(model);
    item.appendChild(icon);

    const info = document.createElement('div');
    info.className = 'model-info';

    const name = document.createElement('div');
    name.className = 'model-name';
    name.textContent = model.display_name;
    info.appendChild(name);

    if (model.description) {
      const description = document.createElement('div');
      description.className = 'model-description';
      description.textContent = model.description;
      info.appendChild(description);
    }

    const capabilities = document.createElement('div');
    capabilities.className = 'model-capabilities';
    if (model.capabilities & 1) {  // TEXT
      const badge = document.createElement('span');
      badge.className = 'capability-badge';
      badge.textContent = 'Text';
      capabilities.appendChild(badge);
    }
    if (model.capabilities & 2) {  // IMAGE
      const badge = document.createElement('span');
      badge.className = 'capability-badge';
      badge.textContent = 'Vision';
      capabilities.appendChild(badge);
    }
    if (model.capabilities & 4) {  // AUDIO
      const badge = document.createElement('span');
      badge.className = 'capability-badge';
      badge.textContent = 'Audio';
      capabilities.appendChild(badge);
    }
    info.appendChild(capabilities);

    item.appendChild(info);

    item.addEventListener('click', () => this.selectModel(model));

    return item;
  }

  getModelIcon(model) {
    if (model.provider === 'mlx') return '🍎';
    if (model.provider === 'ollama') return '🦙';
    if (model.provider === 'openrouter') return '☁️';
    return '🤖';
  }

  selectModel(model) {
    this.selectedModel = model;
    document.getElementById('selected-model-name').textContent =
        model.display_name;
    document.getElementById('model-search').value = '';
    this.hideModelDropdown();
    this.renderModelDropdown();  // Update selected state
  }

  handleModelSearch(event) {
    const query = event.target.value.toLowerCase();
    const dropdown = document.getElementById('model-dropdown');

    if (!query) {
      this.renderModelDropdown();
      return;
    }

    const filtered = this.models.filter((model) => {
      return (
          model.display_name.toLowerCase().includes(query) ||
          model.provider.toLowerCase().includes(query) ||
          (model.description && model.description.toLowerCase().includes(query))
      );
    });

    dropdown.innerHTML = '';

    if (filtered.length === 0) {
      dropdown.innerHTML =
          '<div class="model-item">No models match your search</div>';
      return;
    }

    const groups = this.groupModelsByProvider(filtered);
    for (const [provider, models] of Object.entries(groups)) {
      const group = document.createElement('div');
      group.className = 'model-group';

      const header = document.createElement('div');
      header.className = 'model-group-header';
      header.textContent = this.getProviderLabel(provider);
      group.appendChild(header);

      models.forEach((model) => {
        const item = this.createModelItem(model);
        group.appendChild(item);
      });

      dropdown.appendChild(group);
    }
  }

  showModelDropdown() {
    document.getElementById('model-dropdown').classList.remove('hidden');
  }

  hideModelDropdown() {
    document.getElementById('model-dropdown').classList.add('hidden');
  }

  async sendMessage() {
    if (this.isLoading) return;

    const messageInput = document.getElementById('message-input');
    const message = messageInput.value.trim();

    if (!message && !this.attachedImage) return;

    if (!this.selectedModel) {
      alert('Please select a model first');
      return;
    }

    this.isLoading = true;
    messageInput.value = '';

    // Add user message to UI
    this.appendMessage('user', message, this.selectedModel.display_name);

    // Show loading indicator
    this.showLoadingIndicator();

    try {
      // Prepare request
      const request = {
        message: message,
        model: this.selectedModel.id,
        usePageContext: document.getElementById('use-page-context').checked,
        enableWebSearch: document.getElementById('enable-web-search').checked,
        includeScreenshot: document.getElementById('include-screenshot').checked,
      };

      if (this.attachedImage) {
        request.image = this.attachedImage;
        this.attachedImage = null;
      }

      // Send to backend
      const response = await chrome.send('sendMessage', request);

      this.hideLoadingIndicator();

      if (response.success) {
        this.appendMessage('assistant', response.text, this.selectedModel.display_name);
        this.chatHistory.push({
          role: 'user',
          content: message,
          model: this.selectedModel.id,
        });
        this.chatHistory.push({
          role: 'assistant',
          content: response.text,
          model: this.selectedModel.id,
        });
      } else {
        this.showError(response.error || 'Failed to generate response');
      }
    } catch (error) {
      this.hideLoadingIndicator();
      console.error('Failed to send message:', error);
      this.showError('Failed to send message. Please try again.');
    }

    this.isLoading = false;
  }

  appendMessage(role, text, modelName) {
    const messagesContainer = document.getElementById('chat-messages');

    // Remove welcome message if present
    const welcomeMsg = messagesContainer.querySelector('.welcome-message');
    if (welcomeMsg) {
      welcomeMsg.remove();
    }

    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${role}`;

    const bubble = document.createElement('div');
    bubble.className = 'message-bubble';
    bubble.textContent = text;

    if (role === 'assistant') {
      const meta = document.createElement('div');
      meta.className = 'message-meta';

      const badge = document.createElement('span');
      badge.className = 'model-badge';
      badge.textContent = modelName;

      meta.appendChild(badge);
      bubble.appendChild(meta);
    }

    messageDiv.appendChild(bubble);
    messagesContainer.appendChild(messageDiv);

    // Scroll to bottom
    messagesContainer.scrollTop = messagesContainer.scrollHeight;
  }

  showLoadingIndicator() {
    const messagesContainer = document.getElementById('chat-messages');

    const loadingDiv = document.createElement('div');
    loadingDiv.className = 'message assistant';
    loadingDiv.id = 'loading-indicator';

    const bubble = document.createElement('div');
    bubble.className = 'message-bubble';

    const indicator = document.createElement('div');
    indicator.className = 'loading-indicator';
    indicator.innerHTML = `
      <div class="loading-dot"></div>
      <div class="loading-dot"></div>
      <div class="loading-dot"></div>
    `;

    bubble.appendChild(indicator);
    loadingDiv.appendChild(bubble);
    messagesContainer.appendChild(loadingDiv);

    messagesContainer.scrollTop = messagesContainer.scrollHeight;
  }

  hideLoadingIndicator() {
    const indicator = document.getElementById('loading-indicator');
    if (indicator) {
      indicator.remove();
    }
  }

  showError(message) {
    this.appendMessage('assistant', `Error: ${message}`, 'System');
  }

  handleImageAttachment(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      this.attachedImage = e.target.result;
      // Show visual feedback
      alert(`Image attached: ${file.name}`);
    };
    reader.readAsDataURL(file);
  }
}

// Initialize the sidebar when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', () => {
    new AISidebar();
  });
} else {
  new AISidebar();
}

// Mock chrome.send for development
if (typeof chrome === 'undefined' || !chrome.send) {
  window.chrome = {
    send: async (method, args) => {
      console.log('chrome.send:', method, args);

      // Mock responses
      if (method === 'getSettings') {
        return {
          openRouterApiKey: '',
          ollamaEndpoint: 'http://localhost:11434',
          mlxModelDir: '',
        };
      }

      if (method === 'getAvailableModels') {
        return {
          models: [
            {
              id: 'ollama:llama2',
              display_name: 'Llama 2',
              provider: 'ollama',
              capabilities: 1,
              context_length: 4096,
              description: 'Local Llama 2 model',
            },
            {
              id: 'openrouter:anthropic/claude-3-opus',
              display_name: 'Claude 3 Opus',
              provider: 'openrouter',
              capabilities: 3,
              context_length: 200000,
              description: 'Most capable Claude model',
            },
          ],
        };
      }

      if (method === 'sendMessage') {
        return new Promise((resolve) => {
          setTimeout(() => {
            resolve({
              success: true,
              text: 'This is a mock response from the AI.',
            });
          }, 1000);
        });
      }

      return {};
    },
  };
}
