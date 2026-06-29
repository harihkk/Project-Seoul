# Workspace projection V0 specification

## Projection

A projection is the ordered set of Seoul tab identities presented in one browser window for its active workspace. It is not a second tab model, detached store, profile, OS window, or recreated URL set.

## Window state

Each eligible window tracks:

- active workspace ID
- projected tab identities (ordered)
- selected projected tab
- projection generation
- empty-workspace state
- reconciliation / degraded state

## Visible membership

Normal tabs whose membership belongs to the window's active workspace are projected. Global Essentials remain profile-global model records - not duplicated live renderers per workspace.

## Workspace-pinned tabs

Visible only in their owning workspace. V0 does not map them to globally visible Chromium pinned tabs.

## Temporary and retained tabs

Both project in their owning workspace; role affects lifecycle/UI ordering, not visibility ownership.

## Splits

Projected only when workspace is active, both pane memberships valid, and both panes belong to that workspace. One inactive pane hides the entire split presentation.

## Groups

- Projected groups must not expose inactive-workspace tabs.
- Cross-workspace Chromium groups are inconsistent state - reconcile or surface; do not silently move tabs to repair.

## Multiple windows

V0 allows multiple windows to select the same workspace. Each window has its own active tab and projection state. Live tabs remain in their actual Chromium windows; projection does not duplicate tabs across windows.

## Switching

Use `SwitchWorkspaceForWindow` (transactional). Raw `SetActiveWorkspaceForWindow` is not the normal UI operation.

Steps:

1. Validate target workspace (exists, not archived)
2. Compute target projection
3. Select tab: last active valid → workspace-pinned → retained → temporary → empty
4. Activate via command layer if target exists
5. Commit active workspace only when coherent
6. Publish projection
7. On activation failure: preserve prior workspace and projection

Never commit an active hidden tab as success.

## Empty workspace

V0 returns explicit `ProjectionStatus::kEmptyWorkspace` for the future shell. Does not fabricate tabs or use `about:blank`.

## Hidden-tab activation

When Chromium activates a tab from another workspace (tab search, session restore, commands):

1. Lifecycle observes activation
2. Identify tab workspace
3. Switch window active workspace to match
4. Recompute projection
5. Do not switch back or create duplicate memberships

## Recovery

- Degraded: reject projection-changing ops; retain last safe visible state where possible
- Startup: prefer showing all tabs until projection coherent (`kFailOpen`)
- Failure: fail open - never make tabs inaccessible

## Horizontal mode

V0 workspace projection requires Seoul vertical-tab mode. Horizontal filtering deferred.
