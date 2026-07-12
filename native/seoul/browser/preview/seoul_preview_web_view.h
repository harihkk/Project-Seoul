// Project Seoul Preview WebView.
// Embeds a separately-owned, non-tab WebContents and applies Preview-specific
// containment: no popups, downloads, script dialogs, or fullscreen escape.

#ifndef SEOUL_BROWSER_PREVIEW_SEOUL_PREVIEW_WEB_VIEW_H_
#define SEOUL_BROWSER_PREVIEW_SEOUL_PREVIEW_WEB_VIEW_H_

#include "ui/views/controls/webview/webview.h"

namespace seoul {

class SeoulPreviewWebView : public views::WebView {
  METADATA_HEADER(SeoulPreviewWebView, views::WebView)

 public:
  explicit SeoulPreviewWebView(content::BrowserContext* browser_context);
  SeoulPreviewWebView(const SeoulPreviewWebView&) = delete;
  SeoulPreviewWebView& operator=(const SeoulPreviewWebView&) = delete;
  ~SeoulPreviewWebView() override;

  // content::WebContentsDelegate:
  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   base::OnceCallback<void(bool)> callback) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool CanEnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame) override;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PREVIEW_SEOUL_PREVIEW_WEB_VIEW_H_
