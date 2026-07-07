// Project Seoul native lifecycle bridge.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIVE_WINDOW_STATE_H_
#define SEOUL_BROWSER_LIFECYCLE_LIVE_WINDOW_STATE_H_

#include <map>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "seoul/browser/lifecycle/live_window_snapshot_types.h"

class TabStripModel;

namespace seoul {

class LiveWindowStateObserver : public base::CheckedObserver {
 public:
  ~LiveWindowStateObserver() override = default;
  virtual void OnLiveWindowSnapshotChanged(const LiveWindowSnapshot& snapshot) {
  }
  virtual void OnLiveWindowRemoved(LiveWindowKey window) {}
};

class LiveWindowStateProvider {
 public:
  LiveWindowStateProvider();
  LiveWindowStateProvider(const LiveWindowStateProvider&) = delete;
  LiveWindowStateProvider& operator=(const LiveWindowStateProvider&) = delete;
  ~LiveWindowStateProvider();

  void SetLifecycleDegraded(bool degraded);

  std::optional<LiveWindowSnapshot> GetSnapshot(LiveWindowKey window) const;
  // Keys of every window with a published snapshot, in deterministic order.
  std::vector<LiveWindowKey> Windows() const;
  void PublishSnapshot(LiveWindowKey window, TabStripModel* model);
  void RemoveWindow(LiveWindowKey window);

  void AddObserver(LiveWindowStateObserver* observer);
  void RemoveObserver(LiveWindowStateObserver* observer);

  void SetSnapshotForTesting(LiveWindowKey window, LiveWindowSnapshot snapshot);

 private:
  LiveWindowSnapshot BuildSnapshot(LiveWindowKey window,
                                   TabStripModel* model) const;
  void NotifySnapshot(const LiveWindowSnapshot& snapshot);

  bool lifecycle_degraded_ = false;
  std::map<LiveWindowKey, LiveWindowSnapshot> snapshots_;
  uint64_t next_revision_ = 1;
  base::ObserverList<LiveWindowStateObserver> observers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIVE_WINDOW_STATE_H_
