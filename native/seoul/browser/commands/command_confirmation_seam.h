// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_COMMAND_CONFIRMATION_SEAM_H_
#define SEOUL_BROWSER_COMMANDS_COMMAND_CONFIRMATION_SEAM_H_

#include "base/memory/raw_ptr.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"

namespace seoul {

class CommandExecutor;

class CommandConfirmationSeam {
 public:
  explicit CommandConfirmationSeam(CommandExecutor* executor);
  CommandConfirmationSeam(const CommandConfirmationSeam&) = delete;
  CommandConfirmationSeam& operator=(const CommandConfirmationSeam&) = delete;
  ~CommandConfirmationSeam();

  void OnNormalizedEvent(const NormalizedEvent& event);

 private:
  raw_ptr<CommandExecutor> executor_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_COMMAND_CONFIRMATION_SEAM_H_
