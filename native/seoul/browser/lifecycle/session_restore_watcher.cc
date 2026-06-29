// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/session_restore_watcher.h"

#include "chrome/browser/sessions/session_restore.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/lifecycle/window_watcher.h"

namespace seoul {

SessionRestoreWatcher::SessionRestoreWatcher(Profile* profile,
                                             WindowWatcher* window_watcher,
                                             LifecycleCoordinator* coordinator)
    : profile_(profile),
      window_watcher_(window_watcher),
      coordinator_(coordinator) {}

SessionRestoreWatcher::~SessionRestoreWatcher() {
  StopObserving();
}

void SessionRestoreWatcher::StartObserving() {
  if (observing_) {
    return;
  }
  SessionRestore::AddObserver(this);
  observing_ = true;
}

void SessionRestoreWatcher::StopObserving() {
  if (!observing_) {
    return;
  }
  SessionRestore::RemoveObserver(this);
  observing_ = false;
}

void SessionRestoreWatcher::OnSessionRestoreFinishedLoadingTabs() {
  if (!window_watcher_ || !coordinator_) {
    return;
  }
  ++reconciliation_count_;
  NormalizedEvent began;
  began.type = NormalizedEventType::kReconciliationBegan;
  began.origin = MutationOrigin::kStartupReconciliation;
  coordinator_->OnNormalizedEvent(began);

  window_watcher_->RescanExistingWindows();

  NormalizedEvent completed;
  completed.type = NormalizedEventType::kReconciliationCompleted;
  completed.origin = MutationOrigin::kStartupReconciliation;
  coordinator_->OnNormalizedEvent(completed);
}

}  // namespace seoul
