// Project Seoul hybrid intelligence.
// Out-of-line special members for the provider-neutral value types. These are
// declared in the header and defaulted here so the shapes stay implicit while
// their construction and destruction have a single out-of-line home.

#include "seoul/browser/intelligence/model_provider.h"

namespace seoul {

ModelCapabilities::ModelCapabilities() = default;
ModelCapabilities::ModelCapabilities(const ModelCapabilities&) = default;
ModelCapabilities::ModelCapabilities(ModelCapabilities&&) = default;
ModelCapabilities& ModelCapabilities::operator=(const ModelCapabilities&) =
    default;
ModelCapabilities& ModelCapabilities::operator=(ModelCapabilities&&) = default;
ModelCapabilities::~ModelCapabilities() = default;

// GenerationRequest and GenerationResult hold base::Value members, which are
// move-only; only the move operations are declared, so their copy operations
// remain implicitly deleted exactly as before.
GenerationRequest::GenerationRequest() = default;
GenerationRequest::GenerationRequest(GenerationRequest&&) = default;
GenerationRequest& GenerationRequest::operator=(GenerationRequest&&) = default;
GenerationRequest::~GenerationRequest() = default;

GenerationResult::GenerationResult() = default;
GenerationResult::GenerationResult(GenerationResult&&) = default;
GenerationResult& GenerationResult::operator=(GenerationResult&&) = default;
GenerationResult::~GenerationResult() = default;

}  // namespace seoul
