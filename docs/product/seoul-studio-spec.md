# Seoul Studio Specification

Status: Read-only runtime index source-connected; not compiled or
runtime-verified. Editing and application paths are not implemented.

Studio is the Canvas area for inspecting and eventually editing the systems
that shape how Seoul behaves: reasoning routes, Scenes, Themes, Site Layers,
routing, and workflows. Its first production slice is intentionally smaller
than that destination. It is a read-only index of live profile runtime state,
not a settings mockup and not a fixture catalog.

## Typed runtime projection

`PageHandler.GetStudioSnapshot` returns a bounded JSON document over Mojo from
the Canvas's exact window-bound `SeoulRuntimeService`. The browser constructs it
from three authoritative owners:

- `ProviderRegistry::Snapshot()` supplies local/cloud configuration and
  availability flags.
- `SceneRegistry::List()` supplies id-ordered Scene metadata.
- `ThemeRegistry::List()` supplies id-ordered, accessibility-validated Theme
  metadata and preview tokens.
- `SiteLayerRegistry::List()` supplies id-ordered, validated layer metadata.

The renderer performs only a closed Lit template mapping. It does not receive
HTML, CSS, JavaScript, DOM state, or inferred sample entries. Empty registries
produce explicit empty states rather than invented presets.

## Data minimization

The snapshot exposes provider booleans and counts, Scene name/reference counts,
validated Theme preview colors/preferences, and Site Layer names/scopes/counts.
It excludes credentials, local endpoint
URLs, selected model identifiers, raw provider errors, browsing content, and
capture bytes. Studio is profile-owned but still uses the Canvas window binding
so a tab-loaded or stale Canvas fails closed.

## Deliberately incomplete

The current surface cannot create, edit, delete, activate, or apply anything.
Those controls must not appear until their typed mutations validate references,
provide failure feedback, persist atomically, and have recovery plus capable-host
runtime tests. Workflow/routing editors, Scene
activation, and live Site Layer application remain separate production slices.
