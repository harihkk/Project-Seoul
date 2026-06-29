// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/window_projection_controller.h"

#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/projection/projection_calculator.h"

namespace seoul {
namespace {

bool ProjectionsEqual(const WindowProjection& a, const WindowProjection& b) {
  return a.window == b.window && a.active_workspace == b.active_workspace &&
         a.status == b.status && a.empty_workspace == b.empty_workspace &&
         a.active_tab == b.active_tab && a.tabs.size() == b.tabs.size() &&
         a.splits.size() == b.splits.size() && a.generation == b.generation;
}

}  // namespace

WindowProjectionController::WindowProjectionController(
    LiveWindowKey window,
    OrganizationModel* model,
    LifecycleCoordinator* lifecycle)
    : window_(window),
      model_(model),
      lifecycle_(lifecycle),
      generation_(ProjectionGeneration(1)) {
  projection_.window = window_;
}

WindowProjectionController::~WindowProjectionController() = default;

void WindowProjectionController::UpdateLiveState(
    const LiveWindowTabState& live) {
  if (shutting_down_) {
    return;
  }
  live_ = live;
  Recompute(true);
}

void WindowProjectionController::OnOrganizationChanged() {
  if (shutting_down_) {
    return;
  }
  Recompute(true);
}

void WindowProjectionController::OnLifecycleDegraded(bool degraded) {
  if (shutting_down_) {
    return;
  }
  if (degraded) {
    projection_.status = ProjectionStatus::kReconciliationRequired;
    Publish(ProjectionChangeType::kDegradedStateEntered);
    return;
  }
  Recompute(true);
}

void WindowProjectionController::EnterFailOpen() {
  fail_open_ = true;
  Recompute(true);
}

void WindowProjectionController::ClearFailOpen() {
  fail_open_ = false;
  Recompute(true);
}

void WindowProjectionController::Shutdown() {
  shutting_down_ = true;
  observers_.Clear();
}

void WindowProjectionController::AddObserver(ProjectionObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowProjectionController::RemoveObserver(ProjectionObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowProjectionController::Recompute(bool publish) {
  if (!model_ || shutting_down_) {
    return;
  }
  if (lifecycle_ && lifecycle_->lifecycle_degraded() && !fail_open_) {
    projection_.status = ProjectionStatus::kReconciliationRequired;
    if (publish) {
      Publish(ProjectionChangeType::kDegradedStateEntered);
    }
    return;
  }
  const WorkspaceId active = model_->ActiveWorkspaceForWindow(window_.value());
  generation_ = generation_.Next();
  WindowProjection next = ProjectionCalculator::Compute(
      *model_, live_, active, generation_, fail_open_);
  if (!fail_open_ && live_.active_tab.is_valid() &&
      next.status != ProjectionStatus::kFailOpen) {
    bool active_projected = false;
    for (const ProjectedTab& pt : next.tabs) {
      if (pt.tab == live_.active_tab) {
        active_projected = true;
        break;
      }
    }
    if (!active_projected) {
      fail_open_ = true;
      generation_ = generation_.Next();
      next = ProjectionCalculator::Compute(*model_, live_, active, generation_,
                                           true);
    }
  }
  if (publish && ProjectionsEqual(projection_, next)) {
    return;
  }
  projection_ = std::move(next);
  if (publish) {
    Publish(ProjectionChangeType::kProjectionUpdated);
  }
}

void WindowProjectionController::Publish(ProjectionChangeType type) {
  ProjectionChange change;
  change.type = type;
  change.window = window_;
  change.generation = generation_;
  for (ProjectionObserver& observer : observers_) {
    observer.OnProjectionChanged(change, projection_);
  }
}

}  // namespace seoul
