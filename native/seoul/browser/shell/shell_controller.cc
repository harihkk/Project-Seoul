// Project Seoul native browser shell V0.

#include "seoul/browser/shell/shell_controller.h"

#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/commands/model_command_facade.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/organization/organization_errors.h"
#include "seoul/browser/projection/projection_service.h"
#include "seoul/browser/projection/workspace_switcher.h"
#include "seoul/browser/shell/shell_view_model.h"
#include "url/gurl.h"

namespace seoul {

ShellController::ShellController(ShellWindowKey window,
                                 Profile* profile,
                                 OrganizationModel* model,
                                 ProjectionService* projection_service,
                                 LiveWindowStateProvider* live_state,
                                 CommandExecutor* executor,
                                 LifecycleCoordinator* lifecycle,
                                 bool recovery_required)
    : window_(window),
      profile_(profile),
      model_(model),
      projection_service_(projection_service),
      live_state_(live_state),
      executor_(executor),
      lifecycle_(lifecycle),
      recovery_required_(recovery_required) {
  if (live_state_) {
    live_state_->AddObserver(this);
  }
  if (model_) {
    model_->AddObserver(this);
  }
  WindowProjectionController* controller =
      projection_service_ ? projection_service_->GetController(window_)
                          : nullptr;
  if (controller) {
    controller->AddObserver(this);
  }
  Recompute(true);
}

ShellController::~ShellController() {
  Shutdown();
}

void ShellController::Shutdown() {
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;
  if (live_state_) {
    live_state_->RemoveObserver(this);
  }
  if (model_) {
    model_->RemoveObserver(this);
  }
  if (projection_service_) {
    WindowProjectionController* controller =
        projection_service_->GetController(window_);
    if (controller) {
      controller->RemoveObserver(this);
    }
  }
  observers_.Clear();
}

void ShellController::AddObserver(ShellObserver* observer) {
  observers_.AddObserver(observer);
}

void ShellController::RemoveObserver(ShellObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ShellController::SetCollapsed(bool collapsed) {
  if (collapsed_ == collapsed) {
    return;
  }
  collapsed_ = collapsed;
  Recompute(true);
}

bool ShellController::SnapshotsEqual(const ShellSnapshot& a,
                                     const ShellSnapshot& b) const {
  return a.revision == b.revision && a.status == b.status && a.mode == b.mode &&
         a.show_empty_workspace == b.show_empty_workspace &&
         a.show_status_banner == b.show_status_banner &&
         a.status_message == b.status_message &&
         a.workspace.workspace_id == b.workspace.workspace_id &&
         a.workspace.name == b.workspace.name &&
         a.switch_phase == b.switch_phase;
}

void ShellController::Recompute(bool publish) {
  if (shutting_down_ || !model_) {
    return;
  }
  ShellBuildContext context;
  context.window = window_;
  context.mode = collapsed_ ? ShellMode::kCollapsed : ShellMode::kExpanded;
  context.recovery_required = recovery_required_;
  context.lifecycle_degraded = lifecycle_ && lifecycle_->lifecycle_degraded();
  WorkspaceSwitcher* switcher =
      projection_service_ ? projection_service_->GetSwitcher(window_) : nullptr;
  context.switch_phase =
      switcher ? switcher->phase() : WorkspaceSwitchPhase::kIdle;

  WindowProjection projection;
  if (projection_service_) {
    WindowProjectionController* controller =
        projection_service_->GetController(window_);
    if (controller) {
      projection = controller->projection();
    }
  }
  if (!live_.window.is_valid() && live_state_) {
    if (auto snap = live_state_->GetSnapshot(window_)) {
      live_ = *snap;
    }
  }

  ShellSnapshot next =
      ShellViewModel::Build(*model_, context, projection, live_, revision_++);
  if (!publish || !SnapshotsEqual(snapshot_, next)) {
    snapshot_ = std::move(next);
    if (publish) {
      Publish();
    }
  }
}

void ShellController::Publish() {
  ShellChange change;
  change.window = window_;
  change.revision = snapshot_.revision;
  for (ShellObserver& observer : observers_) {
    observer.OnShellSnapshotChanged(change, snapshot_);
  }
}

void ShellController::OnOrganizationChanged(const OrganizationChange& change) {
  (void)change;
  Recompute(true);
}

void ShellController::OnLiveWindowSnapshotChanged(
    const LiveWindowSnapshot& snapshot) {
  if (snapshot.window != window_) {
    return;
  }
  live_ = snapshot;
  Recompute(true);
}

void ShellController::OnLiveWindowRemoved(LiveWindowKey window) {
  if (window == window_) {
    Shutdown();
  }
}

void ShellController::OnProjectionChanged(const ProjectionChange& change,
                                          const WindowProjection& projection) {
  (void)change;
  (void)projection;
  Recompute(true);
}

ShellResult<WorkspaceId> ShellController::SwitchWorkspace(WorkspaceId target) {
  if (shutting_down_ || !projection_service_) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  WorkspaceSwitcher* switcher = projection_service_->GetSwitcher(window_);
  if (!switcher) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  auto result = switcher->SwitchWorkspaceForWindow(target);
  Recompute(true);
  if (!result.has_value()) {
    return ShellErr(ShellError::kSwitchFailed);
  }
  if (result->phase == WorkspaceSwitchPhase::kAwaitingActivation) {
    return target;
  }
  return target;
}

ShellStatusResult ShellController::OpenNewTemporaryTab() {
  if (shutting_down_ || !executor_) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = CommandKind::kOpenTemporaryTab;
  command.window = window_;
  command.url = GURL();
  auto result = executor_->Submit(std::move(command));
  Recompute(true);
  return result.has_value() ? ShellOk()
                            : ShellErr(ShellError::kCommandRejected);
}

ShellStatusResult ShellController::CreateSplitFromActive() {
  if (shutting_down_ || !executor_ || !projection_service_) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  const WindowProjection& projection =
      projection_service_->GetController(window_)->projection();
  LiveTabKey active = projection.active_tab;
  LiveTabKey partner;
  for (const ProjectedTab& tab : projection.tabs) {
    if (tab.tab != active) {
      partner = tab.tab;
      break;
    }
  }
  if (!active.is_valid() || !partner.is_valid()) {
    return ShellErr(ShellError::kCommandRejected);
  }
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = CommandKind::kCreateSplit;
  command.window = window_;
  command.tab = active;
  command.split_pane_b = partner;
  auto result = executor_->Submit(std::move(command));
  Recompute(true);
  return result.has_value() ? ShellOk()
                            : ShellErr(ShellError::kCommandRejected);
}

ShellStatusResult ShellController::RunReconciliation() {
  if (shutting_down_ || !lifecycle_) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  if (lifecycle_->reconciliation_required()) {
    NormalizedEvent began;
    began.type = NormalizedEventType::kReconciliationBegan;
    lifecycle_->OnNormalizedEvent(began);
    NormalizedEvent completed;
    completed.type = NormalizedEventType::kReconciliationCompleted;
    lifecycle_->OnNormalizedEvent(completed);
  }
  Recompute(true);
  return ShellOk();
}

ShellStatusResult ShellController::AcknowledgeRecovery() {
  if (acknowledge_recovery_callback_) {
    auto result = acknowledge_recovery_callback_.Run();
    recovery_required_ = !result.has_value();
    Recompute(true);
    return result.has_value() ? ShellOk()
                              : ShellErr(ShellError::kMutationRejected);
  }
  return ShellErr(ShellError::kMutationRejected);
}

void ShellController::SetAcknowledgeRecoveryCallback(
    base::RepeatingCallback<MutationStatus()> callback) {
  acknowledge_recovery_callback_ = std::move(callback);
}

ShellStatusResult ShellController::OpenEssential(const EssentialId& id) {
  if (shutting_down_ || !executor_ || !model_) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  const EssentialRecord* essential = model_->FindEssential(id);
  if (!essential || essential->root_url.empty()) {
    return ShellErr(ShellError::kCommandRejected);
  }
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = CommandKind::kOpenRetainedTab;
  command.window = window_;
  command.url = GURL(essential->root_url);
  auto result = executor_->Submit(std::move(command));
  Recompute(true);
  return result.has_value() ? ShellOk()
                            : ShellErr(ShellError::kCommandRejected);
}

ShellStatusResult ShellController::DispatchModelCommand(
    BrowserCommand command) {
  if (shutting_down_ || !model_) {
    return ShellErr(ShellError::kInvalidWindow);
  }
  ModelCommandFacade facade(model_);
  auto result = facade.Execute(command);
  Recompute(true);
  return result.has_value() ? ShellOk()
                            : ShellErr(ShellError::kMutationRejected);
}

}  // namespace seoul
