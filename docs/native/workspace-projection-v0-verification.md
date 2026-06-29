# Workspace projection V0 verification

Machine: 8 GiB RAM - no GN, Ninja, compilation, or test execution.

## VERIFIED NOW (source-level)

### Completion audit

- `docs/research/workspace-projection-v0-completion-audit.md`

### Split creation (corrected)

- Normal path uses `TabStripModel::AddToNewSplit`, not `RestoreSplit`
- Both panes validated by live identity; indices re-resolved immediately before dispatch
- M149 constraint: one explicit index vector entry plus active tab (pane B activated explicitly before dispatch)
- Creation source: `SplitTabCreatedSource::kKeyboardShortcut` (documented limitation - no dedicated Seoul enum at M149)
- No manual `RecordSplitTabCreated`; no duplicate metrics injection
- `DissolveSplit` checks split existence before removal

### Live-state feed

- `LiveWindowStateProvider` owned by `WindowWatcher`
- `TabStripBridge` publishes on tab/split/pin/move/activation/rescan
- `ProjectionService` observes snapshots and updates controllers

### ProjectionService wiring

- Constructed by `SeoulOrganizationService` with live-state provider
- One `WindowProjectionController` + `WorkspaceSwitcher` + `VerticalPresentationAdapter` per eligible window
- Patch registers/unregisters `VerticalTabStripRegionView` with service

### Vertical presentation adapter

- Real `ApplyToVerticalTabStripRegion`: bottom-up visibility on `RootTabCollectionNode`
- Split: all pane tabs must be projected (no half-split)
- Group: visible if any projected child
- Containers: visible if any visible descendant
- Fail-open shows all nodes; no view/node removal

### Transactional workspace switching

- Phases: validating → calculating → awaiting activation → committing → applied / rejected / cancelled / outcome unknown
- Submits activation when needed; `CommandCompletionObserver` waits for completion
- Rollback preserves prior workspace on failure
- External activation commits workspace without second activation command

### Focus and accessibility (source paths)

- Hidden tabs: `SetVisible(false)`, events disabled, `SetAccessibleIgnored(true)`
- Focus cleared/transferred when focused view hidden
- `GetDefaultFocusableChild` patched to use projected visible active/first tab

### Fail-open

- Controller + adapter restore all tab visibility and accessibility exposure

### Pin suppressor

- `ShouldDeferLifecyclePinRoleMutation` defers role mutation only; live pin snapshots still published

### Tests authored

- `seoul_projection_core_unittests`, `seoul_lifecycle_state_unittests`, browser test skeletons
- Not executed on this host

### Static verification

See Phase 14 results in final milestone response (patch round-trip, manifest, header includes, scans).

## NOT VERIFIED UNTIL CAPABLE HOST

- GN generation
- C++ compilation and link
- Unit-test execution
- Browser/view-test execution
- Actual tab visibility in running browser
- Actual focus transfer and keyboard traversal
- Accessibility tree in running browser
- Drag behavior, startup, session restore UX
- Real split creation UX and performance

## Native shell

Do **not** begin final Seoul shell until capable-host runtime verification passes.
