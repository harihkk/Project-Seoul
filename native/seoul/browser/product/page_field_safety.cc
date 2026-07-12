// Project Seoul product runtime - model-facing form field safety policy.

#include "seoul/browser/product/page_field_safety.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace seoul {

namespace {

bool IsCredentialToken(std::string_view token) {
  return token == "current-password" || token == "new-password" ||
         token == "webauthn";
}

bool IsOneTimeCodeToken(std::string_view token) {
  return token == "one-time-code";
}

bool IsPaymentToken(std::string_view token) {
  // These are the payment-specific autofill field names in the HTML standard.
  // Prefix matching covers the defined cc-name/cc-exp subfields without
  // guessing from page copy, ids, or site-specific conventions.
  return token.starts_with("cc-") || token == "transaction-currency" ||
         token == "transaction-amount";
}

}  // namespace

PageFieldSensitivity ClassifyPageField(
    const PageFieldSafetyDescriptor& descriptor) {
  if (descriptor.protected_state ||
      base::EqualsCaseInsensitiveASCII(descriptor.input_type, "password")) {
    return PageFieldSensitivity::kCredential;
  }

  const std::vector<std::string_view> tokens = base::SplitStringPiece(
      descriptor.autocomplete, base::kWhitespaceASCII,
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string_view raw_token : tokens) {
    const std::string token = base::ToLowerASCII(raw_token);
    if (IsOneTimeCodeToken(token)) {
      return PageFieldSensitivity::kOneTimeCode;
    }
    if (IsCredentialToken(token)) {
      return PageFieldSensitivity::kCredential;
    }
    if (IsPaymentToken(token)) {
      return PageFieldSensitivity::kPayment;
    }
  }
  return PageFieldSensitivity::kNone;
}

bool AllowsModelValueMutation(PageFieldSensitivity sensitivity) {
  return sensitivity == PageFieldSensitivity::kNone;
}

const char* PageFieldSensitivityName(PageFieldSensitivity sensitivity) {
  switch (sensitivity) {
    case PageFieldSensitivity::kNone:
      return "none";
    case PageFieldSensitivity::kCredential:
      return "credential";
    case PageFieldSensitivity::kOneTimeCode:
      return "one_time_code";
    case PageFieldSensitivity::kPayment:
      return "payment";
  }
  return "none";
}

}  // namespace seoul
