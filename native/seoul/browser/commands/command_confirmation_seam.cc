// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/command_confirmation_seam.h"

#include "seoul/browser/commands/command_executor.h"

namespace seoul {

CommandConfirmationSeam::CommandConfirmationSeam(CommandExecutor* executor)
    : executor_(executor) {}

CommandConfirmationSeam::~CommandConfirmationSeam() = default;

void CommandConfirmationSeam::OnNormalizedEvent(const NormalizedEvent& event) {
  if (!executor_) {
    return;
  }
  if (event.type == NormalizedEventType::kTabCloseCancelled &&
      event.tab.is_valid()) {
    executor_->OnCloseCancelled(event.tab);
  }
  executor_->OnNormalizedLifecycleEvent(event);
}

}  // namespace seoul
