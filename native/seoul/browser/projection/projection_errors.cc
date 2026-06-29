// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/projection_errors.h"

namespace seoul {

const char* ProjectionErrorToString(ProjectionError error) {
  switch (error) {
    case ProjectionError::kInvalidWindow:
      return "invalid_window";
    case ProjectionError::kInvalidWorkspace:
      return "invalid_workspace";
    case ProjectionError::kArchivedWorkspace:
      return "archived_workspace";
    case ProjectionError::kMissingWorkspace:
      return "missing_workspace";
    case ProjectionError::kReconciliationRequired:
      return "reconciliation_required";
    case ProjectionError::kLifecycleDegraded:
      return "lifecycle_degraded";
    case ProjectionError::kActivationFailed:
      return "activation_failed";
    case ProjectionError::kTargetTabDisappeared:
      return "target_tab_disappeared";
    case ProjectionError::kConcurrentSwitch:
      return "concurrent_switch";
    case ProjectionError::kShutdown:
      return "shutdown";
  }
  return "unknown";
}

}  // namespace seoul
