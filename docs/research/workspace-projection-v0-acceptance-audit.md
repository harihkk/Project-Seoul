# Workspace Projection V0 acceptance audit

Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.

## Verdict: **ACCEPTED** (with documented M149 split constraint)

---

## 1. Split creation

| Check | Result |
| --- | --- |
| Pane B activated before dispatch | **Pass** - `ActivateTabAt(index_b)` in `chromium_mutation_adapter_impl.cc` |
| One explicit pane A index to `AddToNewSplit` | **Pass** - `std::vector<int> indices = {index_a}` |
| Active pane differs from supplied index | **Pass** - verified `strip->active_index() != index_a` before dispatch |
| Does not use `RestoreSplit` for normal creation | **Pass** - grep shows no `RestoreSplit` in command adapter |
| Both identities revalidated before dispatch | **Pass** - key match + index re-resolve |
| Creation source honest | **Pass** - `SplitTabCreatedSource::kKeyboardShortcut` (no toolbar claim) |
| No duplicate metrics | **Pass** - no manual `RecordSplitTabCreated` |

**M149 constraint (documented):** runtime requires vector size == 1 plus active tab; Seoul satisfies by explicit activation of pane B.

---

## 2. Accessibility API

| Check | Result |
| --- | --- |
| `SetAccessibleIgnored` on `views::View` | **Fail at pinned revision** - method does not exist on `views::View` |
| Repair applied | **Pass** - uses `view->GetViewAccessibility().SetIsIgnored(!visible)` per M149 `ui/views/accessibility/view_accessibility.h:339` |

---

## 3. Event processing

| Check | Result |
| --- | --- |
| Hidden views cannot process events | **Pass** - `SetCanProcessEventsWithinSubtree(visible)` |
| Descendants blocked | **Pass** - recursive application on collection tree |
| Re-show restores processing | **Pass** - same path with `visible=true` |

---

## 4. Focus

| Check | Result |
| --- | --- |
| Active tab never hidden by committed projection | **Pass** - `WindowProjectionController::Recompute` enters fail-open if active tab not projected |
| Focus transferred before hide | **Pass** - `ClearFocus` / `RequestFocus` in adapter |
| Fail-open restores focus target | **Pass** - `FindDefaultFocusableChild` + patch hook |
| No focus on destroyed views | **Pass** - uses live `GetFocusedView()` per apply |

---

## 5. Production wiring

```text
TabStripBridge::PublishLiveSnapshot
  → LiveWindowStateProvider (WindowWatcher-owned)
  → ProjectionService::OnLiveWindowSnapshotChanged
  → WindowProjectionController::UpdateLiveState
  → ProjectionService::OnProjectionChanged
  → VerticalPresentationAdapter::ApplyToVerticalTabStripRegion
  → RootTabCollectionNode views
```

Constructed in `SeoulOrganizationService` constructor; region registered from patch `InitializeTabStrip`.

---

## 6. Stub scan

No empty production bodies, unconsumed projection observers, or `RestoreSplit` normal path found after repair.

---

## Repairs in this milestone

1. Replaced invented `SetAccessibleIgnored` with pinned `GetViewAccessibility().SetIsIgnored`.
