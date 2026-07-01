# Native Seoul Shell V0 verification

Machine: 8 GiB RAM - no GN, Ninja, compilation, or test execution.

## VERIFIED NOW

- Projection acceptance audit (`docs/research/workspace-projection-v0-acceptance-audit.md`)
- Accessibility repair: `GetViewAccessibility().SetIsIgnored`
- Shell architecture research (`docs/research/native-shell-integration-audit.md`)
- Product spec (`docs/product/native-seoul-shell-v0-spec.md`)
- Pure shell view model (`shell_view_model.*`)
- Per-window shell controller (`shell_controller.*`)
- Shell service wired in `SeoulOrganizationService`
- Production Views: header, footer, workspace menu, command launcher
- Patch registers `ShellService` in `VerticalTabStripRegionView`
- GN targets: `shell_core`, `shell_chromium`, unit/browser test targets
- Tests authored: `seoul_shell_core_unittests`, `shell_browser_tests`
- Patch manifest SHA256 updated

## NOT VERIFIED UNTIL CAPABLE HOST

- GN generation, compilation, linking
- Unit/browser test execution
- Actual shell appearance and layout
- Focus, accessibility tree, keyboard shortcuts
- Context menus, collapse animation, expand-on-hover
- Real tab actions and workspace switching UX
- Performance and memory

## Recommended next milestone

**Runtime integration verification on a capable host**: compile `shell_chromium`, run `seoul_shell_core_unittests` and browser tests, manually validate workspace switch + Essentials + projection filter together.

## Production completion pass (from main 0fe0e07)

Correction to the section above: the browser-test files
(`shell_browsertest.cc`, `chromium_mutation_adapter_browsertest.cc`,
`vertical_presentation_browsertest.cc`) contain only empty `IN_PROC_BROWSER_TEST_F`
bodies and are NOT authored tests. They remain open (audit item 10).

Full defect inventory and repair specifications:
`docs/research/native-shell-production-completion-audit.md`.

### VERIFIED NOW (this pass, source level)

- Audit of the exact pushed source confirming all 17 listed defects.
- REPAIRED defect 1: dedicated `CommandKind::kOpenNewTab` opens the New Tab Page
  via `chrome::AddAndReturnTabAt(browser, GURL(), -1, foreground)` (empty GURL =
  NTP, documented at `chrome/browser/ui/browser_tabstrip.h:36`); the shell no
  longer submits an empty GURL through URL validation; URL policy stays strict.
- REPAIRED defect 2: `ShellController::RunReconciliation()` delegates to
  `LifecycleCoordinator::RequestReconciliation()`, which runs the real path
  (began -> `WindowWatcher::RescanExistingWindows()` -> completed); no fabricated
  events.
- REPAIRED defect 14: semantic snapshot deduplication - `SnapshotsEqual` compares
  all user-visible/command-relevant fields excluding `revision`; `Recompute` bumps
  `revision` only on real change and skips view rebuild when unchanged; defaulted
  `operator==` added to the nested shell types.
- clang-format clean (all native C++), header include resolution passes (91
  Chromium includes resolved, including the new `browser_tabstrip.h`),
  `git diff --check` clean, patch series applies and reverses cleanly, materialize
  round trip clean, repo boundary clean, patch manifest valid, no AI/vendor names,
  no em/en dashes.
- Chromium checkout restored exactly: HEAD at the lock, 0 tracked edits, no
  `src/seoul` overlay, no patch left applied.

### NOT VERIFIED / STILL OPEN (require a capable build host)

- Defects 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 15, 16, 17 remain open with exact
  repair specs in the audit. They are dominated by native Views (Essential
  resolver UI, workspace input dialogs, split chooser, searchable launcher, tab
  role decoration, collapsed shell), `actions::ActionItem`/accelerator
  registration, the production root-node accessor patch change, the switch
  transaction observer, and real `InProcessBrowserTest` bodies connected to a
  runnable target. None were faked.
- GN generation, C++ compilation, linking, and all test execution remain
  unavailable on this 8 GiB machine. A capable-host compile is now mandatory to
  proceed.

## Production completion pass 2 (from local working tree)

The three prior repairs were revalidated in the current source and remain correct
(`kOpenNewTab` distinct path, `RequestReconciliation` delegation, semantic
`SnapshotsEqual` excluding revision). Additional source-level repairs this pass:

### VERIFIED NOW (source level)

