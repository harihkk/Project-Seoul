// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/command_errors.h"

namespace seoul {

const char* CommandErrorToString(CommandError error) {
  switch (error) {
    case CommandError::kInvalidCommand:
      return "invalid command";
    case CommandError::kUnsupportedCommand:
      return "unsupported command";
    case CommandError::kInvalidUrl:
      return "invalid url";
    case CommandError::kUnsupportedUrlScheme:
      return "unsupported url scheme";
    case CommandError::kProfileMismatch:
      return "profile mismatch";
    case CommandError::kWindowNotFound:
      return "window not found";
    case CommandError::kTabNotFound:
      return "tab not found";
    case CommandError::kStaleTabReference:
      return "stale tab reference";
    case CommandError::kTargetDisappeared:
      return "target disappeared";
    case CommandError::kInvalidDestination:
      return "invalid destination";
    case CommandError::kPinnedBoundaryViolation:
      return "pinned boundary violation";
    case CommandError::kSplitPreconditionFailure:
      return "split precondition failure";
    case CommandError::kDuplicateCommandId:
      return "duplicate command id";
    case CommandError::kConflictingCommand:
      return "conflicting command";
    case CommandError::kCapacityExceeded:
      return "capacity exceeded";
    case CommandError::kDispatchFailure:
      return "dispatch failure";
    case CommandError::kCloseCancelled:
      return "close cancelled";
    case CommandError::kObservationMismatch:
      return "observation mismatch";
    case CommandError::kReconciliationRequired:
      return "reconciliation required";
    case CommandError::kLifecycleQueueDegraded:
      return "lifecycle queue degraded";
    case CommandError::kShutdown:
      return "shutdown";
    case CommandError::kOutcomeUnknown:
      return "outcome unknown";
  }
  return "unknown command error";
}

}  // namespace seoul
