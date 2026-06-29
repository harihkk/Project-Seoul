// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/live_window_state.h"

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "seoul/browser/lifecycle/tab_strip_bridge.h"

namespace seoul {

LiveWindowStateProvider::LiveWindowStateProvider() = default;
LiveWindowStateProvider::~LiveWindowStateProvider() = default;

void LiveWindowStateProvider::SetLifecycleDegraded(bool degraded) {
  lifecycle_degraded_ = degraded;
}

std::optional<LiveWindowSnapshot> LiveWindowStateProvider::GetSnapshot(
    LiveWindowKey window) const {
  auto it = snapshots_.find(window);
  if (it == snapshots_.end()) {
    return std::nullopt;
  }
  return it->second;
}

LiveWindowSnapshot LiveWindowStateProvider::BuildSnapshot(
    LiveWindowKey window,
    TabStripModel* model) const {
  LiveWindowSnapshot snapshot;
  snapshot.window = window;
  snapshot.eligible = true;
  snapshot.lifecycle_degraded = lifecycle_degraded_;
  if (!model) {
    return snapshot;
  }
  const int count = model->count();
  for (int index = 0; index < count; ++index) {
    tabs::TabInterface* tab = model->GetTabAtIndex(index);
    if (!tab) {
      continue;
    }
    LiveTabDescriptor descriptor;
    descriptor.tab = TabStripBridge::KeyForTab(tab);
    descriptor.strip_order = index;
    descriptor.chromium_pinned = model->IsTabPinned(index);
    descriptor.is_active = model->active_index() == index;
    const std::optional<split_tabs::SplitTabId> split_id =
        model->GetSplitForTab(index);
    if (split_id.has_value()) {
      descriptor.upstream_split_token = split_id->ToString();
    }
    const std::optional<tab_groups::TabGroupId> group_id =
        model->GetTabGroupForTab(index);
    if (group_id.has_value()) {
      descriptor.upstream_group_token = group_id->ToString();
    }
    snapshot.tabs.push_back(descriptor);
  }
  const int active = model->active_index();
  if (active >= 0 && active < count) {
    snapshot.active_tab =
        TabStripBridge::KeyForTab(model->GetTabAtIndex(active));
  }
  return snapshot;
}

void LiveWindowStateProvider::PublishSnapshot(LiveWindowKey window,
                                              TabStripModel* model) {
  LiveWindowSnapshot snapshot = BuildSnapshot(window, model);
  auto it = snapshots_.find(window);
  if (it != snapshots_.end() && it->second.tabs == snapshot.tabs &&
      it->second.active_tab == snapshot.active_tab &&
      it->second.lifecycle_degraded == snapshot.lifecycle_degraded) {
    return;
  }
  snapshot.revision = next_revision_++;
  snapshots_[window] = snapshot;
  NotifySnapshot(snapshot);
}

void LiveWindowStateProvider::RemoveWindow(LiveWindowKey window) {
  snapshots_.erase(window);
  for (LiveWindowStateObserver& observer : observers_) {
    observer.OnLiveWindowRemoved(window);
  }
}

void LiveWindowStateProvider::AddObserver(LiveWindowStateObserver* observer) {
  observers_.AddObserver(observer);
}

void LiveWindowStateProvider::RemoveObserver(
    LiveWindowStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LiveWindowStateProvider::SetSnapshotForTesting(
    LiveWindowKey window,
    LiveWindowSnapshot snapshot) {
  auto it = snapshots_.find(window);
  if (it != snapshots_.end() && it->second.tabs == snapshot.tabs &&
      it->second.active_tab == snapshot.active_tab &&
      it->second.lifecycle_degraded == snapshot.lifecycle_degraded) {
    return;
  }
  snapshot.revision = next_revision_++;
  snapshots_[window] = snapshot;
  NotifySnapshot(snapshot);
}

void LiveWindowStateProvider::NotifySnapshot(
    const LiveWindowSnapshot& snapshot) {
  for (LiveWindowStateObserver& observer : observers_) {
    observer.OnLiveWindowSnapshotChanged(snapshot);
  }
}

}  // namespace seoul
