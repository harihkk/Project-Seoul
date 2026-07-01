// Project Seoul theme system.
// Theme validation, WCAG contrast computation, and JSON round-tripping. A
// theme is accepted only when its text-on-background and accent-text pairs
// meet the configured minimum contrast ratio, so no theme (user, imported,
// or model-proposed) can produce unreadable UI.

#ifndef SEOUL_BROWSER_THEMES_THEME_VALIDATION_H_
#define SEOUL_BROWSER_THEMES_THEME_VALIDATION_H_

#include "base/values.h"
#include "seoul/browser/themes/theme_types.h"

namespace seoul {

// WCAG 2.x relative-luminance contrast ratio in [1.0, 21.0].
double ContrastRatio(const ThemeColor& a, const ThemeColor& b);

// WCAG AA normal-text threshold.
inline constexpr double kMinAccessibleContrast = 4.5;
// WCAG AA large-text / UI-component threshold.
inline constexpr double kMinLargeTextContrast = 3.0;

// Validates identity, typography bounds, custom-token count/charset, and the
// required contrast pairs (text/background, muted_text/background,
// accent_text/accent, error/background).
ThemeStatusResult ValidateTheme(const Theme& theme);

base::Value::Dict ThemeToValue(const Theme& theme);
ThemeResult<Theme> ThemeFromValue(const base::Value& value);

}  // namespace seoul

#endif  // SEOUL_BROWSER_THEMES_THEME_VALIDATION_H_
