// Project Seoul workspace projection engine V0.

#ifndef SEOUL_BROWSER_PROJECTION_WORKSPACE_SWITCHER_H_
#define SEOUL_BROWSER_PROJECTION_WORKSPACE_SWITCHER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "seoul/browser/commands/command_completion_observer.h"
#include "seoul/browser/commands/command_executor.h"
#include "seoul/browser/lifecycle/live_window_state.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/projection/projection_errors.h"
#include "seoul/browser/projection/projection_types.h"
#include "seoul/browser/projection/window_projection_controller.h"

class Profile;

namespace seoul {

enum class WorkspaceSwitchPhase {
  kIdle,
  kValidating,
  kCalculating,
  kAwaitingActivation,
  kCommitting,
  kApplied,
  kRejected,
  kCancelled,
  kOutcomeUnknown,
};

// Direct observation of the workspace-switch transaction. Fired on every phase
// transition, including terminal phases (applied/rejected/cancelled/outcome
// unknown) before the transaction resets to idle, so a subscriber sees pending
// and failure states without depending on unrelated organization mutations.
// `error` is set only for failure terminals.
class WorkspaceSwitchObserver : public base::CheckedObserver {
 public:
  virtual void OnWorkspaceSwitchPhaseChanged(
      WorkspaceSwitchPhase phase,
      std::optional<ProjectionError> error) = 0;
};

struct WorkspaceSwitchResult {
  ProjectionStatusResult status;
  WindowProjection committed_projection;
  WorkspaceSwitchPhase phase = WorkspaceSwitchPhase::kIdle;
  bool empty_workspace = false;
};

class WorkspaceSwitcher : public CommandCompletionObserver {
 public:
  WorkspaceSwitcher(Profile* profile,
                    OrganizationModel* model,
                    CommandExecutor* executor,
                    WindowProjectionController* controller,
                    LiveWindowStateProvider* live_state);
  WorkspaceSwitcher(const WorkspaceSwitcher&) = delete;
  WorkspaceSwitcher& operator=(const WorkspaceSwitcher&) = delete;
  ~WorkspaceSwitcher() override;

  ProjectionResult<WorkspaceSwitchResult> SwitchWorkspaceForWindow(
      WorkspaceId target_workspace);

  ProjectionResult<WorkspaceSwitchResult>
  SwitchWorkspaceForWindowExternalActivation(WorkspaceId target_workspace,
                                             LiveTabKey activated_tab);

  WorkspaceSwitchPhase phase() const { return phase_; }

  void AddObserver(WorkspaceSwitchObserver* observer);
  void RemoveObserver(WorkspaceSwitchObserver* observer);

  void OnCommandCompleted(CommandId id,
                          CommandKind kind,
                          CommandStatus status) override;

 private:
  // Assigns the phase and notifies observers. `error` is only meaningful for
  // failure terminals.
  void SetPhase(WorkspaceSwitchPhase phase,
                std::optional<ProjectionError> error = std::nullopt);
  WindowProjection ComputeTargetProjection(WorkspaceId target) const;
  ProjectionResult<WorkspaceSwitchResult> CommitWorkspace(
      WorkspaceId target_workspace,
      const WindowProjection& target_projection);
  ProjectionResult<WorkspaceSwitchResult> RejectSwitch(ProjectionError error);
  void ResetTransaction();
  bool IsTargetTabActive(LiveTabKey tab) const;

  raw_ptr<Profile> profile_;
  raw_ptr<OrganizationModel> model_;
  raw_ptr<CommandExecutor> executor_;
  raw_ptr<WindowProjectionController> controller_;
  raw_ptr<LiveWindowStateProvider> live_state_;
  WorkspaceSwitchPhase phase_ = WorkspaceSwitchPhase::kIdle;
  base::ObserverList<WorkspaceSwitchObserver> observers_;
  WorkspaceId pending_target_;
  WorkspaceId prior_workspace_;
  LiveTabKey pending_activation_tab_;
  CommandId pending_command_id_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_WORKSPACE_SWITCHER_H_
