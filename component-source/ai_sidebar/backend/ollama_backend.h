// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_OLLAMA_BACKEND_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_OLLAMA_BACKEND_H_

#include "components/ai_sidebar/backend/ai_backend_interface.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace ai_sidebar {

// Ollama backend for local model inference
// Connects to Ollama API at http://localhost:11434 by default
class OllamaBackend : public AIBackendInterface {
 public:
  explicit OllamaBackend(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~OllamaBackend() override;

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

  // Set custom Ollama endpoint
  void SetEndpoint(const std::string& endpoint);

 private:
  void OnModelsResponse(ModelsCallback callback,
                       std::unique_ptr<std::string> response_body);
  void OnGenerateResponse(GenerationCallback callback,
                         std::unique_ptr<std::string> response_body);
  void OnStreamingData(StreamCallback stream_callback,
                      GenerationCallback complete_callback,
                      std::string accumulated_text,
                      base::expected<std::string_view, std::string> chunk);

  std::string ParseOllamaResponse(const std::string& json_response);
  base::Value::Dict CreateGenerateRequest(const GenerationRequest& request);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  GURL endpoint_;
  bool is_available_ = false;
  std::vector<ModelInfo> cached_models_;
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_OLLAMA_BACKEND_H_
