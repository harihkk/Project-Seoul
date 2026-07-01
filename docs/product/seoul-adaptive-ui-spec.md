# Seoul Adaptive UI (SAUI) Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

SAUI is the Seoul-owned declarative surface layer. A model emits data conforming
to a fixed schema; a trusted Seoul renderer builds the interface. No component
carries executable code, raw HTML, or event-handler scripts. This spec describes
what the source in `native/seoul/browser/saui/` actually enforces.

## Security boundary

The boundary is explicit in the module header comments and in the code: the
model produces a bounded, typed data document; Seoul owns the catalog, the
parser, the validator, and the renderer. A component action never runs code. It
maps to a typed `SurfaceAction` whose payload is re-validated by the receiving
layer (tool schema, workflow editor, command validation). SAUI guarantees shape
and bounded size only.

## Document model

`native/seoul/browser/saui/saui_types.h` defines the model:

- `AdaptiveSurface`: `SurfaceId` (a UUID allocated by Seoul, never the model),
  `SurfaceKind`, `schema_version`, `title`, a tree of `ComponentNode`, a
  `data` map of named `DataEntry`, a `SurfaceAction` list, `SurfaceProvenance`,
  `pinned`, and a monotonically increasing `revision`.
- `ComponentNode`: charset-validated `id`, `ComponentType`, `props` dict,
  `bindings` (slot name to data-entry name; the primary slot is "data"),
  `accessible_name`, `ComponentState`, `UpdatePolicy` (`kStatic`/`kLive`),
  `action_ids`, and `children`.
- `DataEntry`: a `DataEntryKind` of `kScalar`, `kRecord`, `kSeries`, or
  `kTable`, plus an optional `DataProvenance`. Data never lives inside props;
  components bind entries by name.
- `SurfaceActionKind`: `kToolCall`, `kLocalState`, `kWorkflowEdit`,
  `kBrowserAction`, `kTaskApproval`, `kNavigate`.

`SurfaceKind` covers `kResponse`, `kForm`, `kDashboard`, `kReport`, `kApproval`,
`kWorkflowCanvas`, `kTaskStatus`, and `kMonitor`.

## Trusted component catalog

`native/seoul/browser/saui/saui_catalog.cc` holds one `ComponentTypeInfo` row
per `ComponentType`, in enum declaration order. A `static_assert` ties the table
length to the enum so a type without a row (or the reverse) fails the build. The
catalog defines 109 component types across ten `ComponentCategory` values:
`kFoundation`, `kLayout`, `kInput`, `kData`, `kChart`, `kDomain`, `kWorkflow`,
`kTask`, `kMap`, and `kCode`. Domain families include weather, markets,
products, travel, calendar, research, and files.

Each row carries per-type binding rules: whether the type is a `container`, an
`input`, a `chart`, whether it `requires_accessible_name`, the
`accepted_binding_kinds` bitmask for the "data" slot (`kBindScalar`,
`kBindRecord`, `kBindSeries`, `kBindTable`, or `kBindNone`), whether a binding is
`binding_required`, and the required prop keys. `FindComponentTypeByName`
resolves a wire name; an unknown name is rejected at parse.

## Structural parsing

`native/seoul/browser/saui/saui_document.cc` (`ParseSurface`) consumes untrusted
`base::Value` and yields a bounded `AdaptiveSurface` or a precise `SauiError`.
It enforces:

- Identifier charset: `IsValidSauiIdentifier` requires `[A-Za-z][A-Za-z0-9_-]`
  up to `kMaxComponentIdLength` (64). Prop keys via `IsValidPropKey` require
  `[a-z][a-z0-9_]` up to 40 characters.
- Forbidden keys: any `on...` handler-shaped key and the markup keys `html`,
  `script`, `srcdoc`, `innerhtml`, `style` are rejected outright, distinguished
  as `kForbiddenPropertyKey` so the generator cannot even express the intent.
- Primitive-only values: prop and cell values must be string, bool, or finite
  number. Lists are allowed only for known structured keys (`options`,
  `columns`, `chips`, `sources`, `segments`).
- URL validation: `href`, `src`, and `source_url` values, source entries, and
  navigate targets must pass `IsHttpUrl` (valid GURL with an http or https
  scheme), bounded by `kMaxUrlPropLength`.
- Bounds and depth: `kMaxSurfaceComponents` (512), `kMaxComponentDepth` (12),
  `kMaxChildrenPerComponent` (64), `kMaxDataEntries` (128), `kMaxSeriesPoints`
  (10000), `kMaxTableRows` (2000), and the other caps in
  `native/seoul/browser/saui/saui_limits.h`. Children under a non-container type
  are rejected as `kChildrenNotAllowed`.

## Semantic validation

`native/seoul/browser/saui/saui_validator.cc` (`ValidateSurface`) walks the tree
in deterministic order and returns the first violation. It requires unique
component ids and unique action ids, resolvable bindings whose kind matches the
catalog mask (`kBindingKindMismatch` otherwise), catalog-required props, and a
non-empty `accessible_name` where the row demands one.

Chart honesty is enforced by `EntryChartEligible` and `ValidateChartComponent`.
A chart-backing entry must carry provenance with a named source and both
`retrieved_at` and `effective_at` timestamps, and must be a series with at least
two points or a table with at least two rows (or one row with two or more
columns). A single value is never chart-eligible. Charts require title and
labeled axes and units via required props. A bar or stacked-bar chart must
declare `baseline_zero`; if false, it must also set `axis_truncation_indicated`,
else `kTruncatedAxisNotIndicated`. Other charts declaring a nonzero `y_min` must
indicate truncation. Pie charts are capped at `kMaxPieSlices` (12).

## Typed incremental patches

`native/seoul/browser/saui/saui_patch.cc` applies bounded updates addressed by
stable ids. `PatchOpKind` covers set-props, set-state, set-title,
upsert-data-entry, append-series-points, append-child, remove-component,
replace-component, and set-actions, capped at `kMaxPatchOps` (128).
`ApplySurfacePatch` works on a copy: if any op or the re-validated result is
invalid, the surface is left untouched. On success the `revision` bumps once and
an `AppliedPatch` summary lists the changed ids. The whole Canvas is never
rebuilt for a minor update.

## Typed events

`native/seoul/browser/saui/saui_events.h` defines `ComponentEvent` with a
`SurfaceId`, `component_id`, `ComponentEventKind` (`kActivate`, `kValueChanged`,
`kSubmit`, `kSelect`, `kDismiss`), an optional `action_id`, and a typed value.
The Canvas never emits raw DOM state or click coordinates, and a model never
receives them.

## Presentation selection

`native/seoul/browser/saui/saui_selection.cc` (`SelectPresentation`) chooses a
`PresentationForm` from explicit `PresentationSignals` using deterministic rules,
not raw model text. An explicit text-only request wins unless two or more
required inputs are missing, in which case one `kEditableForm` beats repeated
clarifying questions. A single lone scalar stays `kTextOnly`. With no eligible
visual data it stays `kTextOnly`. A comparison across entities selects
`kComposedSurface`; otherwise more than one visual entry selects
`kComposedSurface` and a single entry selects `kSingleComponent`. Monitoring and
persistent task results set `persist`. Each decision records its
`SelectionReason` values.
