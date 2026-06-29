# Lifecycle bridge gap audit

Milestone Part 1. Before building the inbound Chromium window/tab lifecycle bridge,
the existing Organization Engine V0 was audited to confirm it exposed enough
operations for every real Chromium event, and to make only narrowly justified
additions. Nothing here redesigns the engine. Plain ASCII.

Sources audited: `native/seoul/browser/organization/organization_model.h`,
`organization_types.h`, and the bridge consumer
`native/seoul/browser/lifecycle/lifecycle_coordinator.cc`.

## Event-to-capability table

| Chromium event / bridge need | Existing model capability | Already existed | Minimal addition |
| --- | --- | --- | --- |
| Register a live window, project a workspace | `SetActiveWorkspaceForWindow`, `EnsureDefaultWorkspace`, `default_workspace`, `ActiveWorkspaceForWindow` | Yes | None |
| Window closed: drop the window projection, keep workspaces | (none: only set existed) | No | `ForgetWindow(window_key)` |
| New tab inserted | `AddTabMembership(workspace, tab_key, role)` | Yes | None |
| Look up a tab's membership by its key | (none public; `tab_index_` was private) | No | `FindMembershipIdByTabKey(tab_key)` |
| Genuine close | `RemoveTabMembership(id)` | Yes | None |
| Detach for transfer (preserve membership) | handled in the coordinator (pending transfer); no model change | Yes | None |
| Move tab between workspaces | `MoveTabToWorkspace(id, target)` | Yes | None |
| Tab role change (pin / unpin / retain / temporary) | `PinTab`, `UnpinTab`, `RetainTab`, `MarkTabTemporary` | Yes | None |
| Activation: record last-active | (none: workspace last-active only) | No | `TouchTabActivated(id)` |
| Activation switches window workspace | `SetActiveWorkspaceForWindow` | Yes | None |
| Intra-window move: deterministic ordering | (none: order was set only on create/move-workspace) | No | `ReorderTabMembership(id, order)` |
| Split add / layout / dissolve | `CreateSplitGroup`, `UpdateSplitLayout`, `DissolveSplitGroup` | Yes | None |
| Resolve a split by its Chromium upstream token | (none; only `FindSplit(id)`) | No | `FindSplitIdByUpstreamToken(token)` |
| Load persisted state, deterministic snapshot | `LoadSnapshot`, `ToSnapshot` | Yes | None |
| Observer notifications | `AddObserver` / `RemoveObserver`, `OrganizationModelObserver` | Yes | None |
| Auto-archive eligibility (not used by inbound bridge yet) | `EligibleForAutoArchive`, `ArchiveTab`, `RestoreArchivedTab` | Yes | None |

## Narrow additions made

All five are pure C++ over `//base`, validate before mutating, return typed
`OrganizationError` on failure, and emit exactly one change on success.

1. `ForgetWindow(std::string_view window_key)` emits `kWindowForgotten`. A window
   close must clear that window's active-workspace projection without deleting the
   workspace or its memberships (invariants 6 and the window-projection model).
2. `TouchTabActivated(const TabMembershipId&)` emits `kTabActivated`. Activation
   must update a tab's `last_active_at` so auto-archive eligibility is meaningful;
   no prior op updated tab-level last-active.
3. `ReorderTabMembership(const TabMembershipId&, int order)` emits
   `kMembershipReordered`; rejects a negative order with `kInvalidOrder` and an
   unchanged order with `kNoOpRejected`. Needed for an intra-window tab move,
   which is ordering only and must never be misread as a workspace change.
4. `FindMembershipIdByTabKey(std::string_view) const` is an O(log n) lookup the
   bridge performs on almost every event; it replaces a full membership scan.
5. `FindSplitIdByUpstreamToken(std::string_view) const` resolves a serialized
   `split_tabs::SplitTabId` to a Seoul split id with a bounded scan (the split
   count is capped by `organization_limits.h`). This lets the coordinator hold no
   token cache, so it stays consistent when a split is dissolved implicitly by
   closing a pane.

New `OrganizationChangeType` values: `kWindowForgotten`, `kMembershipReordered`,
`kTabActivated`.

## Rejected broad redesigns

- Threading a mutation-origin field through every record and every change struct.
  Instead the coordinator exposes a scoped `current_origin()` (Chromium
  observation, startup reconciliation, future user command, recovery). This is
  the smallest mechanism that lets the future outbound layer avoid reacting to
  its own changes, and it touches none of the persisted schema.
- Adding any outbound command API (open/close/move/pin/split/switch). That is a
  separate milestone; this one is inbound only.
- Replacing the opaque `tab_key` string identity with a new identity subsystem.
  The lifecycle identity types wrap a SessionID value and serialize to the same
  `tab_key`/`window_key` strings the model already stores.
- Putting Chromium observers inside the pure model. The model stays Chromium-free;
  all observation lives in the bridge layer.

## Remaining compile-time uncertainty

The five additions are pure C++ over `//base` and were not compiled on this 8 GiB
host (the machine cannot build Chromium). They are exercised by the authored unit
tests, which also await a build host. The bridge's Chromium-facing adapters
(`tab_strip_bridge`, `window_watcher`) depend on `//chrome/browser(/ui)` targets
whose cycle-free GN placement is resolved at the build host; see
`docs/research/chromium-lifecycle-source-audit.md` and
`docs/native/lifecycle-bridge-design.md`.
