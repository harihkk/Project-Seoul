# Command layer V0 verification

Milestone: SEOUL OUTBOUND BROWSER COMMAND LAYER V0.
Pinned Chromium: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.
Authored on an 8 GiB Mac; no GN generation, compilation, or test execution performed.

## VERIFIED NOW

### V1 acceptance audit
- See `docs/research/native-core-v1-acceptance-audit.md` - all nine hard gates pass after repairs.

### V1 corrections applied
- `AcknowledgeRecovery()` and `recovery_required()` for corrupt pref recovery.
- Queue overflow triggers bounded reconciliation rescan; degraded state observable and cleared.
- `TabStripBridge::RescanExistingState()` re-inspects live tab strip.
- Runnable `test()` targets for organization, lifecycle-core, and command-core.
- `has_divider_ratio` for split contents validation.
- Pinned `clang-format` and `gn format` executed.

### Command API research
- See `docs/research/chromium-command-api-audit.md`.

### Command architecture
- Layers under `native/seoul/browser/commands/`: types, model facade, resolver interface, Chromium adapter, observation registry, executor, confirmation seam.
- Service owns executor; lifecycle coordinator confirmation callback after model mutation.

### Model-only commands
- All workspace, role, Essential, and routing-rule operations via `ModelCommandFacade`.

### Chromium-affecting commands (source authored)
- Open temporary/retained tab, activate, close, pin/unpin, move within window, create/dissolve split.

### Deferred commands (not faked)
- Cross-window move, window create/close, archived URL restore, routing execution, projection, previews, AI.

### No-replay
- In-memory registry only; shutdown rejects undispatched and marks dispatched unconfirmed as cleared/outcome unknown.

### Tests authored (not executed)
- Command validation, URL policy, model facade, observation registry, executor (with fakes).
- V1 acceptance tests updated for recovery, overflow, rescan, split ratio.

### Formatting
- `clang-format`: external Chromium checkout `buildtools/mac_arm64-format/clang-format` - OK
- `gn format`: external Chromium checkout `buildtools/mac/gn` - OK

### Patch round trip
- `node native/scripts/check-patch-manifest.mjs` - OK
- `bash native/scripts/patches.sh verify` - OK

### Materialization round trip
- apply → patch apply → patch reverse → materialize reverse - OK

### Checkout restoration
- HEAD `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`, no overlay, `verify-checkout.sh` PASSED

### Static scans
- No command replay persistence
- No cached tab indices in command layer
- No optimistic browser-state mutation before observation (membership follows lifecycle)
- Bounded in-flight registry (`kMaxInFlight = 64`)

## NOT VERIFIED UNTIL A CAPABLE HOST

- GN generation and include checking
- C++ compilation and linking
- Unit test execution
- Browser-adapter tests with real TabStripModel
- Real navigation, close cancellation, pin reordering, split operations
- Lifecycle/command timing and shutdown races

## Build commands (future)

```bash
native/scripts/materialize.sh apply
native/scripts/patches.sh apply
native/scripts/gen.sh
autoninja -C out/SeoulBaseline seoul_organization_unittests seoul_lifecycle_core_unittests seoul_command_core_unittests chrome
```

## Remaining compile/runtime risks

1. GN deps path adjustments for `//chrome/browser/ui/navigator` at gen time.
2. `AddToNewSplit` CHECK preconditions if active tab equals sole supplied index.
3. Pin role confirmation depends on lifecycle + post-command `PinTab` for workspace-pinned metadata.
4. Close async unload may leave commands awaiting until observer or cancellation.

## Next milestone recommendation

**Command layer V0 compile-and-prove on a capable host**: GN gen, fix deps, run all three unit-test executables, add browser-test target for `LiveTargetResolver` and `ChromiumMutationAdapterImpl`, then manual smoke of open/activate/close/pin/move/split on a test profile.
