// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_EXPECTED_OBSERVATION_H_
#define SEOUL_BROWSER_COMMANDS_EXPECTED_OBSERVATION_H_

#include "base/time/time.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/commands/command_types.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"

namespace seoul {

struct ExpectedObservation {
  CommandId command_id;
  CommandKind kind = CommandKind::kActivateTab;
  LiveWindowKey window;
  LiveTabKey tab;
  LiveTabKey split_pane_b;
  NormalizedEventType expected_event = NormalizedEventType::kActiveTabChanged;
  CommandStatus status = CommandStatus::kAwaitingObservation;
  base::TimeTicks created_at;
  int destination_index = -1;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_EXPECTED_OBSERVATION_H_
