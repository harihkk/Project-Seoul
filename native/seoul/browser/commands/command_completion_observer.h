// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_COMMAND_COMPLETION_OBSERVER_H_
#define SEOUL_BROWSER_COMMANDS_COMMAND_COMPLETION_OBSERVER_H_

#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/commands/command_types.h"

namespace seoul {

class CommandCompletionObserver {
 public:
  virtual ~CommandCompletionObserver() = default;
  virtual void OnCommandCompleted(CommandId id,
                                  CommandKind kind,
                                  CommandStatus status) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_COMMAND_COMPLETION_OBSERVER_H_
