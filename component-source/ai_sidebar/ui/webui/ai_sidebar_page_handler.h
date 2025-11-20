// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_UI_WEBUI_AI_SIDEBAR_PAGE_HANDLER_H_
#define COMPONENTS_AI_SIDEBAR_UI_WEBUI_AI_SIDEBAR_PAGE_HANDLER_H_

#include <memory>
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebContents;
}

namespace ai_sidebar {

class ModelManager;
class PageContextExtractor;
class WebSearchService;

// Handles messages from the AI Sidebar WebUI
class AISidebarPageHandler : public content::WebUIMessageHandler {
 public:
  explicit AISidebarPageHandler(content::WebContents* web_contents);
  ~AISidebarPageHandler() override;

  // WebUIMessageHandler implementation
  void RegisterMessages() override;

 private:
  // Message handlers
  void HandleGetSettings(const base::Value::List& args);
  void HandleSaveSettings(const base::Value::List& args);
  void HandleGetAvailableModels(const base::Value::List& args);
  void HandleSendMessage(const base::Value::List& args);

  // Callbacks
  void OnModelsReady(std::string callback_id,
                    std::vector<ModelInfo> models);
  void OnMessageGenerated(std::string callback_id,
                         GenerationResult result);

  // Helper methods
  void LoadSettings();
  void SaveSettings(const base::Value::Dict& settings);
  std::string GetCurrentPageContext();

  content::WebContents* web_contents_;
  std::unique_ptr<ModelManager> model_manager_;
  std::unique_ptr<PageContextExtractor> context_extractor_;
  std::unique_ptr<WebSearchService> search_service_;

  base::Value::Dict settings_;

  base::WeakPtrFactory<AISidebarPageHandler> weak_factory_{this};
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_UI_WEBUI_AI_SIDEBAR_PAGE_HANDLER_H_
