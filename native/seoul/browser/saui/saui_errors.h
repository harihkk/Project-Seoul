// Project Seoul Adaptive UI (SAUI).
// Precise validation and patch errors. Every rejected document or operation
// reports a specific cause; there is no generic failure and no partial accept.

#ifndef SEOUL_BROWSER_SAUI_SAUI_ERRORS_H_
#define SEOUL_BROWSER_SAUI_SAUI_ERRORS_H_

#include "base/types/expected.h"

namespace seoul {

enum class SauiError {
  kInvalidDocument,
  kUnsupportedSchemaVersion,
  kUnknownSurfaceKind,
  kInvalidTitle,
  kEmptySurface,
  kLimitExceeded,
  kDepthExceeded,
  kUnknownComponentType,
  kInvalidComponentId,
  kDuplicateComponentId,
  kChildrenNotAllowed,
  kInvalidPropertyKey,
  kForbiddenPropertyKey,
  kInvalidPropertyValue,
  kMissingRequiredProperty,
  kInvalidUrlProperty,
  kMissingAccessibleName,
  kUnknownDataEntry,
  kInvalidDataEntry,
  kBindingKindMismatch,
  kMissingRequiredBinding,
  kMissingProvenance,
  kChartRequirementMissing,
  kTruncatedAxisNotIndicated,
  kInvalidAction,
  kDuplicateActionId,
  kUnknownActionReference,
  kInvalidState,
  kInvalidPatch,
  kPatchTargetMissing,
  kPatchLimitExceeded,
};

const char* SauiErrorToString(SauiError error);

template <typename T>
using SauiResult = base::expected<T, SauiError>;

using SauiStatusResult = base::expected<void, SauiError>;

inline SauiStatusResult SauiOk() {
  return base::ok();
}

inline base::unexpected<SauiError> SauiErr(SauiError error) {
  return base::unexpected(error);
}

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_ERRORS_H_
