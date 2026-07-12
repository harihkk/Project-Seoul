// Project Seoul product runtime - form field safety policy tests.

#include "seoul/browser/product/page_field_safety.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {

TEST(PageFieldSafetyTest, ProtectedStateWinsOverAuthorMarkup) {
  PageFieldSafetyDescriptor descriptor;
  descriptor.protected_state = true;
  descriptor.input_type = "text";
  descriptor.autocomplete = "off";
  EXPECT_EQ(ClassifyPageField(descriptor),
            PageFieldSensitivity::kCredential);
}

TEST(PageFieldSafetyTest, PasswordInputTypeIsCredential) {
  PageFieldSafetyDescriptor descriptor;
  descriptor.input_type = "PaSsWoRd";
  EXPECT_EQ(ClassifyPageField(descriptor),
            PageFieldSensitivity::kCredential);
}

TEST(PageFieldSafetyTest, ParsesCredentialTokensInStandardsOrder) {
  PageFieldSafetyDescriptor descriptor;
  descriptor.autocomplete = "section-login current-password webauthn";
  EXPECT_EQ(ClassifyPageField(descriptor),
            PageFieldSensitivity::kCredential);

  descriptor.autocomplete = "one-time-code";
  EXPECT_EQ(ClassifyPageField(descriptor),
            PageFieldSensitivity::kOneTimeCode);
}

TEST(PageFieldSafetyTest, ParsesEveryPaymentTokenFamily) {
  for (const std::string_view autocomplete :
       {"cc-name", "billing cc-number", "section-card cc-exp-month",
        "cc-csc", "transaction-currency", "transaction-amount"}) {
    PageFieldSafetyDescriptor descriptor;
    descriptor.autocomplete = autocomplete;
    EXPECT_EQ(ClassifyPageField(descriptor),
              PageFieldSensitivity::kPayment)
        << autocomplete;
  }
}

TEST(PageFieldSafetyTest, OrdinaryAutofillFieldsAreNotGuessedSensitive) {
  for (const std::string_view autocomplete :
       {"name", "username", "shipping street-address", "postal-code",
        "email", "tel"}) {
    PageFieldSafetyDescriptor descriptor;
    descriptor.input_type = "text";
    descriptor.autocomplete = autocomplete;
    EXPECT_EQ(ClassifyPageField(descriptor), PageFieldSensitivity::kNone)
        << autocomplete;
  }
}

TEST(PageFieldSafetyTest, OnlyNonSensitiveFieldsAllowModelValueMutation) {
  EXPECT_TRUE(AllowsModelValueMutation(PageFieldSensitivity::kNone));
  EXPECT_FALSE(
      AllowsModelValueMutation(PageFieldSensitivity::kCredential));
  EXPECT_FALSE(
      AllowsModelValueMutation(PageFieldSensitivity::kOneTimeCode));
  EXPECT_FALSE(AllowsModelValueMutation(PageFieldSensitivity::kPayment));
}

}  // namespace seoul
