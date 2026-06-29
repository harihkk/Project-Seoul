# Command layer V0 acceptance audit

Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644` (M149).

## Verdict

**ACCEPTED** after repairs documented below.

## Gate 1 - Exact M149 split API

### Pinned upstream contract

```cpp
split_tabs::SplitTabId AddToNewSplit(
    std::vector<int> indices,
    split_tabs::SplitTabVisualData visual_data,
    split_tabs::SplitTabCreatedSource source);
```

M149 runtime checks (`tab_strip_model.cc:1964-1969`):

- `CHECK_EQ(indices.size(), 1u)` - one explicit index plus the **active** tab.
- `CHECK(std::ranges::is_sorted(indices))`
- `CHECK(active_index() != kNoTab)`
- `CHECK(active_index() != indices[0])`

Seoul V0 rejects relying on unchecked active-tab coupling. The adapter instead uses the sibling API:

```cpp
void RestoreSplit(split_tabs::SplitTabId split_id,
                  const std::vector<int>& indices,  // exactly 2, sorted
                  split_tabs::SplitTabVisualData visual_data);
```

Both panes are validated immediately before dispatch in `ChromiumMutationAdapterImpl::CreateSplit`:

- exactly two distinct sorted indices;
- both indices still resolve to the intended `LiveTabKey` values;
- neither tab is already split;
- same window (resolver) and same Seoul workspace (executor pre-check);
- `SplitTabCreatedSource::kToolbarButton` recorded via `RecordSplitTabCreated`;
- never invokes `AddToNewSplit` with a single unchecked index.

### Defect found

Prior adapter passed `{split.pane_a_index}` only - would `CHECK` unless active tab happened to be pane B.

### Repair

`native/seoul/browser/commands/chromium_mutation_adapter_impl.cc` - two-index `RestoreSplit` path with pre-dispatch validation and workspace check in `command_executor.cc`.

## Gate 2 - Split removal

Adapter now:

1. Resolves token via `ListSplits()`
2. Confirms `TabStripModel::ContainsSplit`
3. Calls `RemoveSplit`
4. Awaits `kSplitRemoved` via observation registry
5. Does not close tabs (Chromium preserves panes)

## Gate 3 - Retained-tab partial completion

If insertion succeeds but `RetainTab` fails:

- Status: `CommandStatus::kAppliedWithOrganizationRepairRequired` (not rejected)
- Live tab remains; reconciliation can promote role without duplicate open

## Gate 4 - Pin-state single authority

### Defect found

Command-initiated pin caused double `PinTab`: lifecycle `HandlePinnedStateChanged` then executor confirmation.

### Repair

- `ExpectedObservationRegistry::HasPendingPinCommandForTab`
- `LifecycleCoordinator::SetPinHandlingSuppressor` wired from service
- Executor owns command-initiated role transition with `saved_root_url`
- Lifecycle handles user-initiated Chromium pin events only
- Idempotent skip when role already matches

Workspace pins use model-only `kSetWorkspacePinned`; Chromium global pin is not used for workspace visibility in projection V0.

## Gate 5 - Late observations after watchdog expiry

- Watchdog expiry marks `kOutcomeUnknown` in `outcome_unknown_commands_`
- Late events reconcile model state via lifecycle; no automatic command retry
- Historical outcome remains unknown unless product policy adds late confirmation

## Gate 6 - Callback lifetime audit

| Site | Mechanism | Proof |
| --- | --- | --- |
| `PersistenceScheduler::ScheduleWrite` | `WeakPtrFactory` | Task no-ops if scheduler destroyed |
| `SeoulOrganizationService::WriteToPrefs` | `WeakPtrFactory` | Replaces prior `Unretained(this)` |
| `SeoulOrganizationService::RunLifecycleReconciliation` | `WeakPtrFactory` | Same |
| `CommandConfirmationSeam` callback | `Unretained` | Cleared in `Shutdown()` before seam destroyed; executor reset first |
| `CommandExecutor::HasPendingPinCommandForTab` suppressor | `Unretained` | Cleared in `Shutdown()` before executor destroyed |
| `TabStripBridge` observer | `RemoveObserver` in destructor | Model nulled on strip destroyed |
| `WindowWatcher` collection observer | `Untrack` on browser close | Bridges erased before coordinator gone |

## Gate 7 - Browser-adapter tests

Authored (not executed):

- `native/seoul/browser/commands/chromium_mutation_adapter_browsertest.cc`
- GN target: `//seoul/browser/commands:command_browser_tests`
