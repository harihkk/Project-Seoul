// Project Seoul workspace projection engine V0.

#ifndef SEOUL_BROWSER_PROJECTION_PROJECTION_ERRORS_H_
#define SEOUL_BROWSER_PROJECTION_PROJECTION_ERRORS_H_

#include "base/types/expected.h"

namespace seoul {

enum class ProjectionError {
  kInvalidWindow,
  kInvalidWorkspace,
  kArchivedWorkspace,
  kMissingWorkspace,
  kReconciliationRequired,
  kLifecycleDegraded,
  kActivationFailed,
  kTargetTabDisappeared,
  kConcurrentSwitch,
  kShutdown,
};

const char* ProjectionErrorToString(ProjectionError error);

template <typename T>
using ProjectionResult = base::expected<T, ProjectionError>;

using ProjectionStatusResult = base::expected<void, ProjectionError>;

inline ProjectionStatusResult ProjectionOk() {
  return base::ok();
}

inline base::unexpected<ProjectionError> ProjectionErr(ProjectionError error) {
  return base::unexpected(error);
}

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_PROJECTION_ERRORS_H_
