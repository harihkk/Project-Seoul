// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/projection_service.h"

#include "seoul/browser/commands/command_executor.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/organization/organization_model.h"
#include "ui/views/view.h"

namespace seoul {

ProjectionService::ProjectionService(Profile* profile,
                                     OrganizationModel* model,
                                     LifecycleCoordinator* lifecycle,
                                     CommandExecutor* executor,
                                     LiveWindowStateProvider* live_state)
    : profile_(profile),
      model_(model),
      lifecycle_(lifecycle),
      executor_(executor),
      live_state_(live_state) {
  if (live_state_) {
    live_state_->AddObserver(this);
  }
}

ProjectionService::~ProjectionService() {
  if (live_state_) {
    live_state_->RemoveObserver(this);
  }
}

WindowProjectionController* ProjectionService::GetController(
    LiveWindowKey window) {
  auto it = controllers_.find(window);
  return it != controllers_.end() ? it->second.get() : nullptr;
}

WorkspaceSwitcher* ProjectionService::GetSwitcher(LiveWindowKey window) {
  auto it = switchers_.find(window);
  return it != switchers_.end() ? it->second.get() : nullptr;
}

WindowProjectionController& ProjectionService::EnsureController(
    LiveWindowKey window) {
  auto it = controllers_.find(window);
  if (it == controllers_.end()) {
    auto controller = std::make_unique<WindowProjectionController>(
        window, model_, lifecycle_);
    controller->AddObserver(this);
    auto switcher = std::make_unique<WorkspaceSwitcher>(
        profile_, model_, executor_, controller.get(), live_state_);
    switchers_[window] = std::move(switcher);
    it = controllers_.emplace(window, std::move(controller)).first;
  }
  return *it->second;
}

void ProjectionService::RegisterVerticalRegion(
    LiveWindowKey window,
    VerticalTabStripRegionView* region) {
  if (shutting_down_ || !window.is_valid() || !region) {
    return;
  }
  regions_[window] = region;
  WindowProjectionController* controller = GetController(window);
  if (controller) {
    ApplyProjectionToViews(window, controller->projection());
  }
}

void ProjectionService::UnregisterVerticalRegion(LiveWindowKey window) {
  regions_.erase(window);
  adapters_.erase(window);
}

views::View* ProjectionService::GetDefaultFocusableChildForWindow(
    LiveWindowKey window) {
  auto region_it = regions_.find(window);
  WindowProjectionController* controller = GetController(window);
  if (region_it == regions_.end() || !region_it->second || !controller) {
    return nullptr;
  }
  return VerticalPresentationAdapter::FindDefaultFocusableChild(
      region_it->second, controller->projection());
}

void ProjectionService::MaybeHandleHiddenActiveTab(
    LiveWindowKey window,
    LiveTabKey tab,
    const WindowProjection& projection) {
  if (shutting_down_ || !tab.is_valid() ||
      projection.status == ProjectionStatus::kFailOpen) {
    return;
  }
  bool projected = false;
  for (const ProjectedTab& pt : projection.tabs) {
    if (pt.tab == tab) {
      projected = true;
      break;
    }
  }
  if (!projected) {
    OnHiddenTabActivated(window, tab);
  }
}

void ProjectionService::OnLiveWindowSnapshotChanged(
    const LiveWindowSnapshot& snapshot) {
  if (shutting_down_ || !snapshot.window.is_valid()) {
    return;
  }
  if (snapshot.lifecycle_degraded) {
    HandleLifecycleDegraded();
  }
  WindowProjectionController& controller = EnsureController(snapshot.window);
  LiveWindowTabState live = snapshot;
  controller.UpdateLiveState(live);
  MaybeHandleHiddenActiveTab(snapshot.window, snapshot.active_tab,
                             controller.projection());
}

void ProjectionService::OnLiveWindowRemoved(LiveWindowKey window) {
  if (auto it = controllers_.find(window); it != controllers_.end()) {
    it->second->RemoveObserver(this);
  }
  controllers_.erase(window);
  switchers_.erase(window);
  regions_.erase(window);
  adapters_.erase(window);
}

void ProjectionService::OnOrganizationChanged(
    const OrganizationChange& change) {
  (void)change;
  if (shutting_down_) {
    return;
  }
  for (auto& [key, controller] : controllers_) {
    controller->OnOrganizationChanged();
  }
}

void ProjectionService::OnProjectionChanged(
    const ProjectionChange& change,
    const WindowProjection& projection) {
  if (shutting_down_) {
    return;
  }
  ApplyProjectionToViews(change.window, projection);
}

void ProjectionService::ApplyProjectionToViews(
    LiveWindowKey window,
    const WindowProjection& projection) {
  auto region_it = regions_.find(window);
  if (region_it == regions_.end() || !region_it->second) {
    return;
  }
  VerticalPresentationAdapter& adapter = adapters_[window];
  adapter.UpdateProjection(projection);
  adapter.ApplyToVerticalTabStripRegion(region_it->second);
}

void ProjectionService::HandleLifecycleDegraded() {
  if (live_state_) {
    live_state_->SetLifecycleDegraded(true);
  }
  for (auto& [key, controller] : controllers_) {
    controller->EnterFailOpen();
  }
}

void ProjectionService::OnHiddenTabActivated(LiveWindowKey window,
                                             LiveTabKey tab) {
  if (shutting_down_ || !model_ || !tab.is_valid()) {
    return;
  }
  const TabMembershipId id = model_->FindMembershipIdByTabKey(tab.value());
  const TabMembershipRecord* m =
      id.is_valid() ? model_->FindMembership(id) : nullptr;
  if (!m || !m->workspace_id.is_valid()) {
    EnsureController(window).EnterFailOpen();
    return;
  }
  WorkspaceSwitcher* switcher = GetSwitcher(window);
  if (!switcher) {
    switcher =
        &*switchers_
              .emplace(window, std::make_unique<WorkspaceSwitcher>(
                                   profile_, model_, executor_,
                                   &EnsureController(window), live_state_))
              .first->second;
  }
  (void)switcher->SwitchWorkspaceForWindowExternalActivation(m->workspace_id,
                                                             tab);
}

void ProjectionService::Shutdown() {
  shutting_down_ = true;
  if (live_state_) {
    live_state_->RemoveObserver(this);
  }
  for (auto& [key, controller] : controllers_) {
    controller->RemoveObserver(this);
    controller->Shutdown();
  }
  controllers_.clear();
  switchers_.clear();
  regions_.clear();
  adapters_.clear();
}

}  // namespace seoul
