// Project Seoul native browser shell V0.

#include "seoul/browser/shell/shell_errors.h"

namespace seoul {

const char* ShellErrorToString(ShellError error) {
  switch (error) {
    case ShellError::kInvalidWindow:
      return "invalid_window";
    case ShellError::kMutationRejected:
      return "mutation_rejected";
    case ShellError::kCommandRejected:
      return "command_rejected";
    case ShellError::kSwitchFailed:
      return "switch_failed";
    case ShellError::kConcurrentSwitch:
      return "concurrent_switch";
    case ShellError::kShutdown:
      return "shutdown";
  }
  return "unknown";
}

}  // namespace seoul
