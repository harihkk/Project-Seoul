// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_COMMAND_ID_H_
#define SEOUL_BROWSER_COMMANDS_COMMAND_ID_H_

#include <compare>
#include <cstdint>
#include <string>

namespace seoul {

class CommandId {
 public:
  CommandId() = default;
  explicit CommandId(uint64_t value) : value_(value) {}

  static CommandId Next();

  bool is_valid() const { return value_ != 0; }
  uint64_t value() const { return value_; }
  std::string ToString() const;

  friend auto operator<=>(const CommandId&, const CommandId&) = default;

 private:
  uint64_t value_ = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_COMMAND_ID_H_
