// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/backend/ollama_backend.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ai_sidebar {

namespace {

constexpr char kDefaultOllamaEndpoint[] = "http://localhost:11434";

net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("ollama_backend", R"(
    semantics {
      sender: "AI Sidebar"
      description:
        "Sends prompts to local Ollama instance for AI text generation."
      trigger: "User sends a message in the AI sidebar."
      data: "User's prompt and selected model name."
      destination: LOCAL
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature can be disabled by not using the AI sidebar."
    })");
}

}  // namespace

OllamaBackend::OllamaBackend(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      endpoint_(kDefaultOllamaEndpoint) {}

OllamaBackend::~OllamaBackend() = default;

bool OllamaBackend::Initialize() {
  // Check if Ollama is available by attempting to reach the API
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_.Resolve("/api/tags");
  resource_request->method = "GET";
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetTrafficAnnotation());

  // Synchronous check with timeout
  // In production, this should be async
  is_available_ = true;  // Assume available, will check on first use
  return true;
}

bool OllamaBackend::IsAvailable() const {
  return is_available_;
}

void OllamaBackend::GenerateText(const GenerationRequest& request,
                                GenerationCallback callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_.Resolve("/api/generate");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  base::Value::Dict request_body = CreateGenerateRequest(request);
  request_body.Set("stream", false);

  std::string json_body;
  base::JSONWriter::Write(request_body, &json_body);

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetTrafficAnnotation());
  loader->AttachStringForUpload(json_body, "application/json");

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OllamaBackend::OnGenerateResponse, base::Unretained(this),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void OllamaBackend::GenerateTextStreaming(
    const GenerationRequest& request,
    StreamCallback stream_callback,
    GenerationCallback complete_callback) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_.Resolve("/api/generate");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  base::Value::Dict request_body = CreateGenerateRequest(request);
  request_body.Set("stream", true);

  std::string json_body;
  base::JSONWriter::Write(request_body, &json_body);

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetTrafficAnnotation());
  loader->AttachStringForUpload(json_body, "application/json");

  // For streaming, we need to handle chunks
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadAsStream(url_loader_factory_.get());

  // Note: Proper streaming implementation would use SetOnResponseStartedCallback
  // and process chunks as they arrive. This is simplified.
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OllamaBackend::OnGenerateResponse, base::Unretained(this),
                     std::move(complete_callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void OllamaBackend::ListModels(ModelsCallback callback) {
  if (!cached_models_.empty()) {
    std::move(callback).Run(cached_models_);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_.Resolve("/api/tags");
  resource_request->method = "GET";

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetTrafficAnnotation());

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OllamaBackend::OnModelsResponse, base::Unretained(this),
                     std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

std::string OllamaBackend::GetBackendName() const {
  return "ollama";
}

bool OllamaBackend::SupportsCapability(const std::string& model_id,
                                      ModelCapability capability) const {
  // Most Ollama models support text
  // Vision models typically have "vision" or "llava" in the name
  if (capability == ModelCapability::TEXT) {
    return true;
  } else if (capability == ModelCapability::IMAGE) {
    return base::Contains(model_id, "vision") ||
           base::Contains(model_id, "llava");
  }
  return false;
}

void OllamaBackend::SetEndpoint(const std::string& endpoint) {
  endpoint_ = GURL(endpoint);
}

void OllamaBackend::OnModelsResponse(
    ModelsCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::vector<ModelInfo> models;

  if (!response_body) {
    is_available_ = false;
    std::move(callback).Run(models);
    return;
  }

  auto parsed = base::JSONReader::Read(*response_body);
  if (!parsed || !parsed->is_dict()) {
    std::move(callback).Run(models);
    return;
  }

  const base::Value::List* models_list = parsed->GetDict().FindList("models");
  if (!models_list) {
    std::move(callback).Run(models);
    return;
  }

  for (const auto& model_value : *models_list) {
    if (!model_value.is_dict()) {
      continue;
    }

    const auto& model_dict = model_value.GetDict();
    const std::string* name = model_dict.FindString("name");
    if (!name) {
      continue;
    }

    ModelInfo info;
    info.id = "ollama:" + *name;
    info.display_name = *name + " (Ollama)";
    info.provider = "ollama";
    info.capabilities = static_cast<uint32_t>(ModelCapability::TEXT);

    // Check for vision capabilities
    if (base::Contains(*name, "llava") || base::Contains(*name, "vision")) {
      info.capabilities |= static_cast<uint32_t>(ModelCapability::IMAGE);
    }

    // Extract context length if available
    if (auto size = model_dict.FindInt("size")) {
      info.context_length = 2048;  // Default, could parse from details
    }

    const std::string* details = model_dict.FindString("details");
    if (details) {
      info.description = *details;
    }

    models.push_back(std::move(info));
  }

  cached_models_ = models;
  is_available_ = !models.empty();
  std::move(callback).Run(models);
}

void OllamaBackend::OnGenerateResponse(
    GenerationCallback callback,
    std::unique_ptr<std::string> response_body) {
  GenerationResult result;

  if (!response_body) {
    result.success = false;
    result.error_message = "No response from Ollama";
    std::move(callback).Run(std::move(result));
    return;
  }

  result.text = ParseOllamaResponse(*response_body);
  result.success = !result.text.empty();

  if (!result.success) {
    result.error_message = "Failed to parse Ollama response";
  }

  std::move(callback).Run(std::move(result));
}

std::string OllamaBackend::ParseOllamaResponse(
    const std::string& json_response) {
  auto parsed = base::JSONReader::Read(json_response);
  if (!parsed || !parsed->is_dict()) {
    return "";
  }

  const std::string* response = parsed->GetDict().FindString("response");
  return response ? *response : "";
}

base::Value::Dict OllamaBackend::CreateGenerateRequest(
    const GenerationRequest& request) {
  base::Value::Dict body;

  // Extract model name without "ollama:" prefix
  std::string model_name = request.model;
  if (base::StartsWith(model_name, "ollama:")) {
    model_name = model_name.substr(7);
  }

  body.Set("model", model_name);
  body.Set("prompt", request.prompt);

  if (!request.system_prompt.empty()) {
    body.Set("system", request.system_prompt);
  }

  // Handle vision models with images
  if (!request.image_data.empty()) {
    // Ollama expects base64 encoded images
    std::string base64_image = base::Base64Encode(request.image_data);
    base::Value::List images;
    images.Append(base64_image);
    body.Set("images", std::move(images));
  }

  return body;
}

}  // namespace ai_sidebar
