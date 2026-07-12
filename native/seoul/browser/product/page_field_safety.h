// Project Seoul product runtime - model-facing form field safety policy.
//
// This policy is intentionally independent of Chromium's DOM and AX types so
// it can be exhaustively unit tested. The browser-owned PageAgent translates
// trusted accessibility metadata into this descriptor before it exposes a
// field handle or accepts a value-changing action.

#ifndef SEOUL_BROWSER_PRODUCT_PAGE_FIELD_SAFETY_H_
#define SEOUL_BROWSER_PRODUCT_PAGE_FIELD_SAFETY_H_

#include <string_view>

namespace seoul {

enum class PageFieldSensitivity {
  kNone,
  kCredential,
  kOneTimeCode,
  kPayment,
};

struct PageFieldSafetyDescriptor {
  // Chromium sets the protected AX state for password controls. This is the
  // primary credential signal and does not depend on author-supplied markup.
  bool protected_state = false;
  // The normalized HTML input type exposed by Chromium accessibility data.
  std::string_view input_type;
  // The raw standards-defined autocomplete attribute. It may contain a
  // section token, shipping/billing hint, field name, and `webauthn` token.
  std::string_view autocomplete;
};

// Classifies only browser/standards-backed signals. Accessible names, ids,
// placeholders, and domain-specific labels are deliberately not guessed.
PageFieldSensitivity ClassifyPageField(
    const PageFieldSafetyDescriptor& descriptor);

// Sensitive fields may still be focused/clicked so Chromium's password or
// payment autofill can work. Model-supplied value mutations are refused.
bool AllowsModelValueMutation(PageFieldSensitivity sensitivity);

const char* PageFieldSensitivityName(PageFieldSensitivity sensitivity);

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_PAGE_FIELD_SAFETY_H_
