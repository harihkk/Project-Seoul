// Project Seoul theme registry tests.

#include "seoul/browser/themes/theme_registry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

Theme ReadableTheme(const std::string& id, const std::string& name) {
  Theme theme;
  theme.id = id;
  theme.name = name;
  theme.scheme = ColorScheme::kLight;
  theme.colors.background = {255, 255, 255, 255};
  theme.colors.surface = {248, 248, 248, 255};
  theme.colors.text = {20, 20, 20, 255};
  theme.colors.muted_text = {80, 80, 80, 255};
  theme.colors.accent = {36, 76, 120, 255};
  theme.colors.accent_text = {255, 255, 255, 255};
  theme.colors.border = {100, 100, 100, 255};
  theme.colors.error = {150, 0, 0, 255};
  theme.typography.font_family = "system-ui";
  return theme;
}

TEST(ThemeRegistryTest, StoresValidatedThemesInIdOrder) {
  ThemeRegistry registry;
  EXPECT_TRUE(registry.Upsert(ReadableTheme("calm", "Calm")).has_value());
  EXPECT_TRUE(registry.Upsert(ReadableTheme("bright", "Bright")).has_value());

  const std::vector<const Theme*> themes = registry.List();
  ASSERT_EQ(themes.size(), 2u);
  EXPECT_EQ(themes[0]->id, "bright");
  EXPECT_EQ(themes[1]->id, "calm");
  EXPECT_TRUE(registry.Exists("calm"));
}

TEST(ThemeRegistryTest, RejectsInvalidAndUnknownRemoval) {
  ThemeRegistry registry;
  Theme invalid = ReadableTheme("invalid id", "Invalid");
  EXPECT_EQ(registry.Upsert(std::move(invalid)).error(),
            ThemeError::kInvalidName);
  EXPECT_EQ(registry.Remove("missing").error(), ThemeError::kUnknownTheme);
}

TEST(ThemeRegistryTest, UpsertReplacesWithoutGrowing) {
  ThemeRegistry registry;
  EXPECT_TRUE(registry.Upsert(ReadableTheme("calm", "Calm")).has_value());
  EXPECT_TRUE(
      registry.Upsert(ReadableTheme("calm", "Calm revised")).has_value());
  EXPECT_EQ(registry.size(), 1u);
  ASSERT_NE(registry.Find("calm"), nullptr);
  EXPECT_EQ(registry.Find("calm")->name, "Calm revised");
}

TEST(ThemeRegistryTest, EnforcesCatalogBound) {
  ThemeRegistry registry;
  for (size_t i = 0; i < kMaxThemes; ++i) {
    EXPECT_TRUE(registry
                    .Upsert(ReadableTheme("theme" + std::to_string(i),
                                          "Theme " + std::to_string(i)))
                    .has_value());
  }
  EXPECT_EQ(
      registry.Upsert(ReadableTheme("overflow", "Overflow")).error(),
      ThemeError::kLimitExceeded);
}

TEST(ThemeRegistryTest, PersistenceRoundTripsAndSkipsInvalidEntries) {
  ThemeRegistry registry;
  ASSERT_TRUE(registry.Upsert(ReadableTheme("calm", "Calm")).has_value());
  base::DictValue state = registry.TakePersistedState();
  base::ListValue* themes = state.FindList("themes");
  ASSERT_NE(themes, nullptr);
  themes->Append(base::DictValue().Set("schema_version", 1));

  ThemeRegistry restored;
  restored.RestorePersistedState(state);
  ASSERT_EQ(restored.size(), 1u);
  ASSERT_NE(restored.Find("calm"), nullptr);
  EXPECT_EQ(restored.Find("calm")->name, "Calm");

  base::DictValue wrong_schema;
  wrong_schema.Set("schema_version", 99);
  wrong_schema.Set("themes", base::ListValue());
  restored.RestorePersistedState(wrong_schema);
  EXPECT_EQ(restored.size(), 1u);
}

}  // namespace
}  // namespace seoul
