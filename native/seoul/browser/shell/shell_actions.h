// Project Seoul native browser shell V0.
// Source-level action identifiers for future accelerator wiring.

#ifndef SEOUL_BROWSER_SHELL_SHELL_ACTIONS_H_
#define SEOUL_BROWSER_SHELL_SHELL_ACTIONS_H_

namespace seoul {

struct ShellActions {
  static constexpr int kNextWorkspace = 1;
  static constexpr int kPreviousWorkspace = 2;
  static constexpr int kOpenWorkspaceSwitcher = 3;
  static constexpr int kFocusWorkspaceHeader = 4;
  static constexpr int kOpenCommandLauncher = 5;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_ACTIONS_H_
