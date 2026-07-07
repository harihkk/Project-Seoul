// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_COMMAND_COMPLETION_OBSERVER_H_
#define SEOUL_BROWSER_COMMANDS_COMMAND_COMPLETION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/commands/command_types.h"

namespace seoul {

// base::ObserverList requires checked observers, which detect a destroyed
// observer that was never removed instead of silently using freed memory.
class CommandCompletionObserver : public base::CheckedObserver {
 public:
  ~CommandCompletionObserver() override = default;
  virtual void OnCommandCompleted(CommandId id,
                                  CommandKind kind,
                                  CommandStatus status) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_COMMAND_COMPLETION_OBSERVER_H_
