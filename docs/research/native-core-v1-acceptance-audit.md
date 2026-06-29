# Native core V1 acceptance audit

Milestone: mandatory acceptance audit before SEOUL OUTBOUND BROWSER COMMAND LAYER V0.
Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.
Audited from the local working tree diff only (not GitHub history).

## Verdict

**ACCEPTED WITH REPAIRS** - all nine hard gates pass after the corrections listed below.
Compile/runtime verification remains deferred on the 8 GiB authoring host.

## Hard gate results

| Gate | Claim | Implementation | Verification | Accepted | Corrections |
| --- | --- | --- | --- | --- | --- |
| G1 Corrupt-pref recovery | Recovery copy + explicit resume | `seoul_organization_service.cc` `CopyActivePrefToRecovery`, `AcknowledgeRecovery`, `recovery_required()` | Source + `organization_recovery_unittest.cc` | yes | Added `AcknowledgeRecovery()`; removed permanent dead-end `suppress_persist_` without path |
| G2 Event-queue overflow | Bounded FIFO + reconciliation | `lifecycle_coordinator.cc` `HandleQueueOverflow`, `reconciliation_required()`, blocks ordinary events | `lifecycle_coordinator_unittest.cc` overflow/reconciliation tests | yes | Overflow now triggers service rescan callback; clears degraded state on reconciliation complete |
| G3 Real rescanning | Re-inspect browser state | `tab_strip_bridge.cc` `RescanExistingState`, `window_watcher.cc` | `lifecycle_rescan_unittest.cc`, coordinator rescan simulation tests | yes | `EnumerateExistingState` delegates to rescan; force rescan compares tab/split sets |
| G4 Runnable test targets | `test()` executables | `organization/BUILD.gn`, `lifecycle/BUILD.gn`, `commands/BUILD.gn` | BUILD review; future: `autoninja -C out/SeoulBaseline seoul_*_unittests` | yes | Added `seoul_organization_unittests`, `seoul_lifecycle_core_unittests`, `seoul_command_core_unittests` |
| G5 GN graph | Approach A single patch | `0001-seoul-native-core.patch`, parent BUILD files | Patch round trip OK | yes | Added `command_chromium` to `allow_circular_includes_from` |
| G6 Formatting | Pinned tools | `buildtools/mac_arm64-format/clang-format`, `buildtools/mac/gn` | Executed over all changed C++/BUILD files | yes | Previously skipped; now run |
| G7 Split replacement | Atomic `ReplaceSplitGroupContents` | `organization_model.cc`, coordinator uses `has_divider_ratio` | `lifecycle_splits_unittest.cc` | yes | Invalid explicit ratio rejected; omitted ratio preserved |
| G8 Transfer cleanup | Bounded pending transfers | `lifecycle_coordinator.cc` | Existing + reconciliation boundary tests | yes | No change required |
| G9 Persistence ownership | Service observer only | `seoul_organization_service.cc` `OnOrganizationChanged` | Source scan: no coordinator `schedule_persist` | yes | No change required |

## Recovery-mode final behavior (G1)

Policy B implemented:

1. Corrupt/unsupported load copies active bytes to `seoul.organization.v1.recovery` at most once.
2. Active pref left unchanged; in-memory model initializes via `EnsureDefaultWorkspace()`.
3. `recovery_required()` observable; `suppress_persist_` blocks writes until `AcknowledgeRecovery()`.
4. `AcknowledgeRecovery()` writes valid current model to active pref and resumes persistence.
5. In-memory mutations during recovery are allowed; they persist only after acknowledgement.

## Overflow/reconciliation final behavior (G2)

1. Bounded queue (`kMaxQueuedEvents = 128`); overflow sets `queue_overflow_` and `reconciliation_required_`.
2. Dropped event is not applied; pending queue cleared.
3. Service callback runs bounded rescan: reconciliation began → `RescanExistingWindows()` → reconciliation completed.
4. Ordinary lifecycle events blocked while `reconciliation_required_`.
5. Degraded flags cleared only after successful reconciliation completion.

## Rescan final behavior (G3)

1. First attach: `EnumerateExistingState()` idempotent via `enumerated_`.
2. Explicit rescan: `RescanExistingState()` re-reads TabStripModel; bypasses first-only guard.
3. Detects removed tabs (emit close), new tabs (emit existing insert), order changes (move), split add/remove/update.
4. Does not reopen URLs or fabricate tabs.

## Runnable test targets (G4)

| Target | Command (capable host) |
| --- | --- |
| `//seoul/browser/organization:seoul_organization_unittests` | `autoninja -C out/SeoulBaseline seoul_organization_unittests` |
| `//seoul/browser/lifecycle:seoul_lifecycle_core_unittests` | `autoninja -C out/SeoulBaseline seoul_lifecycle_core_unittests` |
| `//seoul/browser/commands:seoul_command_core_unittests` | `autoninja -C out/SeoulBaseline seoul_command_core_unittests` |

## Remaining compile/runtime uncertainty

1. Exact GN deps for `command_chromium` navigator/sessions paths at `gn gen` time.
2. `AddToNewSplit` preconditions (active tab must differ from supplied index) - runtime validation required.
3. Full `SeoulOrganizationService` recovery integration test with real `PrefService` deferred to capable host.
4. Browser-adapter integration tests not yet authored (Chromium objects required).

## Defects found in Cursor V1 diff (pre-repair)

| ID | Severity | Issue | Repair |
| --- | --- | --- | --- |
| V1-01 | critical | Permanent `suppress_persist_` without recovery exit | `AcknowledgeRecovery()` |
| V1-02 | critical | Overflow only set flag; no resync | Reconciliation callback + rescan |
| V1-03 | high | `EnumerateExistingState` no-op on repeat | `RescanExistingState()` |
| V1-04 | high | `source_set("unit_tests")` not runnable | `test()` targets |
| V1-05 | medium | Invalid split ratio silently preserved | `has_divider_ratio` flag |
