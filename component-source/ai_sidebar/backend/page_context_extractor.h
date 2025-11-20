// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AI_SIDEBAR_BACKEND_PAGE_CONTEXT_EXTRACTOR_H_
#define COMPONENTS_AI_SIDEBAR_BACKEND_PAGE_CONTEXT_EXTRACTOR_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace ai_sidebar {

struct PageMetadata {
  std::string title;
  GURL url;
  std::string description;
  std::string keywords;
};

struct PageContext {
  PageMetadata metadata;
  std::string visible_text;
  std::string selected_text;
  std::vector<uint8_t> screenshot;
};

using PageContextCallback = base::OnceCallback<void(PageContext)>;

// Extracts context from the current page for AI prompts
class PageContextExtractor {
 public:
  PageContextExtractor();
  ~PageContextExtractor();

  // Extract all context from a page
  void ExtractContext(content::WebContents* web_contents,
                     bool include_screenshot,
                     PageContextCallback callback);

  // Extract only text content
  void ExtractText(content::WebContents* web_contents,
                  base::OnceCallback<void(std::string)> callback);

  // Extract selected text
  std::string ExtractSelection(content::WebContents* web_contents);

  // Capture screenshot
  void CaptureScreenshot(content::WebContents* web_contents,
                        base::OnceCallback<void(std::vector<uint8_t>)> callback);

 private:
  void OnTextExtracted(PageContextCallback callback,
                      PageContext context,
                      bool include_screenshot,
                      content::WebContents* web_contents,
                      const std::string& text);

  void OnScreenshotCaptured(PageContextCallback callback,
                           PageContext context,
                           std::vector<uint8_t> screenshot);

  PageMetadata ExtractMetadata(content::WebContents* web_contents);
};

}  // namespace ai_sidebar

#endif  // COMPONENTS_AI_SIDEBAR_BACKEND_PAGE_CONTEXT_EXTRACTOR_H_
