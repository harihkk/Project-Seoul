# Workspace Projection V0 completion audit

Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.  
Audit method: read production source bodies, wiring paths, and test seams - not comments or test names alone.

## Classification key

| Status | Meaning |
| --- | --- |
| **Complete** | Real source implementation with connected call paths |
| **Partial** | Substantial logic present; gaps remain |
| **Stub** | Empty or no-op body |
| **Disconnected** | Implemented but not constructed/wired |
| **Incorrect API** | Wrong Chromium API or semantics |
| **Compile uncertain** | Cannot link-check on 8 GiB host |
| **Runtime deferred** | Source present; browser execution not run |

---

## Command layer

| Component | Status | Notes |
| --- | --- | --- |
| `ChromiumMutationAdapterImpl::CreateSplit` | **Complete** | Uses `AddToNewSplit` with one explicit index after validating both panes and activating pane B. `RestoreSplit` not used for normal creation. Source: `kKeyboardShortcut` (closest honest M149 enum for internal command; no toolbar claim). No manual `RecordSplitTabCreated`. |
| `ChromiumMutationAdapterImpl::DissolveSplit` | **Complete** | `ContainsSplit` via `ListSplits` + `ContainsSplit` before `RemoveSplit`. |
| `CommandExecutor` completion observers | **Complete** | `NotifyCommandCompleted` on applied/rejected/cancelled/outcome-unknown. |
| `ShouldDeferLifecyclePinRoleMutation` | **Partial** | Defers lifecycle **role** mutation only via pending pin command ID; live pin snapshot still published by `TabStripBridge::PublishLiveSnapshot` after every pin event regardless of suppressor. Bounded to registry in-flight commands. |
| Open/move/close/activate adapters | **Complete** | Index re-resolution at dispatch; no cached raw indices in adapter. |
| `chromium_mutation_adapter_browsertest.cc` | **Runtime deferred** | Skeleton browser tests; not executed. |

### M149 `AddToNewSplit` limitation (documented)

At pinned revision the runtime CHECK requires **vector size == 1** plus active tab ≠ supplied index. Seoul validates both panes, explicitly activates pane B, re-resolves indices, then calls `AddToNewSplit({index_a}, visual, source)`. Both tabs are validated immediately before dispatch; this satisfies M149 while honoring the product requirement to target both identities explicitly.

---

## Lifecycle / live state feed

| Component | Status | Notes |
| --- | --- | --- |
| `LiveWindowSnapshot` / `LiveTabDescriptor` | **Complete** | Ephemeral snapshot types; no WebContents/HTML/credentials. |
| `LiveWindowStateProvider` | **Complete** | `BuildSnapshot`, deduped `PublishSnapshot`, `RemoveWindow`, observer notify. |
| `TabStripBridge::PublishLiveSnapshot` | **Complete** | Called after insert/remove/move/activation/pin/split/rescan. |
| `WindowWatcher` ownership | **Complete** | Owns provider; passes to bridges; `RemoveWindow` on untrack. |
| Session restore / rescan | **Complete** | `RescanExistingState` republishes; reconciliation path rescans windows. |
| Degraded flag propagation | **Partial** | `SetLifecycleDegraded` on provider; projection enters fail-open on degraded snapshots. Coordinator overflow triggers reconciliation callback; projection listens via snapshot flag. |

Architecture:

```text
TabStripBridge / TabStripModel
        |
        v
LiveWindowStateProvider  (WindowWatcher-owned)
        |
        v
ProjectionService  (LiveWindowStateObserver)
        |
        v
WindowProjectionController
        |
        v
VerticalPresentationAdapter  (per window, ProjectionService-owned)
```

---

## Projection core

| Component | Status | Notes |
| --- | --- | --- |
| `ProjectionCalculator` | **Complete** | Workspace filter, split coherence, active-tab inconsistency detection, fail-open path. |
| `ProjectionOrdering` | **Complete** | Move translation, close fallback, switch target selection. |
| `WindowProjectionController` | **Complete** | Recompute on live/org changes; auto fail-open when active tab would be hidden; skips redundant publish. |
| `VerticalPresentationFilter` | **Complete** | Tab/split/group predicates. |
| `VerticalPresentationAdapter::ApplyToVerticalTabStripRegion` | **Complete** | Bottom-up visibility on `RootTabCollectionNode`; split requires all pane tabs visible; group requires any child; containers require visible descendants; `SetVisible` + `SetCanProcessEventsWithinSubtree` + `SetAccessibleIgnored`; focus transfer off hidden views. |
| `VerticalPresentationAdapter::FindDefaultFocusableChild` | **Complete** | Prefers projected active tab; else first visible tab. |
| `ProjectionService` wiring | **Complete** | Observes live state; one controller/switcher/adapter per window; registers vertical region from patch; applies projection on change. |
| `WorkspaceSwitcher` | **Complete** | Transaction phases; awaits activation via `CommandCompletionObserver`; rollback on reject/cancel; outcome-unknown inspects live state; external activation path issues no second activation command; concurrent switch rejected. |
| External hidden-tab activation | **Complete** | `MaybeHandleHiddenActiveTab` + `SwitchWorkspaceForWindowExternalActivation`; lifecycle `HandleActiveTabChanged` also aligns workspace. |
| Fail-open view effect | **Complete** | `EnterFailOpen` → recompute with fail-open projection → adapter shows all nodes. |

---

## Patch / Chromium integration

| Component | Status | Notes |
| --- | --- | --- |
| `0001-seoul-native-core.patch` | **Complete** | Single patch: BUILD deps, factory registration, `RegisterVerticalRegion` / `UnregisterVerticalRegion`, projection-aware `GetDefaultFocusableChild`. |
| Previous stub hook (temporary adapter) | **Fixed** | Removed; uses `ProjectionService` lifetime. |

---

## Tests (authored, not executed)

| Target | Coverage |
| --- | --- |
| `seoul_projection_core_unittests` | Calculator, ordering, filter (incl. split token), switcher (archived, already-active, external activation) |
| `seoul_lifecycle_state_unittests` | Live snapshot publish, dedup, remove observer |
| `seoul_command_core_unittests` | Executor validation, reconciliation gate |
| `projection_browser_tests` / `command_browser_tests` | Future skeletons |

---

## Remaining risks

| Risk | Severity |
| --- | --- |
| C++ compile/link on capable host | **Compile uncertain** |
| Real view visibility/focus/a11y in running browser | **Runtime deferred** |
| Pin suppressor uses pending-command map not per-command-ID consumption audit at runtime | **Runtime deferred** |
| Mixed-workspace group diagnostic marking in views (calculator flags inconsistency; group still shows filtered children) | **Partial** - diagnostics in projection, not separate view badge |

---

## Verdict inputs

All V0 source gates addressed in this audit:

- Normal split creation: `AddToNewSplit`, not `RestoreSplit`
- Live-state feed wired TabStripBridge → Provider → ProjectionService
- Vertical adapter: real bottom-up implementation
- Workspace switching: awaits command completion observer
- Active tab never hidden: controller fail-open + focus seam
- Fail-open restores all tab views via adapter
- No production stub paths remain in projection/lifecycle/command adapters audited above

Runtime/browser verification remains deferred on this host.
