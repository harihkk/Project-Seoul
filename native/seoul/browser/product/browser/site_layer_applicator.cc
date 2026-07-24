// Project Seoul Site Layers - live WebContents application.

#include "seoul/browser/product/browser/site_layer_applicator.h"

#include <string>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "seoul/browser/site_layers/site_layer_registry.h"
#include "url/origin.h"

namespace seoul {

namespace {

bool IsCustomizableUrl(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

std::u16string BuildApplyScript(const std::string& css) {
  // JSONWriter is the only interpolation boundary. CSS has already passed the
  // Site Layer compiler, and the encoded JSON string is passed only to
  // CSSStyleSheet.replaceSync; it is never parsed or evaluated as script.
  base::DictValue payload;
  payload.Set("css", css);
  std::string payload_json;
  base::JSONWriter::Write(payload, &payload_json);
  const std::string script =
      "(() => {"
      "const p=" +
      payload_json +
      ";"
      "const k='__seoulSiteLayerSheetV1';"
      "const old=globalThis[k];"
      "if(old){document.adoptedStyleSheets="
      "document.adoptedStyleSheets.filter(s=>s!==old);delete globalThis[k];}"
      "if(!p.css)return true;"
      "const sheet=new CSSStyleSheet();"
      "sheet.replaceSync(p.css);"
      "document.adoptedStyleSheets=[...document.adoptedStyleSheets,sheet];"
      "globalThis[k]=sheet;"
      "return document.adoptedStyleSheets.includes(sheet);"
      "})()";
  return base::UTF8ToUTF16(script);
}

}  // namespace

SiteLayerApplicator::SiteLayerApplicator(content::WebContents* web_contents,
                                         SiteLayerRegistry* registry)
    : content::WebContentsObserver(web_contents), registry_(registry) {}

SiteLayerApplicator::~SiteLayerApplicator() = default;

void SiteLayerApplicator::Refresh(const std::string& scene_id) {
  scene_id_ = scene_id;
  compiled_css_.clear();
  content::WebContents* contents = web_contents();
  if (registry_ && contents &&
      IsCustomizableUrl(contents->GetLastCommittedURL())) {
    const url::Origin origin =
        url::Origin::Create(contents->GetLastCommittedURL());
    if (!origin.opaque()) {
      SiteLayerResult<std::string> compiled =
          registry_->CompileForOrigin(origin.Serialize(), scene_id_);
      if (compiled.has_value()) {
        compiled_css_ = std::move(compiled.value());
      }
    }
  }
  ApplyToPrimaryMainFrame();
}

void SiteLayerApplicator::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle || !navigation_handle->HasCommitted() ||
      !navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  Refresh(scene_id_);
}

void SiteLayerApplicator::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  if (!render_frame_host || !render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }
  // Reapply once the head exists. This also repairs pages that replace their
  // head during early boot without waiting for another navigation.
  ApplyToPrimaryMainFrame();
}

void SiteLayerApplicator::WebContentsDestroyed() {
  Observe(nullptr);
  compiled_css_.clear();
}

void SiteLayerApplicator::ApplyToPrimaryMainFrame() {
  content::WebContents* contents = web_contents();
  if (!contents) {
    return;
  }
  content::RenderFrameHost* frame = contents->GetPrimaryMainFrame();
  if (!frame || !frame->IsRenderFrameLive()) {
    return;
  }
  frame->ExecuteJavaScriptInIsolatedWorld(
      BuildApplyScript(compiled_css_), base::DoNothing(),
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
}

}  // namespace seoul
