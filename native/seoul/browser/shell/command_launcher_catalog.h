// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_COMMAND_LAUNCHER_CATALOG_H_
#define SEOUL_BROWSER_SHELL_COMMAND_LAUNCHER_CATALOG_H_

#include <string>
#include <vector>

#include "seoul/browser/shell/shell_types.h"

namespace seoul {

struct CommandLauncherEntry {
  std::string id;
  std::string label;
  std::vector<std::string> tokens;
  ShellUtilityAction action = ShellUtilityAction::kCommandLauncher;
  bool model_command = false;
  bool enabled = true;
  std::string disabled_reason;
};

class CommandLauncherCatalog {
 public:
  static std::vector<CommandLauncherEntry> BuildEntries(
      const ShellSnapshot& snapshot);
  static std::vector<CommandLauncherEntry> Filter(
      const std::vector<CommandLauncherEntry>& entries,
      std::string_view query);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_COMMAND_LAUNCHER_CATALOG_H_
