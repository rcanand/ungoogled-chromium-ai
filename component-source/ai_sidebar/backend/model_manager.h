// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_MODEL_MANAGER_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_MODEL_MANAGER_H_

#include <memory>
#include <vector>

#include "components/ai_sidebar/backend/ai_backend_interface.h"
#include "base/memory/weak_ptr.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ai_sidebar {

class MLXBackend;
class OllamaBackend;
class OpenRouterBackend;

// Manages all AI backends and provides unified model access
class ModelManager {
 public:
  explicit ModelManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ModelManager();

  // Initialize all backends
  void Initialize();

  // Get all available models from all backends
  void GetAllModels(ModelsCallback callback);

  // Generate text using specified model
  void GenerateText(const GenerationRequest& request,
                   GenerationCallback callback);

  // Generate text with streaming
  void GenerateTextStreaming(const GenerationRequest& request,
                            StreamCallback stream_callback,
                            GenerationCallback complete_callback);

  // Filter models by capability
  std::vector<ModelInfo> FilterByCapability(
      const std::vector<ModelInfo>& models,
      ModelCapability capability);

  // Search models by query string
  std::vector<ModelInfo> SearchModels(const std::vector<ModelInfo>& models,
                                     const std::string& query);

  // Backend accessors for configuration
  OllamaBackend* ollama_backend() { return ollama_backend_.get(); }
  OpenRouterBackend* openrouter_backend() {
    return openrouter_backend_.get();
  }
#if BUILDFLAG(IS_MAC)
  MLXBackend* mlx_backend() { return mlx_backend_.get(); }
#endif

 private:
  void OnBackendModelsReady(ModelsCallback callback,
                           std::vector<ModelInfo> all_models,
                           int pending_backends,
                           std::vector<ModelInfo> backend_models);

  AIBackendInterface* GetBackendForModel(const std::string& model_id);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<OllamaBackend> ollama_backend_;
  std::unique_ptr<OpenRouterBackend> openrouter_backend_;
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<MLXBackend> mlx_backend_;
#endif

  base::WeakPtrFactory<ModelManager> weak_factory_{this};
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_MODEL_MANAGER_H_
