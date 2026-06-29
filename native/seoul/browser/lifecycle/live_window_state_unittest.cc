// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/live_window_state.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class RecordingLiveObserver : public LiveWindowStateObserver {
 public:
  void OnLiveWindowSnapshotChanged(
      const LiveWindowSnapshot& snapshot) override {
    snapshots_.push_back(snapshot);
  }
  void OnLiveWindowRemoved(LiveWindowKey window) override {
    removed_.push_back(window);
  }
  const std::vector<LiveWindowSnapshot>& snapshots() const {
    return snapshots_;
  }
  const std::vector<LiveWindowKey>& removed() const { return removed_; }

 private:
  std::vector<LiveWindowSnapshot> snapshots_;
  std::vector<LiveWindowKey> removed_;
};

TEST(LiveWindowStateProviderTest, PublishesInitialSnapshot) {
  LiveWindowStateProvider provider;
  RecordingLiveObserver observer;
  provider.AddObserver(&observer);
  LiveWindowSnapshot snapshot;
  snapshot.window = LiveWindowKey::FromSessionId(1);
  snapshot.active_tab = LiveTabKey::FromSessionId(10);
  LiveTabDescriptor tab;
  tab.tab = snapshot.active_tab;
  tab.strip_order = 0;
  snapshot.tabs.push_back(tab);
  provider.SetSnapshotForTesting(snapshot.window, snapshot);
  ASSERT_EQ(observer.snapshots().size(), 1u);
  EXPECT_EQ(observer.snapshots()[0].active_tab, snapshot.active_tab);
}

TEST(LiveWindowStateProviderTest, SkipsDuplicateSnapshots) {
  LiveWindowStateProvider provider;
  RecordingLiveObserver observer;
  provider.AddObserver(&observer);
  LiveWindowSnapshot snapshot;
  snapshot.window = LiveWindowKey::FromSessionId(2);
  provider.SetSnapshotForTesting(snapshot.window, snapshot);
  provider.SetSnapshotForTesting(snapshot.window, snapshot);
  EXPECT_EQ(observer.snapshots().size(), 1u);
}

TEST(LiveWindowStateProviderTest, RemoveWindowNotifiesObserver) {
  LiveWindowStateProvider provider;
  RecordingLiveObserver observer;
  provider.AddObserver(&observer);
  const LiveWindowKey window = LiveWindowKey::FromSessionId(3);
  provider.RemoveWindow(window);
  ASSERT_EQ(observer.removed().size(), 1u);
  EXPECT_EQ(observer.removed()[0], window);
}

}  // namespace
}  // namespace seoul
