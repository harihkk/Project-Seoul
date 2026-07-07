// Project Seoul native lifecycle bridge.
// Observes the global SessionRestore completion signal and triggers a bounded,
// idempotent rescan of the owning profile's windows. SessionRestoreObserver is
// not profile-scoped; reconciliation filters to this profile's windows only.

#ifndef SEOUL_BROWSER_LIFECYCLE_SESSION_RESTORE_WATCHER_H_
#define SEOUL_BROWSER_LIFECYCLE_SESSION_RESTORE_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sessions/session_restore_observer.h"

class Profile;

namespace seoul {

class LifecycleCoordinator;
class WindowWatcher;

class SessionRestoreWatcher : public SessionRestoreObserver {
 public:
  SessionRestoreWatcher(Profile* profile,
                        WindowWatcher* window_watcher,
                        LifecycleCoordinator* coordinator);
  SessionRestoreWatcher(const SessionRestoreWatcher&) = delete;
  SessionRestoreWatcher& operator=(const SessionRestoreWatcher&) = delete;
  ~SessionRestoreWatcher();

  void StartObserving();
  void StopObserving();

  // SessionRestoreObserver:
  void OnSessionRestoreFinishedLoadingTabs() override;

  int reconciliation_count() const { return reconciliation_count_; }

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<WindowWatcher> window_watcher_;
  raw_ptr<LifecycleCoordinator> coordinator_;
  bool observing_ = false;
  int reconciliation_count_ = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_SESSION_RESTORE_WATCHER_H_
