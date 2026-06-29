# Chromium M149 lifecycle source audit

Milestone Part 2. The exact pinned Chromium APIs the inbound lifecycle bridge
depends on. Pinned checkout: `/Users/hk/Documents/Projects/seoul-chromium/src`,
HEAD `6a7b3dbec3b2ca25877c2553b5473b2f277ef644` (macOS arm64). Every API below was
read directly in that checkout. Anything not confirmed there is marked
`RESEARCH REQUIRED`. No signatures are invented. Plain ASCII.

## Window lifecycle

### BrowserWindowInterface
- Path: `chrome/browser/ui/browser_window/public/browser_window_interface.h`.
- `virtual Profile* GetProfile()`, `virtual const SessionID& GetSessionID() const`,
  `enum Type { TYPE_NORMAL, TYPE_POPUP, TYPE_APP, TYPE_DEVTOOLS, TYPE_APP_POPUP,
  TYPE_PICTURE_IN_PICTURE }`, `virtual Type GetType() const`,
  `virtual TabStripModel* GetTabStripModel()`, `virtual bool IsDeleteScheduled()
  const`, `RegisterBrowserDidClose(BrowserDidCloseCallback)`.
- Ownership: the browser window owns its TabStripModel; all tabs share the
  window's Profile. Lifetime: valid between create and close notifications.
- Why Seoul needs it: window key (`GetSessionID().id()`), eligibility
  (`GetType() == TYPE_NORMAL`, `!IsDeleteScheduled()`), the tab strip to observe.
- Profile restriction: Seoul attaches only to regular profiles (enforced by the
  service factory and by `ProfileBrowserCollection`).

### BrowserCollection and observer
- Paths: `chrome/browser/ui/browser_window/public/browser_collection.h`,
  `.../browser_collection_observer.h`.
- `BrowserCollectionObserver : base::CheckedObserver` with
  `OnBrowserCreated(BrowserWindowInterface*)`,
  `OnBrowserClosed(BrowserWindowInterface*)`, `OnBrowserActivated(...)`,
  `OnBrowserDeactivated(...)`.
- `BrowserCollection::ForEach(base::FunctionRef<bool(BrowserWindowInterface*)>,
  Order, bool enumerate_new_browsers)` enumerates current browsers;
  `FindBrowserWithID(SessionID)`. `AddObserver`/`RemoveObserver` are protected and
  meant to be used through `base::ScopedObservation<BrowserCollection,
  BrowserCollectionObserver>` (the class friends `ScopedObservationTraits`).
- Confirmed real-world use of the ScopedObservation pattern:
  `chrome/browser/glic/common/glic_tab_observer_impl.h`.

### ProfileBrowserCollection
- Path: `chrome/browser/ui/browser_window/public/profile_browser_collection.h`.
- `class ProfileBrowserCollection : public BrowserCollection`,
  `static ProfileBrowserCollection* GetForProfile(Profile*)`. Notifies create,
  close, activate, and deactivate for a single profile.
- Why Seoul needs it: a PER-PROFILE collection means Seoul's per-profile service
  observes only its own windows, with no global cross-profile observer and no
  cross-profile filtering.

### BrowserList
- `chrome/browser/ui/browser_list.h` does NOT exist at this revision (confirmed by
  file lookup and by `git grep -l "class BrowserList "` returning nothing). The
  collection classes above replace it. Code that assumes a global `BrowserList`
  observer must be rewritten against `BrowserCollection`.

### SessionID
- Path: `components/sessions/core/session_id.h`. `id_type` is `int32_t`;
  `id_type id() const`; `bool is_valid() const` is true only for values > 0.
  Used as the source of both `LiveWindowKey` and `LiveTabKey` values.

## Tab strip lifecycle

### TabStripModelObserver
- Path: `chrome/browser/ui/tabs/tab_strip_model_observer.h`.
- `virtual void OnTabStripModelChanged(TabStripModel*, const TabStripModelChange&,
  const TabStripSelectionChange&)`.
- `virtual void OnTabWillBeRemoved(tabs::TabInterface*, int)` (NOT used by Seoul:
  it lacks the removal reason and SessionID that the `kRemoved` change carries).
- `virtual void OnTabCloseCancelled(const tabs::TabInterface*)`.
- `virtual void OnTabPinnedStateChanged(tabs::TabInterface*, int index)`.
- `virtual void OnSplitTabChanged(const SplitTabChange&)`.
- `virtual void OnTabStripModelDestroyed(TabStripModel*)` (the signal to stop
  observing before the strip is invalid).

### TabStripModelChange
- `enum Type { kSelectionOnly, kInserted, kRemoved, kMoved, kReplaced }`;
  accessors `GetInsert/GetRemove/GetMove/GetReplace`.
- `Insert.contents` is `std::vector<ContentsWithIndex>` with
  `{tab, contents, index}`.
- `Remove.contents` is `std::vector<RemovedTab>` with `{tab, contents, index,
  TabRemovedReason remove_reason, tabs::TabInterface::DetachReason
  tab_detach_reason, std::optional<SessionID> session_id}`. The optional SessionID
  is the most reliable identity at removal and is what Seoul reads.
- `Move{tab, contents, from_index, to_index}`.
- `Replace{tab, old_contents, new_contents, index}`.
- Event ordering: the header documents that observers must process `contents` in
  vector order and must NOT do index-based queries from their own stored indices
  until a batch is fully processed. Seoul only uses the index carried in each
  entry, never a live model query mid-batch.

### TabRemovedReason
- Path: `chrome/browser/tab_list/tab_removed_reason.h`.
- `enum class TabRemovedReason { kDeleted, kInsertedIntoOtherTabStrip,
  kInsertedIntoSidePanel, kDeletedAndExpandSidePanel }`. Seoul maps `kDeleted` and
  `kDeletedAndExpandSidePanel` to a genuine close, `kInsertedIntoOtherTabStrip` to
  a transfer-out, `kInsertedIntoSidePanel` to side-panel (membership preserved).

