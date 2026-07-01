# Native shell production completion audit

Milestone: NATIVE SEOUL SHELL V0 PRODUCTION COMPLETION. This audit reads the exact
pushed source at main `0fe0e07` (not prior reports) and records, for each known
defect: the exact existing code, why it is incorrect, the repair, the source-level
verification available, and the remaining compile/runtime uncertainty.

Status legend:
- REPAIRED: fixed in this pass at source level (not compiled; see uncertainty).
- CONFIRMED, DEFERRED: defect confirmed in the real source; exact repair specified;
  implementation requires a capable build host (native Views dialogs, action
  registration, real browser-test bodies) and was not faked here.

This 8 GiB machine cannot build Chromium. No production placeholder claim is made
for deferred items; they remain open and are listed honestly.

## 1. New-tab command submitted an invalid empty GURL - REPAIRED

- Existing: `shell_controller.cc` `OpenNewTemporaryTab()` set
  `command.kind = kOpenTemporaryTab; command.url = GURL();` and submitted it.
  `command_executor.cc ValidateCommand` runs `UrlPolicy::ValidateNavigationUrl`
  for `kOpenTemporaryTab`, which rejects an empty/invalid GURL
  (`kInvalidUrl`). The button could never open a tab.
- Why wrong: "normal new tab" was modeled as an invalid URL routed through URL
  navigation validation.
- Repair: added a dedicated `CommandKind::kOpenNewTab`. The mutation adapter gained
  `OpenNewTab(...)` which calls `chrome::AddAndReturnTabAt(browser, GURL(), -1,
  foreground)`; the pinned header documents "If |url| is an empty URL, then the
  new tab-page is loaded", so this is the dedicated NTP path, not URL navigation.
  `kOpenNewTab` is excluded from URL validation, dispatched in
  `DispatchChromiumCommand`, observed as `kTabInserted`, and passes
  `VerifyPostcondition`. The coordinator assigns the inserted tab to the active
  workspace as temporary (existing `kNewTabRole`). The shell submits `kOpenNewTab`
  with no URL. URL policy stays strict for explicit navigation.
- Verification: `chrome::AddAndReturnTabAt` confirmed at
  `chrome/browser/ui/browser_tabstrip.h:36`; clang-format clean; include resolves.
- Uncertainty: not compiled; insertion observation -> membership -> temporary role
  -> focus is exercised by the deferred browser test.

## 2. Reconciliation fabricated begin/completed without the real rescan - REPAIRED

- Existing: `shell_controller.cc RunReconciliation()` directly emitted
  `kReconciliationBegan` and `kReconciliationCompleted` via
  `lifecycle_->OnNormalizedEvent(...)`. It never ran a window rescan.
- Why wrong: synthesized completion; no real `WindowWatcher::RescanExistingWindows`
  call. The real path already existed:
  `SeoulOrganizationService::RunLifecycleReconciliation()` does began ->
  `window_watcher_->RescanExistingWindows()` -> completed, and is wired as the
  coordinator's reconciliation-request callback.
- Repair: added `LifecycleCoordinator::RequestReconciliation()` which runs that
  request callback (the real path). `ShellController::RunReconciliation()` now
  calls `lifecycle_->RequestReconciliation()` and recomputes; it no longer
  synthesizes events. Completion is emitted only after the synchronous bounded
  rescan returns; degraded state clears on the resulting recompute.
- Verification: `RescanExistingWindows` confirmed used at
  `lifecycle/session_restore_watcher.cc:49`; the service path at
  `seoul_organization_service.cc:221`; clang-format clean.
- Uncertainty: not compiled; the rescan's runtime effect needs the browser test.

## 3. Essential state ignores live-window and projection state - CONFIRMED, DEFERRED

- Existing: `shell_view_model.cc` builds `ShellEssentialItem` from the model only;
  `has_live_tab`, `live_in_current_window`, `is_active` are not derived from the
  live window snapshot or projection.
- Why wrong: Essentials cannot show live/active state; clicking cannot activate an
  existing tab (see 4).
- Repair (specified): add a bounded `EssentialResolver` consuming the live window
  snapshot + projection to compute the explicit Essential states (none / live in
  this window / active here / live elsewhere / active elsewhere / opening /
  loading / stale / unresolved / invalid / recovery) via organization references,
  never pointers/indices/WebContents.
- Verification available now: none beyond design; requires the resolver plus tests.
- Uncertainty: needs a capable host to validate live association and activation.

## 4. Every Essential click opens a duplicate retained tab - CONFIRMED, DEFERRED

- Existing: `shell_controller.cc OpenEssential()` unconditionally submits
  `kOpenRetainedTab` with `GURL(essential->root_url)`.
