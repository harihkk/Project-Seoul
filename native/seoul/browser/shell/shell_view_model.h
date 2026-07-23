// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_SHELL_VIEW_MODEL_H_
#define SEOUL_BROWSER_SHELL_SHELL_VIEW_MODEL_H_

#include "seoul/browser/lifecycle/live_window_snapshot_types.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/projection/projection_types.h"
#include "seoul/browser/shell/shell_types.h"

namespace seoul {

struct ShellBuildContext {
  ShellBuildContext();
  ShellBuildContext(const ShellBuildContext&);
  ShellBuildContext(ShellBuildContext&&);
  ShellBuildContext& operator=(const ShellBuildContext&);
  ShellBuildContext& operator=(ShellBuildContext&&);
  ~ShellBuildContext();
  ShellWindowKey window;
  ShellMode mode = ShellMode::kExpanded;
  bool recovery_required = false;
  bool lifecycle_degraded = false;
  WorkspaceSwitchPhase switch_phase = WorkspaceSwitchPhase::kIdle;
  ShellTaskSummary tasks;
  std::vector<LiveWindowSnapshot> other_live_windows;
};

class ShellViewModel {
 public:
  static std::vector<ShellSplitCandidate> BuildSplitCandidates(
      const WindowProjection& projection,
      const LiveWindowSnapshot& live);
  static std::string TaskButtonLabel(const ShellTaskSummary& tasks,
                                     ShellMode mode);
  static std::string TaskAccessibleName(const ShellTaskSummary& tasks);
  static ShellSnapshot Build(const OrganizationModel& model,
                             const ShellBuildContext& context,
                             const WindowProjection& projection,
                             const LiveWindowSnapshot& live,
                             uint64_t revision);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_VIEW_MODEL_H_
