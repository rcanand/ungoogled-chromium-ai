// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/backend/openrouter_backend.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace ai_sidebar {

namespace {

constexpr char kOpenRouterEndpoint[] = "https://openrouter.ai/api/v1";
constexpr base::TimeDelta kModelCacheExpiry = base::Hours(1);

net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("openrouter_backend", R"(
    semantics {
      sender: "AI Sidebar"
      description:
        "Sends prompts to OpenRouter API for AI text generation using "
        "various cloud-based AI models."
      trigger: "User sends a message in the AI sidebar with an OpenRouter model."
      data: "User's prompt, selected model name, and API key."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: NO
      setting:
        "This feature can be disabled by not using OpenRouter models in "
        "the AI sidebar or by removing the API key."
    })");
}

}  // namespace

OpenRouterBackend::OpenRouterBackend(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      endpoint_(kOpenRouterEndpoint) {}

OpenRouterBackend::~OpenRouterBackend() = default;

bool OpenRouterBackend::Initialize() {
  // OpenRouter is available if we have an API key
  is_available_ = !api_key_.empty();
  return true;
}

bool OpenRouterBackend::IsAvailable() const {
  return is_available_ && !api_key_.empty();
}

void OpenRouterBackend::GenerateText(const GenerationRequest& request,
                                    GenerationCallback callback) {
  if (!IsAvailable()) {
    GenerationResult result;
    result.success = false;
    result.error_message = "OpenRouter API key not configured";
    std::move(callback).Run(std::move(result));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_.Resolve("/chat/completions");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");
  resource_request->headers.SetHeader("Authorization", "Bearer " + api_key_);
  resource_request->headers.SetHeader("HTTP-Referer",
                                     "https://ungoogled-chromium.ai");

  base::Value::Dict request_body = CreateChatRequest(request);

  std::string json_body;
  base::JSONWriter::Write(request_body, &json_body);

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetTrafficAnnotation());
  loader->AttachStringForUpload(json_body, "application/json");

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OpenRouterBackend::OnGenerateResponse,
                     base::Unretained(this), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void OpenRouterBackend::GenerateTextStreaming(
    const GenerationRequest& request,
    StreamCallback stream_callback,
    GenerationCallback complete_callback) {
  // For now, fall back to non-streaming
  // Proper streaming would use Server-Sent Events (SSE)
  GenerateText(request, std::move(complete_callback));
}

void OpenRouterBackend::ListModels(ModelsCallback callback) {
  // Check cache expiry
  if (!cached_models_.empty() &&
      (base::Time::Now() - last_model_fetch_) < kModelCacheExpiry) {
    std::move(callback).Run(cached_models_);
    return;
  }

  if (!IsAvailable()) {
    std::move(callback).Run(std::vector<ModelInfo>());
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = endpoint_.Resolve("/models");
  resource_request->method = "GET";
  resource_request->headers.SetHeader("Authorization", "Bearer " + api_key_);

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 GetTrafficAnnotation());

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&OpenRouterBackend::OnModelsResponse,
                     base::Unretained(this), std::move(callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

std::string OpenRouterBackend::GetBackendName() const {
  return "openrouter";
}

bool OpenRouterBackend::SupportsCapability(const std::string& model_id,
                                          ModelCapability capability) const {
  // Find model in cache to check capabilities
  for (const auto& model : cached_models_) {
    if (model.id == model_id) {
      return (model.capabilities & static_cast<uint32_t>(capability)) != 0;
    }
  }

  // Default: assume text support
  return capability == ModelCapability::TEXT;
}

void OpenRouterBackend::SetAPIKey(const std::string& api_key) {
  api_key_ = api_key;
  is_available_ = !api_key_.empty();
}

void OpenRouterBackend::OnModelsResponse(
    ModelsCallback callback,
    std::unique_ptr<std::string> response_body) {
  std::vector<ModelInfo> models;

  if (!response_body) {
    std::move(callback).Run(models);
    return;
  }

  auto parsed = base::JSONReader::Read(*response_body);
  if (!parsed || !parsed->is_dict()) {
    std::move(callback).Run(models);
    return;
  }

  const base::Value::List* data_list = parsed->GetDict().FindList("data");
  if (!data_list) {
    std::move(callback).Run(models);
    return;
  }

  for (const auto& model_value : *data_list) {
    if (!model_value.is_dict()) {
      continue;
    }

    const auto& model_dict = model_value.GetDict();
    const std::string* id = model_dict.FindString("id");
    const std::string* name = model_dict.FindString("name");

    if (!id) {
      continue;
    }

    ModelInfo info;
    info.id = "openrouter:" + *id;
    info.display_name = (name ? *name : *id) + " (OpenRouter)";
    info.provider = "openrouter";
    info.capabilities = static_cast<uint32_t>(ModelCapability::TEXT);

    // Check for vision capability
    const base::Value::Dict* architecture = model_dict.FindDict("architecture");
    if (architecture) {
      const std::string* modality = architecture->FindString("modality");
      if (modality && base::Contains(*modality, "vision")) {
        info.capabilities |= static_cast<uint32_t>(ModelCapability::IMAGE);
      }
    }

    // Get context length
    if (auto context = model_dict.FindInt("context_length")) {
      info.context_length = static_cast<size_t>(*context);
    }

    const std::string* description = model_dict.FindString("description");
    if (description) {
      info.description = *description;
    }

    // Parse pricing if available
    const base::Value::Dict* pricing = model_dict.FindDict("pricing");
    if (pricing) {
      if (auto prompt_price = pricing->FindString("prompt")) {
        info.description += " | Prompt: $" + *prompt_price;
      }
    }

    models.push_back(std::move(info));
  }

  cached_models_ = models;
  last_model_fetch_ = base::Time::Now();
  std::move(callback).Run(models);
}

void OpenRouterBackend::OnGenerateResponse(
    GenerationCallback callback,
    std::unique_ptr<std::string> response_body) {
  GenerationResult result;

  if (!response_body) {
    result.success = false;
    result.error_message = "No response from OpenRouter";
    std::move(callback).Run(std::move(result));
    return;
  }

  result.text = ParseOpenRouterResponse(*response_body);
  result.success = !result.text.empty();

  if (!result.success) {
    result.error_message = "Failed to parse OpenRouter response";
  }

  std::move(callback).Run(std::move(result));
}

std::string OpenRouterBackend::ParseOpenRouterResponse(
    const std::string& json_response) {
  auto parsed = base::JSONReader::Read(json_response);
  if (!parsed || !parsed->is_dict()) {
    return "";
  }

  const base::Value::List* choices = parsed->GetDict().FindList("choices");
  if (!choices || choices->empty()) {
    return "";
  }

  const base::Value::Dict* first_choice = (*choices)[0].GetIfDict();
  if (!first_choice) {
    return "";
  }

  const base::Value::Dict* message = first_choice->FindDict("message");
  if (!message) {
    return "";
  }

  const std::string* content = message->FindString("content");
  return content ? *content : "";
}

base::Value::Dict OpenRouterBackend::CreateChatRequest(
    const GenerationRequest& request) {
  base::Value::Dict body;

  // Extract model name without "openrouter:" prefix
  std::string model_name = request.model;
  if (base::StartsWith(model_name, "openrouter:")) {
    model_name = model_name.substr(11);
  }

  body.Set("model", model_name);

  // Create messages array
  base::Value::List messages;

  // Add system message if provided
  if (!request.system_prompt.empty()) {
    base::Value::Dict system_msg;
    system_msg.Set("role", "system");
    system_msg.Set("content", request.system_prompt);
    messages.Append(std::move(system_msg));
  }

  // Add user message
  base::Value::Dict user_msg;
  user_msg.Set("role", "user");

  // Handle vision models with images
  if (!request.image_data.empty()) {
    base::Value::List content_parts;

    base::Value::Dict text_part;
    text_part.Set("type", "text");
    text_part.Set("text", request.prompt);
    content_parts.Append(std::move(text_part));

    base::Value::Dict image_part;
    image_part.Set("type", "image_url");
    base::Value::Dict image_url;
    std::string base64_image = base::Base64Encode(request.image_data);
    image_url.Set("url", "data:image/jpeg;base64," + base64_image);
    image_part.Set("image_url", std::move(image_url));
    content_parts.Append(std::move(image_part));

    user_msg.Set("content", std::move(content_parts));
  } else {
    user_msg.Set("content", request.prompt);
  }

  messages.Append(std::move(user_msg));

  body.Set("messages", std::move(messages));

  return body;
}

}  // namespace ai_sidebar
