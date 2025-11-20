// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/ui/webui/ai_sidebar_ui.h"

#include "components/ai_sidebar/ui/webui/ai_sidebar_page_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ai_sidebar {

namespace {

constexpr char kAISidebarHost[] = "ai-sidebar";

void CreateAndAddAISidebarUIDataSource(content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kAISidebarHost);

  // Add resources
  source->AddResourcePath("ai_sidebar.html", IDR_AI_SIDEBAR_HTML);
  source->AddResourcePath("ai_sidebar.css", IDR_AI_SIDEBAR_CSS);
  source->AddResourcePath("ai_sidebar.js", IDR_AI_SIDEBAR_JS);
  source->SetDefaultResource(IDR_AI_SIDEBAR_HTML);

  // Allow loading of local resources
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'unsafe-inline';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
}

}  // namespace

AISidebarUIConfig::AISidebarUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, kAISidebarHost) {}

AISidebarUIConfig::~AISidebarUIConfig() = default;

AISidebarUI::AISidebarUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  CreateAndAddAISidebarUIDataSource(web_ui);

  // Add message handler
  web_ui->AddMessageHandler(std::make_unique<AISidebarPageHandler>(
      web_ui->GetWebContents()));
}

AISidebarUI::~AISidebarUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(AISidebarUI)

}  // namespace ai_sidebar
