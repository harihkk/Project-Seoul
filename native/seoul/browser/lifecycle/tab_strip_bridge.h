// Project Seoul native lifecycle bridge.
// One thin adapter per eligible browser window. It observes ONLY that window's
// TabStripModel and translates verified Chromium callbacks into
// NormalizedEvents. It holds no organization logic and never mutates the
// browser. It stops observing before the tab strip becomes invalid.
//
// All Chromium APIs used here were confirmed in the pinned M149 source:
//   TabStripModelObserver::OnTabStripModelChanged / OnTabCloseCancelled /
//   OnTabPinnedStateChanged / OnSplitTabChanged / OnTabStripModelDestroyed,
//   TabStripModelChange::{Insert,Remove,Move,Replace} + RemovedTab(session_id),
//   TabRemovedReason, SplitTabChange + SplitTabVisualData::split_ratio(),
//   SplitTabId::ToString(), sessions::SessionTabHelper::IdForTab(),
//   SessionID::id().

#ifndef SEOUL_BROWSER_LIFECYCLE_TAB_STRIP_BRIDGE_H_
#define SEOUL_BROWSER_LIFECYCLE_TAB_STRIP_BRIDGE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/lifecycle/lifecycle_identity.h"

class TabStripModel;
namespace content {
class WebContents;
}
namespace tabs {
class TabInterface;
}

namespace seoul {

class LifecycleEventSink;

class TabStripBridge : public TabStripModelObserver {
 public:
  // `model` and `sink` must outlive this bridge (the window watcher guarantees
  // teardown order). The bridge adds itself as an observer of `model`.
  TabStripBridge(LiveWindowKey window,
                 TabStripModel* model,
                 LifecycleEventSink* sink);
  TabStripBridge(const TabStripBridge&) = delete;
  TabStripBridge& operator=(const TabStripBridge&) = delete;
  ~TabStripBridge() override;

  LiveWindowKey window() const { return window_; }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabCloseCancelled(const tabs::TabInterface* tab) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;
  void OnTabStripModelDestroyed(TabStripModel* tab_strip_model) override;

 private:
  void EmitTabEvent(NormalizedEventType type,
                    const LiveTabKey& tab,
                    int order_index,
                    int batch_sequence);
  static LiveTabKey KeyForContents(const content::WebContents* contents);
  static LiveTabKey KeyForTab(const tabs::TabInterface* tab);

  const LiveWindowKey window_;
  raw_ptr<TabStripModel> model_;
  raw_ptr<LifecycleEventSink> sink_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_TAB_STRIP_BRIDGE_H_
