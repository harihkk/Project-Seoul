# Seoul Scenes Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

A Scene is a reusable browsing environment composed entirely of id-references
into other subsystems. This spec describes the source in
`native/seoul/browser/scenes/`. A Scene never inlines or duplicates another
subsystem's state; it points at a workspace, a theme, site layers, routing rules,
workflow shortcuts, a lifecycle policy, assistant defaults, and a compact
preference, and activation applies the referenced pieces in a deterministic
order.

## Scene as references

`native/seoul/browser/scenes/scene_types.h` defines `SceneDefinition`:

- `id` and `name`, and a `workspace_id` referencing an organization workspace.
- `theme_id` (empty means the global theme).
- `site_layer_ids`, `routing_rule_ids`, and `workflow_shortcut_ids`: lists of
  ids into the site-layers, routing, and workflow subsystems.
- `SceneLifecyclePolicy`: `archive_temporary_tabs`, `idle_archive_minutes`, and
  `restore_on_activation`. The header notes the actual bounds live in the
  organization model; a Scene only chooses within them.
- `SceneAssistantDefaults`: `allow_network`, `allow_cloud_models`,
  `max_sensitivity`, and `default_connectors`. These are defaults still capped by
  the user's global permission context at run time.
- `prefer_compact`.

Every field that points at another subsystem is a string id or a list of string
ids; no theme tokens, no CSS, no workspace contents are stored in the Scene.
Bounds include `kMaxScenes` (100), `kMaxSceneSiteLayers` (64),
`kMaxSceneWorkflowShortcuts` (32), and `kMaxSceneContextTools` (64).

## Validation against resolver callbacks

`native/seoul/browser/scenes/scene_registry.cc` validates a Scene without owning
the referenced entities. `SceneResolvers` supplies existence-check callbacks:
`workspace_exists`, `theme_exists`, and `site_layer_exists`. `Validate` checks
the schema version; a slug-form id and a bounded non-empty name; a non-empty
`workspace_id`; the per-list caps; that `site_layer_ids`, `routing_rule_ids`, and
`workflow_shortcut_ids` contain no duplicates; that `idle_archive_minutes` is in
1 to 10080; and then runs the resolvers, rejecting an unknown workspace, theme,
or site layer with the corresponding `SceneError`. Because existence is checked
through callbacks, the registry validates references without inlining their
state. `Upsert` validates before storing and enforces `kMaxScenes`.

## Deterministic ordered activation plan

`BuildActivationPlan` returns an ordered list of `SceneActivationStep` and
performs no mutation itself; the shell and services execute the steps in order.
The order is fixed: switch workspace first, then apply theme (only when a
`theme_id` is set), then enable each site layer in declared order, then install
each routing rule in order, then apply the lifecycle policy, then apply assistant
defaults, then set the compact preference. `SceneActivationStep::Kind` enumerates
exactly these step kinds, and each step carries only a `target_id` where one
applies.

## Re-validation at activation time

`BuildActivationPlan` re-runs `Validate` against the resolvers before producing
the plan. The source comment is explicit that referenced entities may have been
removed since the Scene was stored, so a Scene that pointed at a now-deleted
workspace, theme, or site layer fails to activate with a precise `SceneError`
rather than applying a stale reference.
