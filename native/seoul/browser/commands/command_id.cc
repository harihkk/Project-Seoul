// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/command_id.h"

#include "base/strings/string_number_conversions.h"

namespace seoul {

// static
CommandId CommandId::Next() {
  // STATE OWNERSHIP: a per-process monotonic command-id counter.
  //   lifetime/persistence: per-worker ephemeral; ids are transient
  //     correlation tokens and are intentionally not durable across restarts.
  //   bounds: uint64 monotonic; not user-growable.
  //   recovery/teardown: none needed (a fresh process restarts at 1; ids are
  //     never compared across process lifetimes).
  static uint64_t next = 1;
  return CommandId(next++);
}

std::string CommandId::ToString() const {
  return "cmd-" + base::NumberToString(value_);
}

}  // namespace seoul
