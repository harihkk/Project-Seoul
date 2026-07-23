// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/browser_command.h"

namespace seoul {

BrowserCommand::BrowserCommand() = default;
BrowserCommand::BrowserCommand(const BrowserCommand&) = default;
BrowserCommand::BrowserCommand(BrowserCommand&&) = default;
BrowserCommand& BrowserCommand::operator=(const BrowserCommand&) = default;
BrowserCommand& BrowserCommand::operator=(BrowserCommand&&) = default;
BrowserCommand::~BrowserCommand() = default;

}  // namespace seoul
