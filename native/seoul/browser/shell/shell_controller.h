// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_SHELL_CONTROLLER_H_
#define SEOUL_BROWSER_SHELL_SHELL_CONTROLLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/lifecycle/live_window_state.h"
#include "seoul/browser/organization/organization_observer.h"
#include "seoul/browser/projection/projection_observer.h"
#include "seoul/browser/shell/shell_observer.h"
#include "seoul/browser/shell/shell_types.h"

class Profile;

namespace seoul {

class CommandExecutor;
class LifecycleCoordinator;
class ModelCommandFacade;
class OrganizationModel;
class ProjectionService;
class WorkspaceSwitcher;

class ShellController : public OrganizationModelObserver,
                        public LiveWindowStateObserver,
                        public ProjectionObserver,
                        public WorkspaceSwitchObserver {
 public:
  ShellController(ShellWindowKey window,
                  Profile* profile,
                  OrganizationModel* model,
                  ProjectionService* projection_service,
                  LiveWindowStateProvider* live_state,
                  CommandExecutor* executor,
                  LifecycleCoordinator* lifecycle,
                  bool recovery_required);
  ShellController(const ShellController&) = delete;
  ShellController& operator=(const ShellController&) = delete;
  ~ShellController() override;

  const ShellSnapshot& snapshot() const { return snapshot_; }
  OrganizationModel* model() { return model_; }
  void SetCollapsed(bool collapsed);
  void Shutdown();

  void AddObserver(ShellObserver* observer);
  void RemoveObserver(ShellObserver* observer);

  ShellResult<WorkspaceId> SwitchWorkspace(WorkspaceId target);
  ShellStatusResult OpenNewTemporaryTab();
  ShellStatusResult CreateSplitFromActive();
  ShellStatusResult RunReconciliation();
  ShellStatusResult AcknowledgeRecovery();

  void SetAcknowledgeRecoveryCallback(
      base::RepeatingCallback<MutationStatus()> callback);
  ShellStatusResult OpenEssential(const EssentialId& id);
  ShellStatusResult DispatchModelCommand(BrowserCommand command);

  void OnOrganizationChanged(const OrganizationChange& change) override;
  void OnLiveWindowSnapshotChanged(const LiveWindowSnapshot& snapshot) override;
  void OnLiveWindowRemoved(LiveWindowKey window) override;
  void OnProjectionChanged(const ProjectionChange& change,
                           const WindowProjection& projection) override;

  // WorkspaceSwitchObserver:
  void OnWorkspaceSwitchPhaseChanged(
      WorkspaceSwitchPhase phase,
      std::optional<ProjectionError> error) override;

 private:
  void Recompute(bool publish);
  void Publish();
  bool SnapshotsEqual(const ShellSnapshot& a, const ShellSnapshot& b) const;

  ShellWindowKey window_;
  raw_ptr<Profile> profile_;
  raw_ptr<OrganizationModel> model_;
  raw_ptr<ProjectionService> projection_service_;
  raw_ptr<LiveWindowStateProvider> live_state_;
  raw_ptr<CommandExecutor> executor_;
  raw_ptr<LifecycleCoordinator> lifecycle_;
  bool recovery_required_ = false;
  bool collapsed_ = false;
  bool shutting_down_ = false;
  bool initialized_ = false;  // first snapshot established + published
  // Directly observed workspace-switch transaction state (not inferred).
  WorkspaceSwitchPhase observed_switch_phase_ = WorkspaceSwitchPhase::kIdle;
  bool switch_failed_ = false;
  LiveWindowSnapshot live_;
  ShellSnapshot snapshot_;
  uint64_t revision_ = 0;
  base::RepeatingCallback<MutationStatus()> acknowledge_recovery_callback_;
  base::ObserverList<ShellObserver> observers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_CONTROLLER_H_
