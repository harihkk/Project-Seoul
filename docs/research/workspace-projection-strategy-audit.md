# Workspace projection strategy audit (M149)

Pinned revision: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.

## A. Presentation filtering - SELECTED for V0

Keep all tabs in `TabStripModel`; hide inactive-workspace tabs in the vertical view layer only.

| Concern | Assessment |
| --- | --- |
| Session preservation | Safe - session restore inserts all tabs; filter applied post-init |
| Renderer ownership | Unchanged - Chromium owns WebContents |
| Vertical view filtering | Matches collapsed-group pattern (`SetVisible` + layout `visible` flag) |
| Active tab | Must belong to projection; hidden-tab activation switches workspace |
| Keyboard traversal | Hidden views must not be focusable (V0 adapter seam) |
| Splits | Hide split shell when either pane is inactive workspace |
| Groups | Parent visible only when a projected child requires it |
| Pinned tabs | Seoul workspace-pinned role; not mapped to global Chromium pin region |
| Tab search | Hidden-tab activation triggers workspace switch (Phase 7) |
| Drag/drop | TabStripModel unchanged; 1:1 node tree preserved |
| Accessibility | Hidden tabs excluded from a11y tree via view visibility |
| Horizontal mode | Separate codepath - V0 requires vertical-tab mode |

**Seam:** `VerticalTabStripRegionView::InitializeTabStrip()` + `VerticalPresentationFilter` via `VerticalPresentationAdapter`.

## B. New TabCollection type - REJECTED

Would require changes across `components/tabs/`, movement, session serialization, drag, vertical nodes, and public APIs. High blast radius; no safety advantage over presentation filtering for V0.

## C. Detaching inactive tabs - REJECTED

`TabStripModel` and browser-window code assume contiguous window ownership. Detached tabs break session service visibility, unload, tab features, and crash recovery. No pinned-source path supports long-lived detached storage.

## D. Window-per-workspace - REJECTED

Avoids view filtering but proliferates OS windows, complicates cross-window moves, splits, macOS focus, and contradicts Seoul's shared live tab model. Not selected merely because it is simpler.

## Selection

**Presentation filtering** preserves Chromium-owned tabs, session restore, and the existing vertical 1:1 view-model mirror while meeting workspace visibility requirements.
