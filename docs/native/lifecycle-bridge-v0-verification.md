# Lifecycle bridge v0 verification

Milestone Part 15. An honest record of what was verified on this 8 GiB
development machine (which cannot build Chromium) and what remains unverified until
a capable build host. Plain ASCII. Nothing here claims that source inspection
proves runtime behavior.

## Verified now (results)

- Pinned-source API research: every Chromium/base API the bridge uses was read in
  the locked checkout (`docs/research/chromium-lifecycle-source-audit.md`). All 11
  bridge headers exist; `TabStripModel::IsTabPinned` and
  `sessions::SessionTabHelper::IdForTab` confirmed present.
- Ownership and teardown design: `docs/native/lifecycle-bridge-design.md`
  (per-window bridges, per-profile watcher, reverse-order shutdown).
- Identity model: `lifecycle_identity.h` (LiveWindowKey / PersistedWindowRef /
  LiveTabKey / PersistedTabRef, SessionID-backed, no pointers/indices persisted),
  unit-tested in `lifecycle_identity_unittest.cc`.
- Normalized events: `lifecycle_events.h` (no Chromium pointer, no page content).
- Lifecycle coordinator source: `lifecycle_coordinator.{h,cc}` (pure logic),
  unit-tested in `lifecycle_coordinator_unittest.cc`.
- Startup reconciliation source: staged, no timer; tested in
  `lifecycle_reconciliation_unittest.cc`.
- Persistence scheduling source: `persistence_scheduler.{h,cc}`, tested in
  `persistence_scheduler_unittest.cc`.
- Chromium adapters authored: `tab_strip_bridge.{h,cc}`, `window_watcher.{h,cc}`
  (thin, confirmed APIs only; exercised by future browser tests, not unit tests).
- Engine additions: `ForgetWindow`, `TouchTabActivated`, `ReorderTabMembership`,
  `FindMembershipIdByTabKey`, `FindSplitIdByUpstreamToken` (+ three change types).
- Formatting: pinned `clang-format --dry-run --Werror` reports CLEAN on all new and
  modified C++; `gn format` applied to both BUILD.gn files.
- Project checks: `npm run check` 85/85 pass; `check:scripts` OK; `check:json` OK;
  `check:manifest` OK (2 patches, baseRevision matches the lock); `check:boundary`
  OK (51 tracked files, no Chromium source/build/profile/secret leakage).
- Patch series: `patches.sh verify` performs a cumulative apply (1 then 2) and
  reverse (2 then 1) and leaves the checkout byte-identical; patch 2 applies on
  patch 1 and reverses cleanly. `git diff --check` clean on the applied series.
- Materialization: `materialize.sh` apply -> verify -> reverse is clean; the
  overlay matches `native/seoul/` and is fully removed afterward.
- Checkout integrity: `verify-checkout.sh` PASS; after all round trips,
  `src` tracked edits = 0, `src` HEAD = `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`,
  no `src/seoul` overlay remains, depot_tools at the locked revision.
- Content scans: no hardcoded production sites (only `.test` reserved-TLD fixtures
  in unit tests), no placeholders/TODO/stub, no Chromium pointer in the persisted
  layer, no UI symbols, no AI/network dependency, no timer-based restore guess in
  the bridge, no exception catch-all.

## Not verified until a capable host

- C++ compilation and linking of the engine additions and the bridge.
- Execution of the authored unit tests
  (`//seoul/browser/lifecycle:unit_tests` and the engine tests).
- Browser tests for the Chromium adapters.
- Real window creation/destruction, tab insertion/removal/movement.
- Real cross-window tab drag (transfer in/out).
- Real session restoration and the `OnSessionRestoreFinishedLoadingTabs` handshake.
- Real split observer behavior (add/remove/contents/visuals, divider drag).
- Real shutdown ordering and the final persistence flush.
- Persistence timing under real event bursts; crash recovery.
- Runtime memory and performance.
- Cycle-free GN placement of the adapters relative to `//chrome/browser(/ui)`.

Source-level inspection here establishes that the code is internally consistent,
uses only confirmed pinned APIs, formats cleanly, and integrates via a narrow,
reversible patch series, with the Chromium checkout left unchanged. It does not
prove the browser builds or runs.
