// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/command_id.h"

#include "base/strings/string_number_conversions.h"

namespace seoul {

// static
CommandId CommandId::Next() {
  static uint64_t next = 1;
  return CommandId(next++);
}

std::string CommandId::ToString() const {
  return "cmd-" + base::NumberToString(value_);
}

}  // namespace seoul
