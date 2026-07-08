// Project Seoul Site Layers.
// Declarative, per-site visual customization. A Site Layer is a bounded list
// of typed adjustments (color, typography, width, spacing, density, hide,
// emphasize, sticky, reading mode, accessibility) scoped to an origin and
// optionally a Scene. Layers compile to safe scoped CSS; they never carry or
// generate JavaScript, and selectors/values are validated to prevent style
// injection or escaping the target document.

#ifndef SEOUL_BROWSER_SITE_LAYERS_SITE_LAYER_TYPES_H_
#define SEOUL_BROWSER_SITE_LAYERS_SITE_LAYER_TYPES_H_

#include <string>
#include <vector>

#include "base/types/expected.h"

namespace seoul {

inline constexpr int kSiteLayerSchemaVersion = 1;
inline constexpr size_t kMaxLayerRules = 64;
inline constexpr size_t kMaxSelectorLength = 256;
inline constexpr size_t kMaxSelectorsPerRule = 8;
inline constexpr size_t kMaxLayerNameLength = 120;
inline constexpr size_t kMaxOriginPatternLength = 256;
inline constexpr size_t kMaxSiteLayers = 256;

enum class SiteAdjustmentKind {
  kAccentColor,  // recolor accent/link elements
  kBackgroundColor,
  kTextColor,
  kFontFamily,        // family name only
  kFontSizeScale,     // multiplier in [0.5, 2.0]
  kContentWidth,      // max content width in px
  kLineSpacing,       // multiplier in [1.0, 3.0]
  kDensity,           // compact / comfortable / spacious
  kHide,              // display:none for matched elements
  kEmphasize,         // outline/weight emphasis
  kStickyHeaderOff,   // neutralize position:sticky/fixed on matched elements
  kReadingMode,       // document-level readable layout
  kIncreaseContrast,  // accessibility contrast boost
  kReduceMotion,      // accessibility motion reduction
};

enum class DensityLevel {
  kCompact,
  kComfortable,
  kSpacious,
};

struct SiteAdjustment {
  SiteAdjustmentKind kind = SiteAdjustmentKind::kReadingMode;
  // Target selectors. Empty means document-level (only valid for the
  // document-scoped kinds: reading mode, contrast, motion, width). Non-empty
  // selectors are validated to a safe subset.
  std::vector<std::string> selectors;
  std::string color_value;     // "#rrggbb"/"#rrggbbaa" for color kinds
  std::string font_family;     // family name for kFontFamily
  double numeric_value = 0.0;  // scale/width/spacing per kind
  DensityLevel density = DensityLevel::kComfortable;

  friend bool operator==(const SiteAdjustment&,
                         const SiteAdjustment&) = default;
};

struct SiteLayer {
  int schema_version = kSiteLayerSchemaVersion;
  std::string id;
  std::string name;
  std::string origin_pattern;  // "https://example.com" or "*.example.com"
  std::string scene_scope;     // optional Scene id
  bool enabled = true;
  std::vector<SiteAdjustment> adjustments;

  friend bool operator==(const SiteLayer&, const SiteLayer&) = default;
};

enum class SiteLayerError {
  kInvalidId,
  kInvalidName,
  kInvalidOrigin,
  kEmptyLayer,
  kTooManyRules,
  kInvalidSelector,
  kUnsafeSelector,
  kInvalidColor,
  kInvalidFontFamily,
  kInvalidNumericValue,
  kSelectorRequired,
  kSelectorNotAllowed,
  kUnsupportedSchema,
  kUnknownLayer,
  kLimitExceeded,
};

const char* SiteLayerErrorToString(SiteLayerError error);

template <typename T>
using SiteLayerResult = base::expected<T, SiteLayerError>;

using SiteLayerStatusResult = base::expected<void, SiteLayerError>;

}  // namespace seoul

#endif  // SEOUL_BROWSER_SITE_LAYERS_SITE_LAYER_TYPES_H_
