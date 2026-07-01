# Seoul Canvas Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

The Seoul Canvas is the persistent voice, conversational, and visual side panel
that renders validated SAUI surfaces. This spec describes the typed boundary the
Canvas sits behind, drawing on the SAUI sources in
`native/seoul/browser/saui/` and the voice session in
`native/seoul/browser/voice/voice_session.h`. The rich Canvas surface itself is
architecture intent, not built; the native shell today is Chromium Views.

## First-party, not arbitrary web content

The Canvas renders `AdaptiveSurface` documents built by the trusted Seoul
renderer from validated data. It is first-party: not an extension page, not a
remote site, and not arbitrary HTML. As the SAUI type header
(`native/seoul/browser/saui/saui_types.h`) states, a model emits data and the
trusted renderer creates the interface; no component carries executable code, raw
HTML, or event-handler scripts.

## Strict typed boundary

The Canvas boundary is typed in both directions. SAUI documents flow in:
`ParseSurface` (`native/seoul/browser/saui/saui_document.cc`) enforces charsets,
bounds, primitive-only values, and http(s)-only URLs, and `ValidateSurface`
(`native/seoul/browser/saui/saui_validator.cc`) enforces unique ids, binding-kind
matches, chart honesty, and accessible names. A surface that fails validation is
never rendered. Typed `ComponentEvent`s flow out
(`native/seoul/browser/saui/saui_events.h`): each carries a `SurfaceId`, a
`component_id`, a `ComponentEventKind` (`kActivate`, `kValueChanged`, `kSubmit`,
`kSelect`, `kDismiss`), an optional `action_id`, and a typed value. The Canvas
never emits raw DOM state or click coordinates, and a model never receives them.
Resolution of an event to a tool call, approval, or navigation happens in the
Canvas session layer against the surface's validated action list.

## Persistence

An `AdaptiveSurface` carries a `pinned` flag and a stable `SurfaceId`, and
`SurfaceToValue`/`ParseSurface` round-trip it for persistence. The
`saui_document.h` header names the persistence paths: pinning, Scene attachment,
and session restore. A pinned surface can be kept in place; a surface can be
attached to a Context Thread (via a `kSurfaceReference` context item; see the
Context Threads spec) or to a Scene; and a `kDashboard` surface is the persistent,
data-refreshing `SurfaceKind` a surface converts to when kept as a dashboard.

## Live updates via typed patches

Live surfaces change through `SurfacePatch` operations
(`native/seoul/browser/saui/saui_patch.cc`) addressed by stable component and
data-entry ids: set-props, set-state, upsert-data-entry, append-series-points,
append-child, remove-component, replace-component, and set-actions.
`ApplySurfacePatch` applies atomically on a working copy, re-validates, bumps the
`revision` once, and returns an `AppliedPatch` listing exactly the changed ids, so
the Canvas updates only what changed rather than rebuilding the whole panel. A
component's `UpdatePolicy` of `kLive` marks where the renderer should expect
in-place refreshes.

## Coordination with the voice session

The Canvas shares the voice session in
`native/seoul/browser/voice/voice_session.h`: voice, typed text, and Canvas
interaction drive one state machine. A follow-up (spoken or typed) enters
`kUnderstanding` on the same session and thread, and its result updates the open
surfaces in place through typed patches keyed by the stable component ids, rather
than opening a new panel. Spoken references resolve to visible component ids
through the reference resolver (see the Voice spec), so "make that chart a bar
chart" targets the same surface the user is looking at.

## Architecture intent for the rich surface

The native shell stays Chromium Views. The rich Canvas described here would be a
first-party, browser-owned WebUI behind a Mojo boundary that speaks only the
typed SAUI-document-in, `ComponentEvent`-out contract described above. That WebUI
surface is architecture intent and is not built; what exists today is the typed
document, catalog, validator, patch, selection, and event model that such a
surface would render.
