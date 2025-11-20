// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ai_sidebar/backend/page_context_extractor.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ai_sidebar {

namespace {

constexpr char kExtractTextScript[] = R"(
(function() {
  // Get all visible text from the page
  function getVisibleText(element) {
    if (!element) return '';

    let text = '';
    const walker = document.createTreeWalker(
      element,
      NodeFilter.SHOW_TEXT,
      {
        acceptNode: function(node) {
          const parent = node.parentElement;
          if (!parent) return NodeFilter.FILTER_REJECT;

          const style = window.getComputedStyle(parent);
          if (style.display === 'none' ||
              style.visibility === 'hidden' ||
              style.opacity === '0') {
            return NodeFilter.FILTER_REJECT;
          }

          return NodeFilter.FILTER_ACCEPT;
        }
      }
    );

    while (walker.nextNode()) {
      const nodeText = walker.currentNode.textContent.trim();
      if (nodeText) {
        text += nodeText + ' ';
      }
    }

    return text.trim();
  }

  return getVisibleText(document.body);
})();
)";

}  // namespace

PageContextExtractor::PageContextExtractor() = default;

PageContextExtractor::~PageContextExtractor() = default;

void PageContextExtractor::ExtractContext(content::WebContents* web_contents,
                                         bool include_screenshot,
                                         PageContextCallback callback) {
  PageContext context;
  context.metadata = ExtractMetadata(web_contents);
  context.selected_text = ExtractSelection(web_contents);

  // Extract page text
  ExtractText(
      web_contents,
      base::BindOnce(&PageContextExtractor::OnTextExtracted,
                     base::Unretained(this), std::move(callback),
                     std::move(context), include_screenshot, web_contents));
}

void PageContextExtractor::ExtractText(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::string)> callback) {
  if (!web_contents) {
    std::move(callback).Run("");
    return;
  }

  web_contents->GetPrimaryMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(kExtractTextScript),
      base::BindOnce(
          [](base::OnceCallback<void(std::string)> callback, base::Value value) {
            if (value.is_string()) {
              std::move(callback).Run(value.GetString());
            } else {
              std::move(callback).Run("");
            }
          },
          std::move(callback)));
}

std::string PageContextExtractor::ExtractSelection(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return "";
  }

  // This would require querying the selection from the renderer
  // For now, return empty string
  return "";
}

void PageContextExtractor::CaptureScreenshot(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::vector<uint8_t>)> callback) {
  if (!web_contents) {
    std::move(callback).Run(std::vector<uint8_t>());
    return;
  }

  content::RenderWidgetHostView* view =
      web_contents->GetRenderWidgetHostView();
  if (!view) {
    std::move(callback).Run(std::vector<uint8_t>());
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(),
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<uint8_t>)> callback,
             const SkBitmap& bitmap) {
            if (bitmap.empty()) {
              std::move(callback).Run(std::vector<uint8_t>());
              return;
            }

            std::vector<uint8_t> jpeg_data;
            gfx::JPEGCodec::Encode(bitmap, 85, &jpeg_data);
            std::move(callback).Run(std::move(jpeg_data));
          },
          std::move(callback)));
}

void PageContextExtractor::OnTextExtracted(PageContextCallback callback,
                                          PageContext context,
                                          bool include_screenshot,
                                          content::WebContents* web_contents,
                                          const std::string& text) {
  context.visible_text = text;

  if (include_screenshot) {
    CaptureScreenshot(
        web_contents,
        base::BindOnce(&PageContextExtractor::OnScreenshotCaptured,
                       base::Unretained(this), std::move(callback),
                       std::move(context)));
  } else {
    std::move(callback).Run(std::move(context));
  }
}

void PageContextExtractor::OnScreenshotCaptured(PageContextCallback callback,
                                               PageContext context,
                                               std::vector<uint8_t> screenshot) {
  context.screenshot = std::move(screenshot);
  std::move(callback).Run(std::move(context));
}

PageMetadata PageContextExtractor::ExtractMetadata(
    content::WebContents* web_contents) {
  PageMetadata metadata;

  if (!web_contents) {
    return metadata;
  }

  metadata.title = base::UTF16ToUTF8(web_contents->GetTitle());
  metadata.url = web_contents->GetLastCommittedURL();

  // Extract meta description and keywords would require querying the DOM
  // For now, return basic metadata

  return metadata;
}

}  // namespace ai_sidebar
