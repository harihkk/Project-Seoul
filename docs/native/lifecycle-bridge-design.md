# Lifecycle bridge design (inbound v0)

Milestone Part 3. The smallest safe architecture that connects real Chromium
window/tab/split events to the Organization Engine, confirmed against the pinned
M149 source (see `docs/research/chromium-lifecycle-source-audit.md`). Inbound
only: nothing here asks Chromium to change tabs. Plain ASCII.

## Data flow

```
Chromium browser/window/tab/split events
        |
        v
  TabStripBridge (one per eligible normal window)   WindowWatcher (per profile)
        |  normalized events                                |  window events
        +------------------------+-------------------------+
                                 v
                       LifecycleCoordinator  (pure logic)
                                 |
                                 v
                        OrganizationModel (pure)
                                 |
                                 v
                       PersistenceScheduler (coalesced)
                                 |
                                 v
                  PrefService dict "seoul.organization.v1"
```

## Object ownership

```
SeoulOrganizationService  (KeyedService, one per regular profile)
  owns OrganizationModel                model_
  owns PersistenceScheduler             scheduler_   (writer = WriteToPrefs)
  owns LifecycleCoordinator             coordinator_ (schedule = scheduler_.ScheduleWrite)
  owns WindowWatcher                    window_watcher_
        observes ProfileBrowserCollection::GetForProfile(profile)
        owns map<LiveWindowKey, unique_ptr<TabStripBridge>> bridges_
              each TabStripBridge observes exactly one window's TabStripModel
              and emits NormalizedEvents into the coordinator (LifecycleEventSink)
```

No raw pointer outlives the Chromium object it refers to. The bridge holds a
`raw_ptr<TabStripModel>`; the watcher holds `raw_ptr<Profile>` and
`raw_ptr<LifecycleCoordinator>`; the coordinator holds `raw_ptr<OrganizationModel>`.
Nothing in the persisted model holds a pointer.

## Construction order

1. `model_` is constructed and `LoadFromPrefs()` runs (atomic load; default
   workspace ensured). Persistence during load is suppressed (`loading_`).
2. `scheduler_` is constructed with the bool writer `WriteToPrefs` and the current
   sequenced task runner.
3. `coordinator_` is constructed with `&model_` and a schedule closure bound to
   `scheduler_->ScheduleWrite`.
4. `window_watcher_` is constructed with the profile and `coordinator_.get()`,
   then `StartObserving()` enumerates already-open eligible windows once and
   begins observing the collection.

## Destruction order (reverse)

Members are declared `scheduler_`, `coordinator_`, `window_watcher_`, so default
destruction runs `window_watcher_` first, then `coordinator_`, then `scheduler_`.
`KeyedService::Shutdown()` makes this explicit and early (while Chromium
dependencies are still valid):

1. `window_watcher_.reset()` detaches the collection observation and destroys all
   per-window bridges; each bridge removes itself from its TabStripModel.
2. The coordinator is told `kShutdownBegan` (it clears pending transfers and stops
   acting on later events).
3. `scheduler_->Flush()` performs one final synchronous write; `Shutdown()` makes
   further scheduled writes no-ops.

## Eligibility

- Profile: regular, non-incognito only. Enforced twice: the service factory
  (`ProfileSelections` with `WithRegular(kOriginalOnly)` and guest/system/ash set
  to `kNone`) and `ProfileBrowserCollection`, which is per-profile.
- Window: `GetType() == TYPE_NORMAL` and `!IsDeleteScheduled()`. Popup, app,
  app-popup, devtools, and picture-in-picture windows are not organized in v0.
- Tab: a tab becomes tracked when it is inserted into an observed normal window
  and yields a valid SessionID. Tabs without a valid SessionID are not assigned.

## Observer attachment and detachment

- The watcher observes the collection through
  `base::ScopedObservation<BrowserCollection, BrowserCollectionObserver>`.
- On `OnBrowserCreated` (or during the initial `ForEach`) an eligible window gets
  one `TabStripBridge`, created with the window key, the `TabStripModel*`, and the
  coordinator. The bridge calls `model->AddObserver(this)` in its constructor.
- On `OnBrowserClosed` the watcher emits `kWindowDestroyed` and erases the bridge;
  the bridge destructor calls `RemoveObserver`.
- If the strip is torn down first, `OnTabStripModelDestroyed` clears the bridge's
  `model_` and emits `kTabStripDestroyed`, so the destructor does not touch an
  invalid strip.

## Error handling

- Every model mutation returns a typed `OrganizationError`; the coordinator
  ignores no-op and not-found results and never reports false success.
- Untracked panes, cross-workspace splits, and invalid ids fail safely (the event
  is dropped without mutation).
- Persistence failure is observable on the scheduler and never corrupts the
  in-memory model; it does not spin (retried only on the next genuine request).

## Reentrancy

- The coordinator holds an `applying_` guard: an event arriving while another is
  being applied is dropped. Inbound events do not nest; the guard exists for the
  future outbound layer.
- The pure model holds its own `notifying_` guard that rejects mutations issued
  during observer notification.
- Each event runs under a scoped `current_origin_` (`base::AutoReset`), exposed via
  `current_origin()` so the future outbound layer can tell observation from its
  own commands.

## Persistence scheduling

- The coordinator calls a schedule closure after a committed mutation; the
  scheduler coalesces a burst within one task turn into a single posted write.
- Intermediate split divider events are not committed (the coordinator skips
  `split_visuals_intermediate`); even if M149 fired per pixel, the scheduler would
  still collapse the writes. No synchronous disk I/O is added to event callbacks.

## Startup reconciliation

- Staged, no timer: `kReconciliationBegan` sets a reconciling flag; window
  discovery and restored-tab insertions run; `kReconciliationCompleted` clears it.
- Restored tabs match existing persisted memberships by `tab_key` (the SessionID
  string). A matched tab is not duplicated; an unmatched live tab is assigned to
  the window's active-or-default workspace; a persisted membership whose tab never
  reappears is left as bounded restorable metadata (never fabricated as a live
  tab, never reopened as a URL). Repeated reconciliation is idempotent.
- The reliable completion signal is `SessionRestoreObserver
  ::OnSessionRestoreFinishedLoadingTabs()`; wiring that callback to emit
  `kReconciliationCompleted` is finished at the build host (RESEARCH REQUIRED).

## Shutdown behavior

- Detach observers before teardown, tell the coordinator, flush once. After
  shutdown the coordinator ignores all events except the shutdown event, and the
  scheduler ignores further scheduling but still allows the final flush.
