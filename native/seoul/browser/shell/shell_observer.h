// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_SHELL_OBSERVER_H_
#define SEOUL_BROWSER_SHELL_SHELL_OBSERVER_H_

#include "seoul/browser/shell/shell_types.h"

namespace seoul {

class ShellObserver {
 public:
  virtual ~ShellObserver() = default;
  virtual void OnShellSnapshotChanged(const ShellChange& change,
                                      const ShellSnapshot& snapshot) {}
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_OBSERVER_H_
