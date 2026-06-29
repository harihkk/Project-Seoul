// Project Seoul native lifecycle bridge.
// Normalized browser events: a Chromium-free representation of only the
// verified facts the coordinator needs. The thin per-window adapter translates
// Chromium callbacks into these; the coordinator consumes only these. No raw
// Chromium pointer, page content, credential, form state, or HTML ever appears
// here.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_EVENTS_H_
#define SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_EVENTS_H_

#include <string>

#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/organization/organization_types.h"

namespace seoul {

enum class NormalizedEventType {
  kWindowDiscovered,
  kWindowClosing,
  kWindowDestroyed,
  kTabInserted,
  kTabRemoved,
  kTabMoved,
  kActiveTabChanged,
  kPinnedStateChanged,
  kTabReplaced,
  kTabCloseCancelled,
  kSplitAdded,
  kSplitRemoved,
  kSplitContentsChanged,
  kSplitVisualsChanged,
  kTabStripDestroyed,
  kReconciliationBegan,
  kReconciliationCompleted,
  kShutdownBegan,
};

// How an inserted tab arrived. Drives whether membership is created vs
// preserved.
enum class TabInsertKind {
  kUnknown,
  kNew,         // a genuinely new tab
  kTransferIn,  // arriving from another window (matches a pending transfer)
  kRestored,    // created by session restore
};

// Why a tab was removed, from TabRemovedReason + TabInterface::DetachReason.
enum class TabRemovalKind {
  kUnknown,
  kGenuineClose,    // TabRemovedReason::kDeleted
  kTransferOut,     // kInsertedIntoOtherTabStrip (detach for transfer)
  kSidePanel,       // kInsertedIntoSidePanel
  kReplaced,        // WebContents replacement (logical tab preserved)
  kWindowShutdown,  // removal as part of window/strip teardown
  kStripDestroyed,
};

// A single normalized event. Presence of an id is signalled by is_valid().
struct NormalizedEvent {
  NormalizedEventType type;
  MutationOrigin origin = MutationOrigin::kChromiumObservation;

  LiveWindowKey window;
  PersistedWindowRef persisted_window;

  LiveTabKey tab;
  PersistedTabRef persisted_tab;

  TabInsertKind insert_kind = TabInsertKind::kUnknown;
  TabRemovalKind removal_kind = TabRemovalKind::kUnknown;
  bool pinned = false;
  int order_index = -1;  // tab index within the window, for ordering only

  // Split payload (only for kSplit* events).
  std::string upstream_split_token;
  LiveTabKey split_pane_a;
  LiveTabKey split_pane_b;
  double divider_ratio = 0.5;
  // True while a divider drag is in progress; such events must not persist.
  bool split_visuals_intermediate = false;

  // Ordering context within a Chromium batch (process in ascending order; do
  // not query stale indices mid-batch).
  int batch_sequence = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_EVENTS_H_
