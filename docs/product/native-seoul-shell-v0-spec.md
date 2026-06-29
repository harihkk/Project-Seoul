# Native Seoul Shell V0 spec

## Internal name

**Seoul Vertical Shell** (`SeoulShell*` types)

## Hierarchy

```text
Seoul Vertical Shell
├── Context Header        (active workspace, switcher)
├── Essentials Deck       (profile-global destinations)
├── Workspace Pinned label
├── Projected Tab Area    (Chromium VerticalTabStripView - not duplicated)
└── Utility Bar           (new tab, launcher, split, status)
```

## Identity states

| State | Representation |
| --- | --- |
| Expanded | Workspace name, Essential labels, section labels |
| Collapsed | Workspace button icon text, Essential icons only |
| Empty workspace | Footer empty-state with new-tab action |
| Degraded | Status banner + reconcile action; projection fail-open |
| Recovery | Status banner + acknowledge recovery |

## Essentials V0

- Source: `OrganizationModel` `EssentialRecord` list
- Click: `CommandKind::kOpenRetainedTab` with saved `root_url` when no live resolver yet
- No page content, credentials, or screenshots

## Workspace pinned

- Section label when projection reports pinned memberships
- Actual pinned rows remain Chromium vertical tab views filtered by projection

## Tab actions

| Action | Path |
| --- | --- |
| New tab | `CommandExecutor::kOpenTemporaryTab` |
| Split | `CommandExecutor::kCreateSplit` (corrected adapter) |
| Pin/retain/temporary | `ModelCommandFacade` |
| Workspace switch | `WorkspaceSwitcher::SwitchWorkspaceForWindow` |

## Command launcher V0

Deterministic local catalog (`CommandLauncherCatalog`); no network/AI.

## Keyboard (source-level)

Action ID constants in `shell_actions.h` for future accelerator wiring; no Chromium shortcut theft in V0.

## Explicit non-goals

AI, previews, sync, final branding, fake sample workspaces/tabs.
