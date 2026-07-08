# Seoul Site Layers Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

A Site Layer is a declarative, per-site visual customization that compiles to
safe scoped CSS and never carries or generates JavaScript. This spec describes
the source in `native/seoul/browser/site_layers/`. Selectors and values are
validated to prevent style injection or escaping the target document.

## Declarative per-site adjustments

`native/seoul/browser/site_layers/site_layer_types.h` defines `SiteLayer` (schema
version, stable id, name, `origin_pattern`, optional `scene_scope`, `enabled`, and a
bounded list of `SiteAdjustment`). `SiteAdjustmentKind` lists the supported
adjustments: `kAccentColor`, `kBackgroundColor`, `kTextColor`, `kFontFamily`,
`kFontSizeScale`, `kContentWidth`, `kLineSpacing`, `kDensity`, `kHide`,
`kEmphasize`, `kStickyHeaderOff`, `kReadingMode`, `kIncreaseContrast`, and
`kReduceMotion`. A `SiteAdjustment` carries target `selectors`, a `color_value`,
a `font_family`, a `numeric_value`, and a `DensityLevel`. Bounds include
`kMaxLayerRules` (64), `kMaxSelectorsPerRule` (8), and `kMaxSelectorLength` (256).

## Compilation to scoped CSS

`CompileSiteLayer` in `site_layer_compiler.cc` validates the layer, then emits
CSS deterministically in adjustment order. Color, font, size, spacing, hide,
emphasize, and sticky-off adjustments emit a rule scoped to the joined selector
list; document-scoped kinds emit fixed document-level rules. Every declaration
uses `!important`. The compiler never emits or references any script.

## Safe selector subset and rejected characters

`ContainsUnsafeCssChar` rejects any selector or value containing the characters
`{`, `}`, `;`, `<`, `\`, double quote, single quote, `@`, `(`, `)`, or
backtick, the comment sequences `/*` and `*/`, or the tokens `url`, `expression`,
`javascript`, or `import` (case-insensitive). `IsSafeSelector` additionally
requires the selector to be non-empty, within the length cap, drawn only from
letters, digits, and the combinator/attribute characters `. # - _ [ ] = * ~ ^ $
| :` plus space, `>`, and `+`, and to contain at least one identifier character
so a bare universal selector is not allowed. Because quotes are rejected, only
attribute-presence and prefix forms survive, which the header notes is acceptable
for the safe subset.

## Identity and origin-pattern validation

Layer ids are stable lowercase slugs. They must start with a letter and may then
contain lowercase letters, digits, `_`, and `-`; missing or malformed ids are
rejected before storage or import so Scenes can safely reference layers.

`IsValidOriginPattern` accepts only `https://host[:port]` or `*.host`. A
`https://` pattern may carry an optional numeric port in 1 to 65535. The host is
length-bounded, drawn from letters, digits, `-`, and `.`, with no consecutive
dots and no leading or trailing dot. Any other form (including plain `http://`)
is rejected with `kInvalidOrigin`.

## Document-scoped versus selector-scoped rules

`IsDocumentScoped` marks `kReadingMode`, `kIncreaseContrast`, `kReduceMotion`,
`kContentWidth`, and `kDensity` as whole-document adjustments that take no
selectors; supplying selectors for them is `kSelectorNotAllowed`. Every other
kind requires at least one selector (`kSelectorRequired`). Numeric ranges are
checked per kind: font-size scale in 0.5 to 2.0, content width in 320 to 2000,
and line spacing in 1.0 to 3.0. Colors must be `#rgb`, `#rrggbb`, or `#rrggbbaa`;
font families are a bounded alphanumeric-plus-space-and-dash name.

## No arbitrary JavaScript, injection rejected at import

The module header and the compiler are explicit that a layer never carries or
generates JavaScript. `SiteLayerFromValue` parses an imported layer and then runs
`ValidateSiteLayer` (which runs `ValidateAdjustment` and therefore `IsSafeSelector`
and the value checks) before returning, so a layer whose selectors or values
attempt to break out of a declaration is rejected at import time rather than
compiled. `SiteLayerToValue` and `SiteLayerFromValue` form the round trip.

## Registry and page resolution

`SiteLayerRegistry` stores validated layers by id, rejects over-limit inserts,
and resolves CSS for a page origin plus optional Scene id. Disabled layers are
ignored. A layer with no `scene_scope` applies globally to matching origins; a
layer with `scene_scope` applies only when that Scene is active. Matching is
deterministic: exact `https://host[:port]` patterns require the same host and
port, while `*.host` matches the host and its subdomains.