- Why wrong: no live-tab resolution, no pending-open suppression -> a duplicate
  tab every click.
- Repair (specified): with the EssentialResolver (3): if live in current window,
  `kActivateTab` (and coherent workspace switch); if live elsewhere, focus the
  owning window/tab or present an explicit choice; if none, one retained-tab open
  with pending-open suppression and post-insertion association; if stale, reconcile
  before any open. Depends on 3.
- Uncertainty: runtime activation/focus behavior needs the browser test.

## 5. Workspace create/rename use hardcoded placeholder names - CONFIRMED, DEFERRED

- Existing: workspace creation paths use a constant name and rename appends a
  suffix; no user input dialog. (Menu/view layer.)
- Why wrong: not real user operations; placeholder data.
- Repair (specified): source-grounded native Views dialog/bubble for validated
  name input, then `ModelCommandFacade`/`CommandExecutor` create/rename.
- Uncertainty: native dialog construction and modality require a capable host.

## 6. Workspace reorder/restore/valid-delete behavior missing - CONFIRMED, DEFERRED

- Existing: the model supports reorder/restore/delete and the command kinds exist
  (`kReorderWorkspace`, `kRestoreWorkspace`, `kDeleteWorkspace`), but the shell/menu
  exposes no real callbacks for them, including default-delete rejection and an
  archived section.
- Repair (specified): wire menu items to facade commands by stable `WorkspaceId`;
  archived section; deterministic fallback; default-workspace delete rejection;
  failure display.
- Uncertainty: menu/runner behavior requires a capable host.

## 7. Split UI selects the first other tab arbitrarily - CONFIRMED, DEFERRED

- Existing: `shell_controller.cc CreateSplitFromActive()` loops projected tabs and
  picks the first `tab != active` as pane B.
- Why wrong: arbitrary partner; not a user choice.
- Repair (specified): a bounded native split chooser listing eligible same-profile,
  same-window, same-workspace tabs (excluding pane A, existing split members,
  unresolved/unsupported), user selects pane B, revalidate, then the existing
  correct one-index-plus-active-tab `AddToNewSplit` flow. The adapter contract is
  already correct and must not change.
- Uncertainty: chooser UI requires a capable host; the adapter dispatch is verified.

## 8. Command launcher is a tiny static menu - CONFIRMED, DEFERRED

- Existing: `command_launcher_catalog.cc` (96 lines) builds a handful of entries;
  no search input, ranking, or full dispatch surface.
- Repair (specified): a native searchable launcher with entries carrying id,
  label, tokens, category, enablement, disabled reason, dispatch type, target, and
  async policy; real dispatch for the full command set via WorkspaceSwitcher /
  ModelCommandFacade / CommandExecutor / existing Chromium actions; deterministic
  ranking, keyboard selection, pending/failure, focus restoration.
- Uncertainty: launcher Views + dispatch breadth require a capable host.

## 9. Shell action constants are not registered actions/accelerators - CONFIRMED, DEFERRED

- Existing: `shell_actions.h` is `struct ShellActions { static constexpr int
  kNextWorkspace = 1; ... }` - bare integers.
- Why wrong: not `actions::ActionItem`/`ActionManager` registrations, no callbacks,
  no accelerators.
- Repair (specified): per-window `actions::ActionItem`s with collision-free IDs,
  callbacks, enabled state, accessible names, teardown, optional accelerators after
  a conflict audit; expose via the launcher when no safe default exists.
- Uncertainty: action/accelerator wiring requires a capable host and a conflict
  audit against Chromium/DevTools/macOS.

## 10. Browser-test bodies are empty - CONFIRMED, DEFERRED

- Existing: `shell_browsertest.cc`, `chromium_mutation_adapter_browsertest.cc`,
  `vertical_presentation_browsertest.cc` each declare `IN_PROC_BROWSER_TEST_F`
  with empty `{}` bodies. `shell_browsertest.cc` confirmed: 3 empty tests.
- Why wrong: empty bodies are not authored tests.
- Repair (specified): real setup/construction/action/assertions per Phase 15.
- Uncertainty: `InProcessBrowserTest` bodies can only be authored meaningfully
  against a build; they must be written for the runnable target (16).

## 11. Browser-test source sets not connected to a runnable executable - CONFIRMED, DEFERRED

- Existing: browser-test files live in `source_set`s, which cannot host
  `InProcessBrowserTest` as a runnable target.
- Repair (specified): add the Seoul browser-test sources to the existing
  `//chrome/test:browser_tests` target via the single integration patch, or a
  source-grounded runnable target; document exact future commands.
- Uncertainty: requires build-host GN to validate.

