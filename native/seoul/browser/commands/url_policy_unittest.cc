// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/url_policy.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace seoul {
namespace {

TEST(UrlPolicyTest, RejectsInvalidUrl) {
  EXPECT_EQ(UrlPolicy::ValidateNavigationUrl(GURL()).error(),
            CommandError::kInvalidUrl);
}

TEST(UrlPolicyTest, AcceptsHttps) {
  EXPECT_TRUE(UrlPolicy::ValidateNavigationUrl(GURL("https://example.test/"))
                  .has_value());
}

TEST(UrlPolicyTest, RejectsJavascriptScheme) {
  EXPECT_EQ(
      UrlPolicy::ValidateNavigationUrl(GURL("javascript:alert(1)")).error(),
      CommandError::kUnsupportedUrlScheme);
}

}  // namespace
}  // namespace seoul
