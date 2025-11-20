// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/ui/webui/ai_sidebar_page_handler.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ai_sidebar/backend/model_manager.h"
#include "components/ai_sidebar/backend/page_context_extractor.h"
#include "components/ai_sidebar/backend/web_search_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace ai_sidebar {

namespace {

constexpr char kOpenRouterAPIKeyPref[] = "ai_sidebar.openrouter_api_key";
constexpr char kOllamaEndpointPref[] = "ai_sidebar.ollama_endpoint";
constexpr char kMLXModelDirPref[] = "ai_sidebar.mlx_model_dir";

}  // namespace

AISidebarPageHandler::AISidebarPageHandler(content::WebContents* web_contents)
    : web_contents_(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  // Initialize model manager
  model_manager_ = std::make_unique<ModelManager>(
      profile->GetURLLoaderFactory());
  model_manager_->Initialize();

  // Initialize other services
  context_extractor_ = std::make_unique<PageContextExtractor>();
  search_service_ = std::make_unique<WebSearchService>(
      chrome::FindBrowserWithWebContents(web_contents));

  LoadSettings();
}

AISidebarPageHandler::~AISidebarPageHandler() = default;

void AISidebarPageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSettings",
      base::BindRepeating(&AISidebarPageHandler::HandleGetSettings,
                         weak_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "saveSettings",
      base::BindRepeating(&AISidebarPageHandler::HandleSaveSettings,
                         weak_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "getAvailableModels",
      base::BindRepeating(&AISidebarPageHandler::HandleGetAvailableModels,
                         weak_factory_.GetWeakPtr()));

  web_ui()->RegisterMessageCallback(
      "sendMessage",
      base::BindRepeating(&AISidebarPageHandler::HandleSendMessage,
                         weak_factory_.GetWeakPtr()));
}

void AISidebarPageHandler::HandleGetSettings(const base::Value::List& args) {
  AllowJavascript();

  if (args.empty()) {
    return;
  }

  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(base::Value(callback_id), settings_.Clone());
}

void AISidebarPageHandler::HandleSaveSettings(const base::Value::List& args) {
  AllowJavascript();

  if (args.size() < 2 || !args[1].is_dict()) {
    return;
  }

  const std::string& callback_id = args[0].GetString();
  const base::Value::Dict& new_settings = args[1].GetDict();

  SaveSettings(new_settings);

  // Update backend configurations
  const std::string* api_key = new_settings.FindString("openRouterApiKey");
  if (api_key && model_manager_->openrouter_backend()) {
    model_manager_->openrouter_backend()->SetAPIKey(*api_key);
  }

  const std::string* endpoint = new_settings.FindString("ollamaEndpoint");
  if (endpoint && model_manager_->ollama_backend()) {
    model_manager_->ollama_backend()->SetEndpoint(*endpoint);
  }

#if BUILDFLAG(IS_MAC)
  const std::string* model_dir = new_settings.FindString("mlxModelDir");
  if (model_dir && model_manager_->mlx_backend()) {
    model_manager_->mlx_backend()->SetModelDirectory(
        base::FilePath(*model_dir));
  }
#endif

  ResolveJavascriptCallback(base::Value(callback_id), base::Value(true));
}

