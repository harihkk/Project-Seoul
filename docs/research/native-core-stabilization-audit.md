# Native core stabilization audit

Milestone: NATIVE CORE STABILIZATION AND COMPILE-READINESS REPAIR V1.
Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644` (M149.0.7827.201).
Plain ASCII. Compile/runtime verification deferred on the 8 GiB authoring host.

Sources audited: every file under `native/seoul/browser/organization/`,
`native/seoul/browser/lifecycle/`, `native/patches/`, and referenced docs.

## Defect register

| ID | Severity | Affected file(s) | Exact behavior | Violated invariant | Smallest correction | Verification | Compile deferred |
| --- | --- | --- | --- | --- | --- | --- | --- |
| D01 | critical | `seoul_organization_service.cc` | After corrupt/unsupported pref parse, `WriteToPrefs()` overwrote stored bytes with a fresh default snapshot | Corrupt preferences must not be silently destroyed | Recovery pref copy; `suppress_persist_`; no write on corrupt load; expose `OrganizationLoadResult` | `organization_invariants` + service source review | yes |
| D02 | high | `organization_model.cc` `DeleteWorkspace` | Routing rules referencing deleted workspace as `result.target_workspace` were retained | Deleted workspace references must not dangle | Erase rules where source OR specific-workspace target matches | `OrganizationInvariantsTest.DeleteRoutingTargetWorkspace` | yes |
| D03 | high | `organization_model.cc` | `AddTabMembership` / `MoveTabToWorkspace` / `CreateSplitGroup` accepted archived workspaces | Archived workspaces cannot accept live tabs | Reject with `kArchivedWorkspaceCannotActivate` | `OrganizationInvariantsTest.RejectArchivedDestination*` | yes |
| D04 | high | `organization_model.cc` `RestoreArchivedTab` | Always restored as retained while copying pinned `saved_root_url` | No retained record with pinned-only metadata | Store `original_role` in archive; restore pinned as retained without saved root | `OrganizationInvariantsTest.ArchiveRestoreRoleBehavior` | yes |
| D05 | high | `organization_model.cc` / `organization_store.cc` | Empty upstream split token accepted; duplicate tokens not rejected at load | Live splits require valid nonempty upstream token | Validate on create/load; `kInvalidUpstreamSplitToken`, `kDuplicateSplitToken` | `OrganizationInvariantsTest.DuplicateSplitTokenRejected` | yes |
| D06 | high | `organization_store.cc` | Walked unbounded membership/split/window lists; invalid Essential enum defaulted | Reject oversize stored snapshot before walking lists; strict enum parsing | Pre-list size caps; reject invalid Essential kind | `OrganizationInvariantsTest.OversizedMembershipListRejected`, `InvalidEssentialEnumRejected` | yes |
| D07 | high | `organization_model.cc` `LoadSnapshot` | Missing aggregate bounds, duplicate window keys, invalid archive workspace refs | Snapshot must always reload after valid mutations | Added total caps, per-workspace split caps, duplicate window key rejection, archive workspace validation | `OrganizationInvariantsTest.*` + round-trip test | yes |
| D08 | medium | `organization_types.h` | Used `kOrganizationSchemaVersion` without including `organization_limits.h` | Headers must be self-contained | Direct include of `organization_limits.h` | `organization_header_unittest.cc`, `check-header-includes.mjs` | yes |
| D09 | critical | `lifecycle_coordinator.cc` | `if (applying_) return` silently dropped reentrant events | Lifecycle events must not be silently discarded | Bounded FIFO queue with overflow flag | `LifecycleCoordinatorTest.ReentrantEventIsQueuedAndApplied`, `QueueOverflowSurfacesFailure` | yes |
| D10 | high | `lifecycle_coordinator.cc` + `seoul_organization_service.cc` | Coordinator called `schedule_persist` AND service observer scheduled writes | One persistence scheduling authority | Remove coordinator persistence callback; service observer only | Source scan: no `NotePersist` / `schedule_persist` | yes |
| D11 | high | `window_watcher.cc` | `Track()` emitted window event only; no tab/split enumeration | Existing eligible tabs must be observed at attach | `TabStripBridge::EnumerateExistingState()` after window discovered | Coordinator enumeration tests (`kExisting` insert kind) | yes |
| D12 | high | (missing) | No session-restore completion wiring | No timer-based restore guess; rescan at reliable completion | `SessionRestoreWatcher` on global finish signal + profile-filtered rescan | `SessionRestoreWatcher` source + reconciliation tests | yes |
| D13 | high | `tab_strip_bridge.cc` | All `kDeleted` removals classified as genuine close during window shutdown | Window shutdown must preserve organization metadata | Classify via `closing_all()` / `IsDeleteScheduled()` as `kWindowShutdown` | `LifecycleCoordinatorTest.WindowShutdownRemovalPreservesMembership` | yes |
| D14 | high | `lifecycle_coordinator.cc` `HandleSplitContentsChanged` | Dissolve then recreate as two mutations | Split updates must be atomic | `ReplaceSplitGroupContents` model op | `SplitsTest.AtomicContentsReplacementPreservesSplitId` | yes |
| D15 | medium | `tab_strip_bridge.cc` | Comment claimed M149 `VisualsChange::is_intermediate()` (does not exist) | Do not copy newer API into M149 | Document coalescing limitation honestly | Source comment + verification doc | yes |
| D16 | medium | `lifecycle_identity.h` | Comments claimed SessionID stability as established everywhere | Identity guarantees must be honest | Document verified and unresolved boundaries | This audit + verification doc | yes |
| D17 | critical | `organization/BUILD.gn`, `lifecycle/BUILD.gn`, patches | `RESEARCH REQUIRED` GN placement; invalid public dep path; patch 2 duplicated lifecycle dep | GN graph must be coherent without suppression | Chromium circular-implementation pattern A; single patch | Patch round trip; BUILD review | yes (GN gen deferred) |
| D18 | low | `organization_invariants_unittest.cc` (initial) | Split round-trip test referenced missing second tab | Tests must match model preconditions | Fixed test data | Test source review | yes |

## GN integration decision

**Chosen: Approach A - Chromium circular-implementation-target pattern.**

- Pure targets remain: `organization_model`, `lifecycle_core`.
- Chromium-facing targets `organization` and `lifecycle` publicly depend on `//chrome/browser:browser_public_dependencies`.
- `//chrome/browser:browser` adds `//seoul/browser/organization` to `deps` (lifecycle linked transitively).
- Both Seoul targets listed in `allow_circular_includes_from`.
- Single patch `0001-seoul-native-core.patch` replaces the prior two-patch series.

Approach B (glue sources listed directly in `browser`) was rejected: it would inflate the monolith target and diverge from established `*:impl` placement without reducing total complexity.

## Session identity (pinned M149 research)

Verified paths:
- `chrome/browser/ui/browser_window/public/browser_window_interface.h` - `GetSessionID()`
- `components/sessions/content/session_tab_helper.h` - `IdForTab`
- `chrome/browser/sessions/session_restore_observer.h` - global finish callback (not profile-scoped)

Unresolved boundaries documented in `lifecycle_identity.h`: recently-closed restoration may assign new SessionIDs; Seoul leaves references unresolved rather than fabricating matches.

## Persistence policy (corrupt prefs)

Active pref `seoul.organization.v1` is left unchanged on corrupt/unsupported load.
First failure copies bytes to `seoul.organization.v1.recovery`.
In-memory state initializes via `EnsureDefaultWorkspace()` only.
No automatic overwrite of active pref until explicit repair (future milestone).
