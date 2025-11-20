// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/backend/web_search_service.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

namespace ai_sidebar {

WebSearchService::WebSearchService(Browser* browser) : browser_(browser) {}

WebSearchService::~WebSearchService() = default;

void WebSearchService::Search(const std::string& query,
                             SearchCallback callback) {
  GURL search_url = BuildSearchURL(query);

  if (!search_url.is_valid()) {
    std::move(callback).Run(std::vector<SearchResult>());
    return;
  }

  // Open search in a new background tab
  content::WebContents* web_contents =
      browser_->tab_strip_model()->AddWebContents(
          content::WebContents::Create(
              content::WebContents::CreateParams(browser_->profile())),
          -1, ui::PAGE_TRANSITION_LINK, TabStripModel::ADD_NONE);

  content::NavigationController::LoadURLParams params(search_url);
  web_contents->GetController().LoadURLWithParams(params);

  // Note: In production, would observe load completion and extract results
  // For now, return empty results
  std::move(callback).Run(std::vector<SearchResult>());
}

void WebSearchService::OpenInTabs(const std::vector<std::string>& urls) {
  for (const auto& url_str : urls) {
    GURL url(url_str);
    if (!url.is_valid()) {
      continue;
    }

    content::WebContents* web_contents =
        browser_->tab_strip_model()->AddWebContents(
            content::WebContents::Create(
                content::WebContents::CreateParams(browser_->profile())),
            -1, ui::PAGE_TRANSITION_LINK, TabStripModel::ADD_NONE);

    content::NavigationController::LoadURLParams params(url);
    web_contents->GetController().LoadURLWithParams(params);
  }
}

std::vector<SearchResult> WebSearchService::ExtractSearchResults(
    content::WebContents* web_contents) {
  std::vector<SearchResult> results;

  // This would require executing JavaScript to extract search results
  // from the DOM. Implementation depends on the search engine used.

  // Example for Google:
  // - Query selector for .g (search result containers)
  // - Extract title from h3
  // - Extract URL from a href
  // - Extract snippet from .VwiC3b

  return results;
}

GURL WebSearchService::BuildSearchURL(const std::string& query) {
  // Use a simple DuckDuckGo search as default
  // In production, would use the browser's configured search engine
  std::string base_url = "https://duckduckgo.com/";
  return net::AppendQueryParameter(GURL(base_url), "q", query);
}

}  // namespace ai_sidebar