- REPAIRED defect 15/4 (direct switch-transaction observation): added
  `WorkspaceSwitchObserver` and `WorkspaceSwitcher::AddObserver/RemoveObserver`;
  every transition now routes through `SetPhase(...)` which notifies observers,
  including the terminal applied/rejected/cancelled/outcome-unknown phases before
  the transaction resets to idle (previously rejection set no observable phase).
  `ShellController` implements the observer, subscribes in the constructor,
  unsubscribes in `Shutdown`, drives recompute on every transition, uses the
  directly observed phase (not an inferred `switcher->phase()` snapshot), and
  surfaces a sticky user-facing "Workspace switch failed." banner on failure
  terminals. The workspace `switching` flag is now true only for in-progress
  phases, so a rejected switch no longer reads as switching.
- REPAIRED the internal-status-string defect (audit 17 / verification list):
  user-facing `status_message`, action `disabled_reason`, and section labels no
  longer expose internal snake_case identifiers (`organization_recovery_required`,
  `reconciliation_required`, `projection_fail_open`, `workspace_pinned`,
  `retained_tabs`, `temporary_tabs`, `split_precondition_failure`). They are now
  readable strings ("Recovery required.", "Pinned", etc.). Resource (grit)
  backing remains a future step; no internal identifier is shown to the user.
- clang-format clean (all native C++), header include resolution passes, repo
  boundary clean, patch manifest valid, `git diff --check` clean, no AI/vendor
  names, no em/en dashes.

### STILL OPEN (source work remaining; not faked, not deferred for compilation)

Implemented incrementally to keep each change correct and verifiable. Remaining
defects with specs in `docs/research/native-shell-production-completion-audit.md`:
scoped region-host ownership (region-host map); Essential
live-association resolver and duplicate-open prevention (audit 3, 4); native
workspace dialogs and full menu operations (audit 5, 6); explicit split chooser
(audit 7); searchable command launcher with full dispatch (audit 8); per-window
`actions::ActionItem` registration and accelerators (audit 9); tab-role
representation (audit 16); collapsed-mode Views behavior (audit 17);
accessibility roles and announcements; non-empty browser tests connected to a
runnable target (audit 10, 11). Only compilation, linking, and runtime
validation of all of the above require a capable host.

## Production completion pass 3 (from local working tree)

Verified against the exact pinned `vertical_tab_strip_region_view.{cc,h}`.

### VERIFIED NOW (source level)

- REPAIRED defect 12 (registration order): the integration patch previously
  registered projection and shell immediately after `root_node_` construction,
  before `SetController()` and `RootTabCollectionNode::Init()` (lines verified in
  the applied file). Registration is now emitted at the END of
  `InitializeTabStrip()`, after `SetController()`, `Init()`, and the
  child-add/remove/move callback subscriptions, so Seoul never observes a
  partially initialized view tree.
- REPAIRED `ResetTabStrip` teardown: Seoul projection and shell are now
  unregistered at the START of `ResetTabStrip()`, before the subscriptions,
  controller, root node, and child views are reset. Combined with the existing
  destructor unregistration (guarded by `root_node_`), unregistration runs
  exactly once per reset or destruction and is idempotent; reinitialization
  re-registers after the next `Init()`.
- REPAIRED defect 13 (production testing accessor): added a production
  `VerticalTabStripRegionView::GetSeoulRootNode()` accessor to the patched
  header; `VerticalPresentationAdapter` now uses it instead of
  `root_node_for_testing()`. A production-accessor scan confirms no
  `*_for_testing()` use remains in `projection/` or `shell/` production code.
- The single integration patch was regenerated (no patch 0002): it applies and
  reverses cleanly against the pinned checkout, contains no trailing whitespace,
  `git diff --check` is clean, the manifest `sha256`, `affectedPaths`
  (now including the `.h`), and description are updated, clang-format and header
  include resolution pass, repo boundary is clean, and the Chromium checkout is
  restored unchanged (HEAD at the lock, 0 tracked edits, no `src/seoul`).

### STILL OPEN (source work remaining)

Scoped region-host ownership (audit region-host map); Essential live-association
resolver and duplicate-open prevention (audit 3, 4); native workspace dialogs and
full menu operations (audit 5, 6); explicit split chooser (audit 7); searchable
command launcher dispatch (audit 8); per-window `actions::ActionItem` +
accelerators (audit 9); tab-role representation (audit 16); collapsed-mode Views
(audit 17); accessibility roles and announcements; non-empty browser tests +
runnable target (audit 10, 11).
