// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/tab_strip_bridge.h"

#include <utility>
#include <vector>

#include "chrome/browser/tab_list/tab_removed_reason.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "seoul/browser/lifecycle/lifecycle_event_sink.h"

namespace seoul {

namespace {

TabRemovalKind RemovalKindFromReason(TabRemovedReason reason) {
  switch (reason) {
    case TabRemovedReason::kDeleted:
      return TabRemovalKind::kGenuineClose;
    case TabRemovedReason::kInsertedIntoOtherTabStrip:
      return TabRemovalKind::kTransferOut;
    case TabRemovedReason::kInsertedIntoSidePanel:
      return TabRemovalKind::kSidePanel;
    case TabRemovedReason::kDeletedAndExpandSidePanel:
      return TabRemovalKind::kGenuineClose;
  }
  return TabRemovalKind::kUnknown;
}

}  // namespace

TabStripBridge::TabStripBridge(LiveWindowKey window,
                               TabStripModel* model,
                               LifecycleEventSink* sink)
    : window_(window), model_(model), sink_(sink) {
  model_->AddObserver(this);
}

TabStripBridge::~TabStripBridge() {
  // If the strip is already gone, OnTabStripModelDestroyed cleared model_.
  if (model_) {
    model_->RemoveObserver(this);
  }
}

// static
LiveTabKey TabStripBridge::KeyForContents(
    const content::WebContents* contents) {
  if (!contents) {
    return LiveTabKey();
  }
  const SessionID id = sessions::SessionTabHelper::IdForTab(contents);
  return id.is_valid() ? LiveTabKey::FromSessionId(id.id()) : LiveTabKey();
}

// static
LiveTabKey TabStripBridge::KeyForTab(const tabs::TabInterface* tab) {
  return tab ? KeyForContents(tab->GetContents()) : LiveTabKey();
}

void TabStripBridge::EmitTabEvent(NormalizedEventType type,
                                  const LiveTabKey& tab,
                                  int order_index,
                                  int batch_sequence) {
  NormalizedEvent event;
  event.type = type;
  event.window = window_;
  event.tab = tab;
  event.order_index = order_index;
  event.batch_sequence = batch_sequence;
  sink_->OnNormalizedEvent(event);
}

void TabStripBridge::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted: {
      const TabStripModelChange::Insert* insert = change.GetInsert();
      int seq = 0;
      // Process in the documented vector order; rely only on the index carried
      // by each entry, never on a live model query mid-batch.
      for (const auto& entry : insert->contents) {
        EmitTabEvent(NormalizedEventType::kTabInserted,
                     KeyForContents(entry.contents), entry.index, seq++);
      }
      break;
    }
    case TabStripModelChange::kRemoved: {
      const TabStripModelChange::Remove* remove = change.GetRemove();
      int seq = 0;
      for (const auto& entry : remove->contents) {
        // Prefer the SessionID carried by the removal; fall back to the tab.
        LiveTabKey key =
            (entry.session_id.has_value() && entry.session_id->is_valid())
                ? LiveTabKey::FromSessionId(entry.session_id->id())
                : KeyForTab(entry.tab);
        NormalizedEvent event;
        event.type = NormalizedEventType::kTabRemoved;
        event.window = window_;
        event.tab = key;
        event.order_index = entry.index;
        event.batch_sequence = seq++;
        event.removal_kind = RemovalKindFromReason(entry.remove_reason);
        sink_->OnNormalizedEvent(event);
      }
      break;
    }
    case TabStripModelChange::kMoved: {
      const TabStripModelChange::Move* move = change.GetMove();
      EmitTabEvent(NormalizedEventType::kTabMoved, KeyForTab(move->tab),
                   move->to_index, 0);
      break;
    }
    case TabStripModelChange::kReplaced: {
      const TabStripModelChange::Replace* replace = change.GetReplace();
      // Logical tab preserved; report so the coordinator keeps membership and
      // the identity is taken from the tab, not the (changed) WebContents.
      EmitTabEvent(NormalizedEventType::kTabReplaced, KeyForTab(replace->tab),
                   replace->index, 0);
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  // Activation can accompany any change (including discard, where the tab is
  // the same but contents differ). Emit once if the active tab actually
  // changed.
  if (selection.active_tab_changed() && selection.new_tab) {
    EmitTabEvent(NormalizedEventType::kActiveTabChanged,
                 KeyForTab(selection.new_tab), -1, 0);
  }
}

void TabStripBridge::OnTabCloseCancelled(const tabs::TabInterface* tab) {
  EmitTabEvent(NormalizedEventType::kTabCloseCancelled, KeyForTab(tab), -1, 0);
}

void TabStripBridge::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                             int index) {
  NormalizedEvent event;
  event.type = NormalizedEventType::kPinnedStateChanged;
  event.window = window_;
  event.tab = KeyForTab(tab);
  event.order_index = index;
  // Discrete callback (not inside a batch): the index is valid to query.
  event.pinned = model_->IsTabPinned(index);
  sink_->OnNormalizedEvent(event);
}

void TabStripBridge::OnSplitTabChanged(const SplitTabChange& change) {
  NormalizedEvent event;
  event.window = window_;
  event.upstream_split_token = change.split_id.ToString();

  switch (change.type) {
    case SplitTabChange::Type::kAdded: {
      const SplitTabChange::AddedChange* added = change.GetAddedChange();
      event.type = NormalizedEventType::kSplitAdded;
      const auto& tabs = added->tabs();
      if (tabs.size() >= 2) {
        event.split_pane_a = KeyForTab(tabs[0].first);
        event.split_pane_b = KeyForTab(tabs[1].first);
      }
      event.divider_ratio = added->visual_data().split_ratio();
      break;
    }
    case SplitTabChange::Type::kContentsChanged: {
      const SplitTabChange::ContentsChange* contents =
          change.GetContentsChange();
      event.type = NormalizedEventType::kSplitContentsChanged;
      const auto& tabs = contents->new_tabs();
      if (tabs.size() >= 2) {
        event.split_pane_a = KeyForTab(tabs[0].first);
        event.split_pane_b = KeyForTab(tabs[1].first);
      }
      break;
    }
    case SplitTabChange::Type::kVisualsChanged: {
      const SplitTabChange::VisualsChange* visuals = change.GetVisualsChange();
      event.type = NormalizedEventType::kSplitVisualsChanged;
      event.divider_ratio = visuals->new_visual_data().split_ratio();
      // RESEARCH REQUIRED: M149's SplitTabChange exposes no per-event "drag in
      // progress" flag, only kLayoutUpdated / kRatioUpdated. We therefore mark
      // every visuals change as committable; per-pixel writes are still avoided
      // because the PersistenceScheduler coalesces a burst into one write.
      event.split_visuals_intermediate = false;
      break;
    }
    case SplitTabChange::Type::kRemoved:
      event.type = NormalizedEventType::kSplitRemoved;
      break;
  }
  sink_->OnNormalizedEvent(event);
}

void TabStripBridge::OnTabStripModelDestroyed(TabStripModel* tab_strip_model) {
  // Stop observing before the strip becomes invalid; do not touch it afterward.
  if (model_) {
    model_->RemoveObserver(this);
    model_ = nullptr;
  }
  NormalizedEvent event;
  event.type = NormalizedEventType::kTabStripDestroyed;
  event.window = window_;
  sink_->OnNormalizedEvent(event);
}

}  // namespace seoul
