# Seoul Shell Core Interactions V0 verification

Milestone: SEOUL SHELL CORE INTERACTIONS V0 (four areas: scoped shell-host
ownership; Essential live associations; workspace dialogs/operations; explicit
split chooser). Machine: 8 GiB RAM, no GN/compile/test execution. Source
implementation is not deferred; only compilation and runtime validation are.

This pass implemented and verified area 1 (scoped shell-host ownership) against
the exact pinned source. Areas 2 to 4 are not yet implemented; their precise
specs are in the milestone and in
`docs/research/native-shell-production-completion-audit.md`.

## 1. Existing local repairs revalidated

Confirmed present and coherent in the working tree (unchanged by this pass except
where noted): `kOpenNewTab` via `chrome::AddAndReturnTabAt`; real reconciliation
delegation (`LifecycleCoordinator::RequestReconciliation` ->
`SeoulOrganizationService::RunLifecycleReconciliation` ->
`WindowWatcher::RescanExistingWindows`); semantic `ShellSnapshot` equality
excluding revision; direct `WorkspaceSwitchObserver` observation in
`ShellController`; user-facing status strings; registration after
`RootTabCollectionNode::Init()`; `ResetTabStrip` unregistration; production
`GetSeoulRootNode()` seam with no `root_node_for_testing()` in production; single
reversible integration patch. The patch applies and reverses cleanly.

## 2. Final shell-host ownership graph (VERIFIED AT SOURCE LEVEL)

The process-global `base::NoDestructor<std::map<VerticalTabStripRegionView*,
std::unique_ptr<SeoulShellRegionHost>>>` is removed. Ownership is now:

```
SeoulOrganizationService (per regular profile)
  \-- ShellService
        +-- std::map<ShellWindowKey, unique_ptr<ShellController>> controllers_
        \-- std::map<ShellWindowKey, unique_ptr<SeoulShellRegionHost>> hosts_
              \-- SeoulShellRegionHost (owns header/footer child views in the
                    initialized VerticalTabStripRegionView)
```

`SeoulShellRegionHost` is now an owned instance (no static `Attach`/`Detach`, no
`Hosts()` map, no `FromRegion`). Its destructor detaches and removes the shell
child views. `RegisterVerticalRegion` creates one owned host per window and
replaces any prior host deterministically (assigning a fresh `unique_ptr`
destroys the old host first, detaching its views). There is no process-global
state and no cross-profile host state (the map lives on the per-profile
`ShellService`).

## 3. Reset / reinitialization / shutdown behavior (VERIFIED AT SOURCE LEVEL)

- `ResetTabStrip` (patch) unregisters projection and shell at the START, before
  subscriptions/controller/root-node/child-view teardown; `UnregisterVertical
  Region` erases the owned host first (its destructor removes child views while
  the controller is still alive), then tears down the controller binding.
- Reinitialization: `InitializeTabStrip` registers again after `Init()`, creating
  a fresh host and controller binding.
- Window close: the region destructor (patch) calls `UnregisterVerticalRegion`,
  removing the binding.
- `ShellService::Shutdown` destroys all hosts before the controllers and the
  organization dependencies.
- No stale raw `VerticalTabStripRegionView*` remains and no global host survives a
  window.

## 4 to 14. Essentials, workspace dialogs, split chooser (NOT YET IMPLEMENTED)

These areas are not implemented in this pass and are reported honestly as open:

- Essential live associations and duplicate prevention (states, ephemeral
  association store, resolver, click activation, management): NOT implemented.
  `ShellController::OpenEssential` still issues one retained-tab open and the
  view model still derives Essential state without live association.
- Workspace dialogs and complete operations (real text input for create/rename,
  reorder/archive/restore/delete with stable ids): NOT implemented. The native
  Views input surface is required and was not authored blind.
- Explicit split chooser (candidate filtering, explicit pane-B selection, native
  chooser): NOT implemented. `CreateSplitFromActive` still selects the first
  other projected tab.

Precise repair specs for each remain in the production-completion audit.

## 15. Files created and changed (this pass)

Changed: `native/seoul/browser/shell/views/seoul_shell_region_host.{h,cc}`
(instance ownership), `native/seoul/browser/shell/shell_service.{h,cc}`
(per-window owned hosts). Created: this verification document.

## 16. BUILD and patch changes

None this pass. The integration patch and `BUILD.gn` are unchanged (the host
refactor is entirely Seoul-owned source). Single integration patch retained.

## 17. Tests authored

None this pass. The host-ownership behaviors (first/duplicate registration,
reset, reinitialization, window close, shutdown, no-stale-host, two-profile
isolation) require a real `VerticalTabStripRegionView` and therefore belong in
browser-test source; the existing browser-test bodies remain empty and are an
open item (audit 10/11). This is stated honestly rather than satisfied with a
pure test that cannot exercise the Views host.

## 18. Static checks and results

clang-format clean (all native C++); header include resolution OK; repo boundary
OK (224 tracked files); patch manifest valid; JSON valid; `git diff --check`
clean; global-shell-host-map scan: none; production testing-accessor scan: none;
hardcoded-URL scan (shell): none; no AI/vendor names; no em/en dashes.

## 19. Patch / materialization round-trip evidence

Patch series applies and reverses cleanly; materialize apply/verify/reverse
clean.

## 20. Checkout restoration evidence

`src` HEAD `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`, 0 tracked edits, no
`src/seoul` overlay, no patch left applied; verify-checkout PASS.

## 21. Remaining compile/runtime uncertainty

Not compiled or run on this machine: C++ compilation, linking, unit/browser-test
execution, real host attach/detach across reset and window close, focus and
accessibility trees, dialog/chooser behavior, split behavior, projection
visibility, runtime performance.

## 22. Exact next milestone

Implement the remaining three areas as source: (a) Essential live-association
resolver + ephemeral association store + duplicate-open prevention + management;
(b) native workspace create/rename dialogs and complete reorder/archive/restore/
delete operations with stable ids; (c) explicit split-partner chooser with
candidate filtering, plus real browser-test bodies connected to the runnable
target, then a capable-host compile.
