// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_OPENROUTER_BACKEND_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_OPENROUTER_BACKEND_H_

#include "components/ai_sidebar/backend/ai_backend_interface.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ai_sidebar {

// OpenRouter backend for cloud-based model inference
// Connects to OpenRouter API at https://openrouter.ai/api/v1
class OpenRouterBackend : public AIBackendInterface {
 public:
  explicit OpenRouterBackend(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~OpenRouterBackend() override;

  // AIBackendInterface implementation
  bool Initialize() override;
  bool IsAvailable() const override;
  void GenerateText(const GenerationRequest& request,
                   GenerationCallback callback) override;
  void GenerateTextStreaming(const GenerationRequest& request,
                            StreamCallback stream_callback,
                            GenerationCallback complete_callback) override;
  void ListModels(ModelsCallback callback) override;
  std::string GetBackendName() const override;
  bool SupportsCapability(const std::string& model_id,
                         ModelCapability capability) const override;

  // Set API key for authentication
  void SetAPIKey(const std::string& api_key);

 private:
  void OnModelsResponse(ModelsCallback callback,
                       std::unique_ptr<std::string> response_body);
  void OnGenerateResponse(GenerationCallback callback,
                         std::unique_ptr<std::string> response_body);

  std::string ParseOpenRouterResponse(const std::string& json_response);
  base::Value::Dict CreateChatRequest(const GenerationRequest& request);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GURL endpoint_;
  std::string api_key_;
  bool is_available_ = false;
  std::vector<ModelInfo> cached_models_;
  base::Time last_model_fetch_;
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_OPENROUTER_BACKEND_H_