void AISidebarPageHandler::HandleGetAvailableModels(
    const base::Value::List& args) {
  AllowJavascript();

  if (args.empty()) {
    return;
  }

  const std::string& callback_id = args[0].GetString();

  model_manager_->GetAllModels(
      base::BindOnce(&AISidebarPageHandler::OnModelsReady,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AISidebarPageHandler::HandleSendMessage(const base::Value::List& args) {
  AllowJavascript();

  if (args.size() < 2 || !args[1].is_dict()) {
    return;
  }

  const std::string& callback_id = args[0].GetString();
  const base::Value::Dict& request = args[1].GetDict();

  const std::string* message = request.FindString("message");
  const std::string* model_id = request.FindString("model");

  if (!message || !model_id) {
    base::Value::Dict error_result;
    error_result.Set("success", false);
    error_result.Set("error", "Missing message or model");
    ResolveJavascriptCallback(base::Value(callback_id),
                             base::Value(std::move(error_result)));
    return;
  }

  // Build generation request
  GenerationRequest gen_request;
  gen_request.model = *model_id;
  gen_request.prompt = *message;

  // Add page context if requested
  auto use_context = request.FindBool("usePageContext");
  if (use_context && *use_context) {
    gen_request.system_prompt = "Page context:\n" + GetCurrentPageContext();
  }

  // Handle image attachment if present
  const std::string* image_data = request.FindString("image");
  if (image_data) {
    // Decode base64 image
    std::string decoded;
    if (base::Base64Decode(*image_data, &decoded)) {
      gen_request.image_data.assign(decoded.begin(), decoded.end());
    }
  }

  // Handle web search if requested
  auto enable_search = request.FindBool("enableWebSearch");
  if (enable_search && *enable_search) {
    // Perform web search and add results to context
    // For now, add a note in the system prompt
    gen_request.system_prompt +=
        "\n\nNote: Web search is enabled. You can reference web search "
        "results if needed.";
  }

  // Generate response
  model_manager_->GenerateText(
      gen_request,
      base::BindOnce(&AISidebarPageHandler::OnMessageGenerated,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void AISidebarPageHandler::OnModelsReady(std::string callback_id,
                                        std::vector<ModelInfo> models) {
  base::Value::Dict response;
  base::Value::List models_list;

  for (const auto& model : models) {
    models_list.Append(model.ToDict());
  }

  response.Set("models", std::move(models_list));

  ResolveJavascriptCallback(base::Value(callback_id),
                           base::Value(std::move(response)));
}

void AISidebarPageHandler::OnMessageGenerated(std::string callback_id,
                                              GenerationResult result) {
  base::Value::Dict response;
  response.Set("success", result.success);

  if (result.success) {
    response.Set("text", result.text);
  } else {
    response.Set("error", result.error_message);
  }

  if (!result.metadata.empty()) {
    response.Set("metadata", result.metadata.Clone());
  }

  ResolveJavascriptCallback(base::Value(callback_id),
                           base::Value(std::move(response)));
}

void AISidebarPageHandler::LoadSettings() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  settings_.Set("openRouterApiKey", prefs->GetString(kOpenRouterAPIKeyPref));
  settings_.Set("ollamaEndpoint", prefs->GetString(kOllamaEndpointPref));
  settings_.Set("mlxModelDir", prefs->GetString(kMLXModelDirPref));

  // Apply loaded settings to backends
  const std::string* api_key = settings_.FindString("openRouterApiKey");
  if (api_key && !api_key->empty() && model_manager_->openrouter_backend()) {
    model_manager_->openrouter_backend()->SetAPIKey(*api_key);
  }

  const std::string* endpoint = settings_.FindString("ollamaEndpoint");
  if (endpoint && !endpoint->empty() && model_manager_->ollama_backend()) {
    model_manager_->ollama_backend()->SetEndpoint(*endpoint);
  }

#if BUILDFLAG(IS_MAC)
  const std::string* model_dir = settings_.FindString("mlxModelDir");
  if (model_dir && !model_dir->empty() && model_manager_->mlx_backend()) {
    model_manager_->mlx_backend()->SetModelDirectory(
        base::FilePath(*model_dir));
  }
#endif
}

void AISidebarPageHandler::SaveSettings(const base::Value::Dict& new_settings) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  PrefService* prefs = profile->GetPrefs();

  const std::string* api_key = new_settings.FindString("openRouterApiKey");
  if (api_key) {
    prefs->SetString(kOpenRouterAPIKeyPref, *api_key);
    settings_.Set("openRouterApiKey", *api_key);
  }

  const std::string* endpoint = new_settings.FindString("ollamaEndpoint");
  if (endpoint) {
    prefs->SetString(kOllamaEndpointPref, *endpoint);
    settings_.Set("ollamaEndpoint", *endpoint);
  }

  const std::string* model_dir = new_settings.FindString("mlxModelDir");
  if (model_dir) {
    prefs->SetString(kMLXModelDirPref, *model_dir);
    settings_.Set("mlxModelDir", *model_dir);
  }
}

std::string AISidebarPageHandler::GetCurrentPageContext() {
  std::string context;

  context_extractor_->ExtractText(
      web_contents_,
      base::BindOnce([](std::string* output, const std::string& text) {
                       *output = text;
                     },
                     &context));

  return context;
}

}  // namespace ai_sidebar
