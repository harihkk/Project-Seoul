// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/url_policy.h"

namespace seoul {

CommandStatusResult UrlPolicy::ValidateNavigationUrl(const GURL& url) {
  if (!url.is_valid()) {
    return CommandErr(CommandError::kInvalidUrl);
  }
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIs(url::kAboutScheme) &&
      !url.SchemeIs(url::kFileScheme)) {
    return CommandErr(CommandError::kUnsupportedUrlScheme);
  }
  return CommandOk();
}

}  // namespace seoul
