# Seoul Theme System Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

A Seoul theme is a bounded set of design tokens plus accessibility preferences.
This spec describes the source in `native/seoul/browser/themes/`. Contrast is
validated before a theme is accepted, so no theme (user, imported, or
model-proposed) can make text unreadable.

## Theme token model

`native/seoul/browser/themes/theme_types.h` defines `Theme`: a schema version, a
slug `id`, a `name`, a `ColorScheme`, `ThemeColorRoles`, `ThemeTypography`,
`ThemeMotion`, a corner radius, and a `custom_colors` map. Colors are stored as
`ThemeColor` bytes (r, g, b, a) so serialization is exact; `ParseHexColor`
accepts `#rgb`, `#rrggbb`, or `#rrggbbaa`.

- `ThemeColorRoles` are the required roles every theme must define: `background`,
  `surface`, `text`, `muted_text`, `accent`, `accent_text` (text drawn on
  accent), `border`, and `error`. Custom tokens extend but never replace these.
- `ThemeTypography` carries a `font_family` (family name only, never a URL or
  @font-face), `base_size_px`, `scale_ratio`, and `base_line_height_permille`.
- `ThemeMotion` carries `reduced_motion`, `reduced_transparency`, and
  `base_duration_ms`.
- `custom_colors` is a name-to-`ThemeColor` map capped at `kMaxCustomTokens`
  (64), each name a bounded slug.

## Light, dark, and system

`ColorScheme` is `kLight`, `kDark`, or `kSystem`, round-tripped through
`SchemeToString`/`SchemeFromString`. The header notes themes apply globally or
per Scene (a Scene references a theme by id; see the Scenes spec).

## Contrast computation

`theme_validation.cc` computes the WCAG relative-luminance contrast ratio.
`LinearChannel` linearizes each sRGB channel (the 0.03928 / 12.92 / 2.4 curve),
`RelativeLuminance` weights them 0.2126 / 0.7152 / 0.0722, and `ContrastRatio`
returns `(lighter + 0.05) / (darker + 0.05)` in the range 1.0 to 21.0. The AA
thresholds are constants in the header: `kMinAccessibleContrast` (4.5) for normal
text and `kMinLargeTextContrast` (3.0) for large text and UI components.

## Contrast gates

`ValidateTheme` rejects a theme with `kContrastTooLow` unless all five gates
pass. Three require the normal-text AA ratio of 4.5: text over background, text
over surface, and accent_text over accent. Two require the large-text/UI ratio of
3.0: muted_text over background and error over background. A theme that would
render any of these pairs unreadable is never accepted.

## Other validation

`ValidateTheme` also checks the schema version, a valid slug id and bounded name,
and typography and shape bounds: base size 8 to 48 px, scale ratio 1.0 to 2.0,
base line height 1000 to 3000 permille, and corner radius 0 to 64 px. The custom
token count and each token name charset are validated. Failures return precise
`ThemeError` values (`kUnsupportedSchema`, `kInvalidName`, `kInvalidTypography`,
`kTooManyCustomTokens`, `kInvalidToken`, `kInvalidColor`, `kContrastTooLow`).

## Reduced motion and transparency

`ThemeMotion.reduced_motion` and `reduced_transparency` are part of the theme
token model and are carried through validation and the JSON round trip, so a
theme can express reduced-motion and reduced-transparency preferences alongside
its visual tokens.

## JSON import and export

`ThemeToValue` serializes the full theme (scheme, all color roles, typography,
motion, corner radius, and custom colors) to a `base::Value::Dict`.
`ThemeFromValue` parses it back, rejects an unsupported schema version, requires
all eight color roles to parse, and then runs `ValidateTheme` before returning,
so contrast and bounds are enforced on import. An imported theme that fails any
gate is rejected rather than applied.
