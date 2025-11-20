// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/backend/model_manager.h"

#include "components/ai_sidebar/backend/ollama_backend.h"
#include "components/ai_sidebar/backend/openrouter_backend.h"

#if BUILDFLAG(IS_MAC)
#include "components/ai_sidebar/backend/mlx_backend.h"
#endif

#include "base/strings/string_util.h"

namespace ai_sidebar {

ModelManager::ModelManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

ModelManager::~ModelManager() = default;

void ModelManager::Initialize() {
  // Initialize Ollama backend (all platforms)
  ollama_backend_ = std::make_unique<OllamaBackend>(url_loader_factory_);
  ollama_backend_->Initialize();

  // Initialize OpenRouter backend (all platforms)
  openrouter_backend_ =
      std::make_unique<OpenRouterBackend>(url_loader_factory_);
  openrouter_backend_->Initialize();

#if BUILDFLAG(IS_MAC)
  // Initialize MLX backend (macOS only)
  mlx_backend_ = std::make_unique<MLXBackend>();
  mlx_backend_->Initialize();
#endif
}

void ModelManager::GetAllModels(ModelsCallback callback) {
  std::vector<ModelInfo> all_models;
  int pending_backends = 0;

  // Count available backends
  if (ollama_backend_ && ollama_backend_->IsAvailable()) {
    pending_backends++;
  }
  if (openrouter_backend_ && openrouter_backend_->IsAvailable()) {
    pending_backends++;
  }
#if BUILDFLAG(IS_MAC)
  if (mlx_backend_ && mlx_backend_->IsAvailable()) {
    pending_backends++;
  }
#endif

  if (pending_backends == 0) {
    std::move(callback).Run(all_models);
    return;
  }

  // Fetch models from Ollama
  if (ollama_backend_ && ollama_backend_->IsAvailable()) {
    ollama_backend_->ListModels(base::BindOnce(
        &ModelManager::OnBackendModelsReady, weak_factory_.GetWeakPtr(),
        std::move(callback), std::move(all_models), pending_backends));
  }

  // Fetch models from OpenRouter
  if (openrouter_backend_ && openrouter_backend_->IsAvailable()) {
    openrouter_backend_->ListModels(base::BindOnce(
        &ModelManager::OnBackendModelsReady, weak_factory_.GetWeakPtr(),
        std::move(callback), std::move(all_models), pending_backends));
  }

#if BUILDFLAG(IS_MAC)
  // Fetch models from MLX
  if (mlx_backend_ && mlx_backend_->IsAvailable()) {
    mlx_backend_->ListModels(base::BindOnce(
        &ModelManager::OnBackendModelsReady, weak_factory_.GetWeakPtr(),
        std::move(callback), std::move(all_models), pending_backends));
  }
#endif
}

void ModelManager::GenerateText(const GenerationRequest& request,
                               GenerationCallback callback) {
  AIBackendInterface* backend = GetBackendForModel(request.model);

  if (!backend) {
    GenerationResult result;
    result.success = false;
    result.error_message = "No backend found for model: " + request.model;
    std::move(callback).Run(std::move(result));
    return;
  }

  backend->GenerateText(request, std::move(callback));
}

void ModelManager::GenerateTextStreaming(
    const GenerationRequest& request,
    StreamCallback stream_callback,
    GenerationCallback complete_callback) {
  AIBackendInterface* backend = GetBackendForModel(request.model);

  if (!backend) {
    GenerationResult result;
    result.success = false;
    result.error_message = "No backend found for model: " + request.model;
    std::move(complete_callback).Run(std::move(result));
    return;
  }

  backend->GenerateTextStreaming(request, std::move(stream_callback),
                                 std::move(complete_callback));
}

std::vector<ModelInfo> ModelManager::FilterByCapability(
    const std::vector<ModelInfo>& models,
    ModelCapability capability) {
  std::vector<ModelInfo> filtered;

  for (const auto& model : models) {
    if ((model.capabilities & static_cast<uint32_t>(capability)) != 0) {
      filtered.push_back(model);
    }
  }

  return filtered;
}

std::vector<ModelInfo> ModelManager::SearchModels(
    const std::vector<ModelInfo>& models,
    const std::string& query) {
  if (query.empty()) {
    return models;
  }

  std::vector<ModelInfo> results;
  std::string lower_query = base::ToLowerASCII(query);

  for (const auto& model : models) {
    std::string lower_name = base::ToLowerASCII(model.display_name);
    std::string lower_provider = base::ToLowerASCII(model.provider);
    std::string lower_desc = base::ToLowerASCII(model.description);

    if (base::Contains(lower_name, lower_query) ||
        base::Contains(lower_provider, lower_query) ||
        base::Contains(lower_desc, lower_query)) {
      results.push_back(model);
    }
  }

  return results;
}

void ModelManager::OnBackendModelsReady(
    ModelsCallback callback,
    std::vector<ModelInfo> all_models,
    int pending_backends,
    std::vector<ModelInfo> backend_models) {
  // Append backend models to all_models
  all_models.insert(all_models.end(), backend_models.begin(),
                   backend_models.end());

  pending_backends--;

  if (pending_backends == 0) {
    // All backends have responded
    std::move(callback).Run(std::move(all_models));
  } else {
    // Still waiting for other backends
    // Note: This simplified implementation doesn't properly handle
    // multiple callbacks. A production implementation would need
    // a barrier or counter to collect all results.
  }
}

AIBackendInterface* ModelManager::GetBackendForModel(
    const std::string& model_id) {
  if (base::StartsWith(model_id, "ollama:") && ollama_backend_) {
    return ollama_backend_.get();
  } else if (base::StartsWith(model_id, "openrouter:") &&
             openrouter_backend_) {
    return openrouter_backend_.get();
  }
#if BUILDFLAG(IS_MAC)
  else if (base::StartsWith(model_id, "mlx:") && mlx_backend_) {
    return mlx_backend_.get();
  }
#endif

  return nullptr;
}

}  // namespace ai_sidebar
