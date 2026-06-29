// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_COMMAND_ERRORS_H_
#define SEOUL_BROWSER_COMMANDS_COMMAND_ERRORS_H_

#include "base/types/expected.h"

namespace seoul {

enum class CommandError {
  kInvalidCommand,
  kUnsupportedCommand,
  kInvalidUrl,
  kUnsupportedUrlScheme,
  kProfileMismatch,
  kWindowNotFound,
  kTabNotFound,
  kStaleTabReference,
  kTargetDisappeared,
  kInvalidDestination,
  kPinnedBoundaryViolation,
  kSplitPreconditionFailure,
  kDuplicateCommandId,
  kConflictingCommand,
  kCapacityExceeded,
  kDispatchFailure,
  kCloseCancelled,
  kObservationMismatch,
  kReconciliationRequired,
  kLifecycleQueueDegraded,
  kShutdown,
  kOutcomeUnknown,
};

const char* CommandErrorToString(CommandError error);

template <typename T>
using CommandResult = base::expected<T, CommandError>;

using CommandStatusResult = base::expected<void, CommandError>;

inline CommandStatusResult CommandOk() {
  return base::ok();
}

inline base::unexpected<CommandError> CommandErr(CommandError error) {
  return base::unexpected(error);
}

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_COMMAND_ERRORS_H_