### TabStripSelectionChange
- Same header. `bool active_tab_changed() const { return old_contents !=
  new_contents; }`, plus `old_tab/new_tab`, `old_contents/new_contents`, `reason`.
  Seoul emits one activation event when `active_tab_changed()` and `new_tab` is
  set. Note: discard keeps the same tab with different contents, which this method
  intentionally treats as an active change.

## Tab identity and per-tab ownership

### tabs::TabInterface
- Path: `components/tabs/public/tab_interface.h`. `content::WebContents*
  GetContents() const`; `BrowserWindowInterface* GetBrowserWindowInterface()`;
  `enum class DetachReason`. The TabInterface pointer is runtime-only and is never
  persisted.

### sessions::SessionTabHelper::IdForTab
- Path: `components/sessions/content/session_tab_helper.h`.
  `static SessionID IdForTab(const content::WebContents*)`. Seoul derives a
  `LiveTabKey`/`PersistedTabRef` from this SessionID's value; it never uses a
  WebContents pointer as identity.

### Per-tab ownership decision
- For window-scoped tab events Seoul uses a per-window `TabStripModelObserver`
  (the bridge). `TabFeatures` / per-tab controllers are not needed for inbound
  organization events in v0; they would be appropriate for per-tab UI later.

## Window tracking helpers: why NOT BrowserTabStripTracker

- Path: `chrome/browser/ui/browser_tab_strip_tracker.h` (+ `_delegate.h`).
  `class BrowserTabStripTracker : public BrowserCollectionObserver` with `Init()`,
  `ShouldTrackBrowser(BrowserWindowInterface*)`, and a delegate
  `BrowserTabStripTrackerDelegate::ShouldTrackBrowser(BrowserWindowInterface*)`.
- It exists and is usable, but it adds the SAME single TabStripModelObserver to
  every tracked window's TabStripModel. That conflicts with Seoul's requirement of
  one bridge per window (each observing only its own strip) and with the rule
  against one global observer across all windows. REJECTED for that reason.
- Seoul instead observes `ProfileBrowserCollection` and creates a separate
  `TabStripBridge` per eligible window.

## Session and restore

- Path: `chrome/browser/sessions/session_restore_observer.h`.
  `class SessionRestoreObserver` with `OnSessionRestoreStartedLoadingTabs()`,
  `OnSessionRestoreFinishedLoadingTabs()` (the reliable restore-complete signal:
  called once all restored tabs finish loading, across concurrent restores), and
  `OnGotSession(Profile*, bool for_app, const std::vector<const
  sessions::SessionWindow*>&)`.
- Registration: `chrome/browser/sessions/session_restore.cc`
  `SessionRestore::AddObserver(SessionRestoreObserver*)`.
- Why Seoul needs it: a real completion point lets reconciliation be staged
  (begin -> discover/insert -> complete) WITHOUT a timer-based guess. Seoul models
  this as the normalized `kReconciliationBegan` / `kReconciliationCompleted`
  events; the exact wiring of these observer callbacks to the service is finished
  at the build host.

## Split view

- All in `chrome/browser/ui/tabs/tab_strip_model_observer.h` unless noted.
- `struct SplitTabChange { enum class Type { kAdded, kVisualsChanged,
  kContentsChanged, kRemoved }; split_tabs::SplitTabId split_id; TabStripModel*
  model; Type type; }` with `GetAddedChange/GetVisualsChange/GetContentsChange/
  GetRemovedChange`.
- `AddedChange::tabs()` -> `std::vector<std::pair<tabs::TabInterface*, int>>`,
  `AddedChange::visual_data()` -> `SplitTabVisualData`, `AddedChange::reason()`.
- `VisualsChange::new_visual_data()` / `old_visual_data()`, and
  `reason()` -> `enum class SplitVisualChangeReason { kLayoutUpdated,
  kRatioUpdated }`.
- `ContentsChange::new_tabs()` / `prev_tabs()`. `RemovedChange::tabs()`.
- `SplitTabVisualData` (`components/split_tabs/split_tab_visual_data.h`):
  `double split_ratio() const`, `SplitTabLayout split_layout() const`.
- `SplitTabId` (`components/split_tabs/split_tab_id.h`) is
  `tab_groups::TokenId<SplitTabId>` (`components/tab_groups/token_id.h`) and has
  `std::string ToString() const`. Seoul uses this string as the opaque
  `upstream_split_token`.
- The split id (`SplitTabId`, upstream) is kept distinct from Seoul's
  `SplitGroupId` (a UUID in the pure model).

## RESEARCH REQUIRED

1. Exact ordering of profile-keyed-service creation relative to the first window
   for the profile and to session-restore start. `StartObserving` handles both
   orders (it enumerates current windows once and observes future ones), but the
   reconciliation handshake to `SessionRestoreObserver` is wired at the build host.
2. Whether a tab's SessionID (`IdForTab`) is preserved across WebContents
   discard/replacement and across a full session restore at every point Seoul
   reads it. Where it is not preserved, the coordinator leaves the prior reference
   unresolved rather than fabricating identity.
3. `SplitTabChange` exposes NO per-event "drag in progress" flag (only
   `kLayoutUpdated` / `kRatioUpdated`), so an intermediate divider drag cannot be
   distinguished from a final commit by the event alone. Seoul avoids per-pixel
   writes through persistence coalescing rather than relying on such a flag.
4. Cycle-free GN placement of the Chromium-facing adapters relative to
   `//chrome/browser(/ui)`. See `BUILD.gn` and the integration patch.
