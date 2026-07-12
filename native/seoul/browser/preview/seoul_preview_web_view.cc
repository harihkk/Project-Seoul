// Project Seoul Preview WebView.

#include "seoul/browser/preview/seoul_preview_web_view.h"

#include <utility>

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace seoul {

SeoulPreviewWebView::SeoulPreviewWebView(
    content::BrowserContext* browser_context)
    : views::WebView(browser_context) {
  set_allow_accelerators(true);
  GetViewAccessibility().SetName(u"Link preview");
}

SeoulPreviewWebView::~SeoulPreviewWebView() = default;

content::WebContents* SeoulPreviewWebView::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  if (was_blocked) {
    *was_blocked = true;
  }
  // Ownership dies here. A Preview can navigate itself, but it cannot create
  // a hidden tab/window or silently escape the transient surface.
  return nullptr;
}

void SeoulPreviewWebView::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

bool SeoulPreviewWebView::ShouldSuppressDialogs(
    content::WebContents* source) {
  return true;
}

bool SeoulPreviewWebView::CanEnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame) {
  return false;
}

BEGIN_METADATA(SeoulPreviewWebView)
END_METADATA

}  // namespace seoul
