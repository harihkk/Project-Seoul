# Native core stabilization V1 verification

Milestone: NATIVE CORE STABILIZATION AND COMPILE-READINESS REPAIR V1.
Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.
Authored on an 8 GiB Mac; no GN generation, compilation, or test execution performed.

## VERIFIED NOW

### Pinned API research
- `BrowserWindowInterface::GetSessionID`, `GetTabStripModel`, `IsDeleteScheduled`, `TYPE_NORMAL`
- `TabStripModel::{count,GetTabAtIndex,active_index,IsTabPinned,GetSplitForTab,GetSplitData,closing_all}`
- `TabStripModelObserver` batch change types and `RemovedTab.session_id`
- `TabRemovedReason` enum (M149)
- `SplitTabChange` types; **no** `VisualsChange::is_intermediate()` at pinned revision
- `SessionRestoreObserver::OnSessionRestoreFinishedLoadingTabs` (global, not profile-scoped)
- Chromium `allow_circular_includes_from` + `browser_public_dependencies` pattern (e.g. `chrome/browser/media:impl`)

### Corrected source design
- GN Approach A circular-implementation targets with resolved BUILD files (no `RESEARCH REQUIRED`)
- Single patch `0001-seoul-native-core.patch` (replaces 0001 organization + 0002 lifecycle patches)
- Parent `seoul/BUILD.gn` and `seoul/browser/BUILD.gn` group targets for materialization

### Corrected persistence invariants
- `OrganizationLoadResult`: empty / loaded / corrupt / unsupported version
- Recovery pref `seoul.organization.v1.recovery`; active pref preserved on failure
- `AcknowledgeRecovery()` explicit transition; `recovery_required()` observable
- `SeoulOrganizationService` sole persistence scheduler via model observer + `PersistenceScheduler`

### Existing-window enumeration
- `TabStripBridge::EnumerateExistingState()` on first attach; `RescanExistingState()` on recovery/restore
- Emits `TabInsertKind::kExisting` for pre-existing tabs, pinned/active state, splits with upstream tokens
- Rescan detects post-enumeration tab/split changes

### Restore reconciliation
- `SessionRestoreWatcher` registers global `SessionRestoreObserver`
- On finish: reconciliation begin → `WindowWatcher::RescanExistingWindows()` → reconciliation complete
- Documented limitation: callback is global; profile filtering relies on owned `ProfileBrowserCollection`

### Bounded lifecycle queue
- FIFO cap `kMaxQueuedEvents = 128`; overflow sets `queue_overflow_` and `reconciliation_required_`
- Overflow triggers service reconciliation rescan; ordinary events blocked until complete
- Duplicate suppression only for semantically identical back-to-back events
- Shutdown stops draining; pending transfers cleared on shutdown/reconciliation

### Transfer cleanup
- `kWindowShutdown` / `kStripDestroyed` preserve membership
- Pending transfers capped, evicted oldest, cleared on reconciliation/shutdown/window gone

### Split replacement
- `OrganizationModel::ReplaceSplitGroupContents` atomic op
- Coordinator uses single mutation; dissolves only on explicit invalid proposal

### Tests authored (not executed)
- Runnable targets: `seoul_organization_unittests`, `seoul_lifecycle_core_unittests`
- `organization_header_unittest.cc`, `organization_invariants_unittest.cc`, `organization_recovery_unittest.cc`
- Updated `lifecycle_coordinator_unittest.cc`, `lifecycle_rescan_unittest.cc`, `lifecycle_splits_unittest.cc`
- Existing store/model/routing/persistence/identity tests retained

### Formatting
- `clang-format`: pinned tool at `seoul-chromium/src/buildtools/mac_arm64-format/clang-format` - OK
- `gn format`: pinned tool at `seoul-chromium/src/buildtools/mac/gn` - OK

### Patch round trip
- `node native/scripts/check-patch-manifest.mjs` - OK
- `bash native/scripts/patches.sh verify` - apply + reverse clean on pinned checkout

### Materialization round trip
- `materialize.sh apply` → `patches.sh apply` → `patches.sh reverse` → `materialize.sh reverse` - OK
- Final checkout: no `src/seoul` overlay

### Header direct-include audit
- `node native/scripts/check-header-includes.mjs` - OK

### Checkout restoration
- Chromium HEAD: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`
- `git status --porcelain` (tracked files): empty after restoration
- `verify-checkout.sh`: PASSED (read-only)

### Static scans (repository)
- No production `RESEARCH REQUIRED` markers under `native/seoul/`
- No `NotePersist` / duplicate `schedule_persist` scheduling
- No silent `if (applying_) return` discard (queue replaces it)
- Test-only URLs (`*.test` domains) in unit tests; no production hardcoded sites

## NOT VERIFIED UNTIL A CAPABLE HOST

- GN graph generation (`gn gen`)
- GN include checking (`check_includes`)
- C++ compilation and linking
- Unit test execution
- Real browser startup
- Real tab/window/split/session-restore events
- Shutdown race behavior under load
- Performance and memory impact of event queue and enumeration

## Remaining compile/runtime risks

1. Exact `deps` paths for `//chrome/browser/ui/tabs` and `//chrome/browser/sessions` may need adjustment at GN gen time.
2. `SessionRestoreWatcher` global callback may rescan non-owning profiles' windows if they share timing; mitigation is idempotent enumeration only.
3. M149 split ratio updates rely on task-turn persistence coalescing (no intermediate visuals flag).
4. Service corrupt-pref repair UI not implemented; `AcknowledgeRecovery()` API exists for future command/UI.

## Acceptance audit

See `docs/research/native-core-v1-acceptance-audit.md` for gate-by-gate acceptance after repairs.

## Readiness for command-layer milestone

V1 hard gates pass after local repairs. Command layer V0 source authored; see `docs/native/command-layer-v0-verification.md`.

| Command | Result |
| --- | --- |
| `node native/scripts/check-patch-manifest.mjs` | OK |
| `node native/scripts/check-header-includes.mjs` | OK |
| `bash native/scripts/patches.sh verify` | OK (apply + reverse) |
| `bash native/scripts/materialize.sh apply/reverse` | OK |
| `bash native/scripts/verify-checkout.sh` | PASSED |
| `python3 -m json.tool native/patches/manifest.json` | OK |
| `python3 -m json.tool native/chromium.lock.json` | OK |
| `git diff --check` (Project Seoul repo) | OK |
| `clang-format` / `gn format` | OK (pinned Chromium buildtools) |
