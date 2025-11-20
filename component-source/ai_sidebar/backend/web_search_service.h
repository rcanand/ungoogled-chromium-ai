// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_WEB_SEARCH_SERVICE_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_WEB_SEARCH_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

class Browser;

namespace ai_sidebar {

struct SearchResult {
  std::string title;
  std::string url;
  std::string snippet;
};

using SearchCallback =
    base::OnceCallback<void(std::vector<SearchResult> results)>;

// Service for web search integration
class WebSearchService {
 public:
  explicit WebSearchService(Browser* browser);
  ~WebSearchService();

  // Perform a web search using the browser's default search engine
  void Search(const std::string& query, SearchCallback callback);

  // Open search results in new tabs
  void OpenInTabs(const std::vector<std::string>& urls);

  // Extract search results from a search results page
  std::vector<SearchResult> ExtractSearchResults(
      content::WebContents* web_contents);

 private:
  void OnSearchPageLoaded(SearchCallback callback,
                         content::WebContents* web_contents);

  GURL BuildSearchURL(const std::string& query);

  Browser* browser_;
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_WEB_SEARCH_SERVICE_H_
