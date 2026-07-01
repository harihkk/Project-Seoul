// Project Seoul Adaptive UI (SAUI).
// Parsing and serialization for adaptive surface documents. Parse consumes
// untrusted generator output (base::Value from JSON) and produces a typed,
// bounded AdaptiveSurface or a precise error. Structural rules (charsets,
// limits, primitive-only values, URL schemes) are enforced here; semantic
// rules (bindings, chart requirements, action references) live in
// saui_validator.h.

#ifndef SEOUL_BROWSER_SAUI_SAUI_DOCUMENT_H_
#define SEOUL_BROWSER_SAUI_SAUI_DOCUMENT_H_

#include "base/values.h"
#include "seoul/browser/saui/saui_errors.h"
#include "seoul/browser/saui/saui_types.h"

namespace seoul {

// True when `id` is a legal component/action/data-entry identifier:
// [A-Za-z][A-Za-z0-9_-]{0,63}.
bool IsValidSauiIdentifier(std::string_view id);

// True when `key` is a legal property key: [a-z][a-z0-9_]{0,39}, not an
// event-handler-shaped key ("on...") and not a markup-injection key ("html",
// "script", "srcdoc", "innerhtml", "style").
bool IsValidPropKey(std::string_view key);

// Parses one untrusted surface document. On success the surface is
// structurally sound; run ValidateSurface before rendering. If the document
// carries no valid "id", a fresh SurfaceId is generated.
SauiResult<AdaptiveSurface> ParseSurface(const base::Value& value);

// Structural building blocks shared with the patch parser (saui_patch.h).
// Each enforces the same charset, primitive-only, and bound rules as
// ParseSurface.
SauiStatusResult ValidateSurfacePropsDict(const base::Value::Dict& props);
SauiResult<ComponentNode> ParseComponentValue(const base::Value::Dict& dict);
SauiResult<DataEntry> ParseDataEntryValue(const base::Value::Dict& dict);
SauiResult<SurfaceAction> ParseActionValue(const base::Value::Dict& dict);

// Serializes a surface for persistence (pinning, Scene attachment, session
// restore). ParseSurface(SurfaceToValue(s)) reproduces `s` except that
// revision resets for a re-parsed document.
base::Value::Dict SurfaceToValue(const AdaptiveSurface& surface);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_DOCUMENT_H_
