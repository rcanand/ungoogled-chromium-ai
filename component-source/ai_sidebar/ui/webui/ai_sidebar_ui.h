// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_UI_WEBUI_AI_SIDEBAR_UI_H_
#define COMPONENTS_AI_SIDEBAR_UI_WEBUI_AI_SIDEBAR_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace ai_sidebar {

class AISidebarUI;

// WebUIConfig for AI Sidebar
class AISidebarUIConfig : public content::DefaultWebUIConfig<AISidebarUI> {
 public:
  AISidebarUIConfig();
  ~AISidebarUIConfig() override;
};

// WebUI controller for AI Sidebar
class AISidebarUI : public content::WebUIController {
 public:
  explicit AISidebarUI(content::WebUI* web_ui);
  ~AISidebarUI() override;

  AISidebarUI(const AISidebarUI&) = delete;
  AISidebarUI& operator=(const AISidebarUI&) = delete;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_UI_WEBUI_AI_SIDEBAR_UI_H_
