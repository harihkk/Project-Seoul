// Project Seoul theme registry.

#include "seoul/browser/themes/theme_registry.h"

#include <tuple>
#include <utility>

namespace seoul {

namespace {

constexpr int kThemeRegistrySchemaVersion = 1;

}  // namespace

ThemeRegistry::ThemeRegistry() = default;
ThemeRegistry::~ThemeRegistry() = default;

ThemeStatusResult ThemeRegistry::Upsert(Theme theme) {
  if (ThemeStatusResult valid = ValidateTheme(theme); !valid.has_value()) {
    return valid;
  }
  const bool exists = themes_.contains(theme.id);
  if (!exists && themes_.size() >= kMaxThemes) {
    return base::unexpected(ThemeError::kLimitExceeded);
  }
  const std::string id = theme.id;
  themes_[id] = std::move(theme);
  return base::ok();
}

ThemeStatusResult ThemeRegistry::Remove(const std::string& theme_id) {
  if (themes_.erase(theme_id) == 0) {
    return base::unexpected(ThemeError::kUnknownTheme);
  }
  return base::ok();
}

const Theme* ThemeRegistry::Find(const std::string& theme_id) const {
  auto it = themes_.find(theme_id);
  return it == themes_.end() ? nullptr : &it->second;
}

bool ThemeRegistry::Exists(const std::string& theme_id) const {
  return Find(theme_id) != nullptr;
}

std::vector<const Theme*> ThemeRegistry::List() const {
  std::vector<const Theme*> result;
  result.reserve(themes_.size());
  for (const auto& [id, theme] : themes_) {
    result.push_back(&theme);
  }
  return result;
}

base::DictValue ThemeRegistry::TakePersistedState() const {
  base::DictValue state;
  state.Set("schema_version", kThemeRegistrySchemaVersion);
  base::ListValue themes;
  for (const auto& [id, theme] : themes_) {
    themes.Append(ThemeToValue(theme));
  }
  state.Set("themes", std::move(themes));
  return state;
}

void ThemeRegistry::RestorePersistedState(const base::DictValue& state) {
  if (state.FindInt("schema_version").value_or(0) !=
      kThemeRegistrySchemaVersion) {
    return;
  }
  const base::ListValue* themes = state.FindList("themes");
  if (!themes) {
    return;
  }
  ThemeRegistry restored;
  for (const base::Value& entry : *themes) {
    if (restored.size() >= kMaxThemes) {
      break;
    }
    ThemeResult<Theme> theme = ThemeFromValue(entry);
    if (theme.has_value()) {
      std::ignore = restored.Upsert(std::move(theme.value()));
    }
  }
  themes_ = std::move(restored.themes_);
}

}  // namespace seoul
