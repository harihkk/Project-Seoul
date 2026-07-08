// Project Seoul Site Layers.
// Validation and deterministic compilation of a Site Layer into a scoped CSS
// string. The compiler accepts only a safe selector subset and typed values;
// it rejects any input that could break out of a declaration (braces,
// semicolons, url(), expression(), @-rules, comments, backslashes, left angle
// brackets) so a layer can never inject arbitrary style or script.

#ifndef SEOUL_BROWSER_SITE_LAYERS_SITE_LAYER_COMPILER_H_
#define SEOUL_BROWSER_SITE_LAYERS_SITE_LAYER_COMPILER_H_

#include <string>

#include "base/values.h"
#include "seoul/browser/site_layers/site_layer_types.h"

namespace seoul {

// True when `selector` is in the safe subset: sequences of type, class, id,
// attribute-presence, descendant, and child combinators over a restricted
// charset. No pseudo-classes with arguments, no url()/expression(), no
// escapes, no braces or at-signs.
bool IsSafeSelector(const std::string& selector);

// True when `origin` is "https://host[:port]" or "*.host" (host label glob).
bool IsValidOriginPattern(const std::string& origin);

// Stable layer id used by registries and Scenes. Lowercase slug with digits,
// '_' and '-' allowed after the first character.
bool IsValidSiteLayerId(const std::string& id);

SiteLayerStatusResult ValidateSiteLayer(const SiteLayer& layer);

// Compiles the validated layer to a CSS string. Rules are emitted
// deterministically in adjustment order. Returns an error if the layer does not
// validate.
SiteLayerResult<std::string> CompileSiteLayer(const SiteLayer& layer);

base::DictValue SiteLayerToValue(const SiteLayer& layer);
SiteLayerResult<SiteLayer> SiteLayerFromValue(const base::Value& value);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SITE_LAYERS_SITE_LAYER_COMPILER_H_
