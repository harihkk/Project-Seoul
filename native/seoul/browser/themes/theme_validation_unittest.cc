// Project Seoul theme system.
// Unit tests for color parsing, WCAG contrast, theme validation, round trips.

#include "seoul/browser/themes/theme_validation.h"

#include <cmath>

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

ThemeColor Rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ThemeColor{r, g, b, 255};
}

Theme ReadableLightTheme() {
  Theme theme;
  theme.id = "seoul-light";
  theme.name = "Seoul Light";
  theme.scheme = ColorScheme::kLight;
  theme.colors.background = Rgb(255, 255, 255);
  theme.colors.surface = Rgb(245, 245, 245);
  theme.colors.text = Rgb(20, 20, 20);
  theme.colors.muted_text = Rgb(90, 90, 90);
  theme.colors.accent = Rgb(20, 60, 160);
  theme.colors.accent_text = Rgb(255, 255, 255);
  theme.colors.border = Rgb(200, 200, 200);
  theme.colors.error = Rgb(150, 20, 20);
  theme.typography.font_family = "Inter";
  return theme;
}

TEST(ThemeColorTest, ParsesAndSerializesHex) {
  ThemeColor color;
  ASSERT_TRUE(ParseHexColor("#1a2b3c", &color));
  EXPECT_EQ(color.r, 0x1a);
  EXPECT_EQ(color.g, 0x2b);
  EXPECT_EQ(color.b, 0x3c);
  EXPECT_EQ(color.a, 255);
  EXPECT_EQ(ColorToHex(color), "#1a2b3cff");

  ThemeColor shorthand;
  ASSERT_TRUE(ParseHexColor("#fff", &shorthand));
  EXPECT_EQ(shorthand.r, 255);

  ThemeColor alpha;
  ASSERT_TRUE(ParseHexColor("#11223344", &alpha));
  EXPECT_EQ(alpha.a, 0x44);

  ThemeColor invalid;
  EXPECT_FALSE(ParseHexColor("1a2b3c", &invalid));   // no '#'
  EXPECT_FALSE(ParseHexColor("#12", &invalid));      // wrong length
  EXPECT_FALSE(ParseHexColor("#gggggg", &invalid));  // non-hex
}

TEST(ThemeContrastTest, KnownRatios) {
  // Black on white is the maximum 21:1.
  EXPECT_NEAR(ContrastRatio(Rgb(0, 0, 0), Rgb(255, 255, 255)), 21.0, 0.01);
  // Identical colors are 1:1.
  EXPECT_NEAR(ContrastRatio(Rgb(80, 80, 80), Rgb(80, 80, 80)), 1.0, 0.001);
  // Symmetric.
  EXPECT_NEAR(ContrastRatio(Rgb(255, 255, 255), Rgb(0, 0, 0)),
              ContrastRatio(Rgb(0, 0, 0), Rgb(255, 255, 255)), 0.001);
}

TEST(ThemeValidationTest, AcceptsReadableTheme) {
  EXPECT_TRUE(ValidateTheme(ReadableLightTheme()).has_value());
}

TEST(ThemeValidationTest, RejectsLowContrastText) {
  Theme theme = ReadableLightTheme();
  theme.colors.text = Rgb(200, 200, 200);  // light gray on white
  EXPECT_EQ(ValidateTheme(theme).error(), ThemeError::kContrastTooLow);
}

TEST(ThemeValidationTest, RejectsLowContrastAccentText) {
  Theme theme = ReadableLightTheme();
  theme.colors.accent = Rgb(220, 220, 255);
  theme.colors.accent_text = Rgb(240, 240, 255);
  EXPECT_EQ(ValidateTheme(theme).error(), ThemeError::kContrastTooLow);
}

TEST(ThemeValidationTest, RejectsInvalidTypographyAndTokens) {
  Theme theme = ReadableLightTheme();
  theme.typography.base_size_px = 2;
  EXPECT_EQ(ValidateTheme(theme).error(), ThemeError::kInvalidTypography);

  theme = ReadableLightTheme();
  theme.custom_colors["Invalid Name"] = Rgb(0, 0, 0);
  EXPECT_EQ(ValidateTheme(theme).error(), ThemeError::kInvalidToken);

  theme = ReadableLightTheme();
  theme.name.clear();
  EXPECT_EQ(ValidateTheme(theme).error(), ThemeError::kInvalidName);
}

TEST(ThemeValidationTest, DarkThemePassesAndRoundTrips) {
  Theme dark;
  dark.id = "seoul-dark";
  dark.name = "Seoul Dark";
  dark.scheme = ColorScheme::kDark;
  dark.colors.background = Rgb(18, 18, 20);
  dark.colors.surface = Rgb(30, 30, 34);
  dark.colors.text = Rgb(235, 235, 240);
  dark.colors.muted_text = Rgb(160, 160, 170);
  dark.colors.accent = Rgb(120, 170, 255);
  dark.colors.accent_text = Rgb(10, 10, 14);
  dark.colors.border = Rgb(70, 70, 80);
  dark.colors.error = Rgb(255, 140, 140);
  dark.typography.font_family = "Inter";
  dark.motion.reduced_motion = true;
  dark.custom_colors["chart_series_one"] = Rgb(120, 200, 160);
  ASSERT_TRUE(ValidateTheme(dark).has_value());

  base::Value::Dict serialized = ThemeToValue(dark);
  auto parsed = ThemeFromValue(base::Value(serialized.Clone()));
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed.value(), dark);
}

TEST(ThemeValidationTest, ImportRejectsUnreadableTheme) {
  Theme theme = ReadableLightTheme();
  theme.colors.text = Rgb(250, 250, 250);
  base::Value::Dict serialized = ThemeToValue(theme);
  EXPECT_EQ(ThemeFromValue(base::Value(std::move(serialized))).error(),
            ThemeError::kContrastTooLow);
}

}  // namespace
}  // namespace seoul