## 12. Projection/shell registration before RootTabCollectionNode::Init() - CONFIRMED, DEFERRED

- Existing: the patch registers projection/shell inside
  `VerticalTabStripRegionView::InitializeTabStrip()`. The required order
  (construct root node, drag handler, controller; `SetController`; `Init()`;
  confirm hierarchy; then register) is not guaranteed relative to `root_node_->
  Init()`.
- Repair (specified): move registration to after `root_node_->Init()` and the real
  hierarchy exists; apply initial projection after registration even when the live
  snapshot predates it; unregister in `ResetTabStrip()` before subscriptions reset,
  controller destruction, root-node reset, child teardown.
- Uncertainty: exact ordering needs the real `InitializeTabStrip`/`ResetTabStrip`
  bodies and a build host.

## 13. Production projection uses root_node_for_testing() - CONFIRMED, DEFERRED

- Existing: `projection/vertical_presentation_adapter.cc` calls
  `region->root_node_for_testing()` in production
  (`ApplyToVerticalTabStripRegion`, `FindDefaultFocusableChild`). The patch adds
  `GetSeoulShell*Anchor` / `GetSeoulTabStripView` but no root-node accessor.
- Why wrong: testing-only accessor in production.
- Repair (specified): add a narrow production accessor (e.g.
  `RootTabCollectionNode* GetSeoulRootNode() { return root_node_.get(); }`) to the
  patched `vertical_tab_strip_region_view.h`; switch the adapter to it; keep
  `root_node_for_testing()` for tests only. This is a patch update (no patch 0002).
- Uncertainty: must verify `root_node_` member type on the build host before the
  patch change is finalized.

## 14. Snapshot equality compared revision and ignored content - REPAIRED

- Existing: `ShellController::SnapshotsEqual` returned
  `a.revision == b.revision && ...` while `Recompute` did `revision_++` every call,
  so equality was always false and it rebuilt on every notification; it also
  ignored essentials, pinned items, sections, and actions.
- Repair: defaulted `operator==` added to `ShellEssentialItem`, `ShellPinnedItem`,
  `ShellWorkspaceHeader`, `ShellSectionInfo`, `ShellActionEnablement`.
  `SnapshotsEqual` now compares window, mode, status, workspace, essentials,
  pinned_items, sections, actions, empty/banner/message, switch_phase - but NOT
  revision. `Recompute` builds with the current revision, returns early when
  semantically equal (after first init), and bumps revision only on real change.
- Verification: clang-format clean; pure logic; covered by a unit test to add.
- Uncertainty: not compiled; defaulted `operator==` relies on member `operator==`
  (ids and `LiveTabKey` provide them).

## 15. Workspace-switch completion/failure not directly observed - CONFIRMED, DEFERRED

- Existing: `ShellController::SwitchWorkspace` returns `target` even when
  `phase == kAwaitingActivation` (pending reported as applied); switch progress is
  inferred only during unrelated `Recompute`.
- Repair (specified): a narrow switch-transaction observer from `WorkspaceSwitcher`
  / `ProjectionService` (validating/calculating/awaiting/committing/applied/
  rejected/cancelled/outcome-unknown/idle); `ShellController` observes it directly;
  pending appears immediately; failure/cancel clears the indicator; the menu must
  not report an async switch as synchronously complete.
- Uncertainty: runtime switch sequencing needs the browser test.

## 16. Retained/Temporary labels do not identify real tab roles - CONFIRMED, DEFERRED

- Existing: free-floating section labels in the shell views do not map to actual
  tab rows.
- Repair (specified): least-invasive role indication on the Chromium-owned tab row
  (badge/tooltip/context-menu) without duplicating rows or altering
  `TabCollectionNode` indexing; every projected tab understandable as
  pinned/retained/temporary.
- Uncertainty: tab-row decoration requires a capable host.

## 17. Collapsed mode changes only limited text - CONFIRMED, DEFERRED

- Existing: collapse handling changes limited text, not a coherent collapsed shell.
- Repair (specified): icon-only workspace control, bounded Essential icons,
  Chromium favicons, utility icons with accessible names, degraded indicator,
  keyboard operation, participation in the existing expand-on-hover animation, no
  second width animation, no rebuild per frame.
- Uncertainty: collapsed Views behavior requires a capable host.

## Summary

Repaired this pass at source level: 1, 2, 14. Confirmed and deferred with exact
repair specifications: 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17. The
deferred items are dominated by native Views (dialogs, launcher, split chooser,
role decoration, collapsed shell), action/accelerator registration, and real
`InProcessBrowserTest` bodies, which cannot be authored and verified honestly
without a capable build host. No empty test body or production placeholder has been
claimed removed beyond the three repaired defects.
