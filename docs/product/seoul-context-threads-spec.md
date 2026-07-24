# Seoul Context Threads Specification

Status: Current compile and runtime evidence is maintained in the product
readiness report.

A Context Thread holds only user-approved working context. This spec describes
the source in `native/seoul/browser/context/`. The design guarantee is
structural: forbidden content classes are not representable by the item type, and
what does get in is filtered again before any cloud call.

## Approved-only working context

`native/seoul/browser/context/context_thread.h` defines `ContextThread` as a
named, optionally archived list of `ContextItem`. Items are added only through
`AddItem`, which validates and assigns an id; there is no bulk import of page
state or history. Bounds: `kMaxContextItems` (200), a bounded title, and bounded
excerpt and note bodies.

## Forbidden content is unrepresentable

`ContextItemKind` enumerates the only item classes a thread may contain:
`kTabReference`, `kExcerpt`, `kFileReference`, `kNote`, `kSurfaceReference`,
`kTaskOutput`, `kCitation`, `kWorkflowReference`, and `kDecision`. There is no
`kPassword`, `kCookie`, `kToken`, `kRawAudio`, or `kFullHistory` member, and the
header states this deliberately: forbidden content is not representable. A
`ContextItem` carries a title, a `reference` (a tab key, file handle, url, or
surface id), a display `origin`, and a bounded `text` body for excerpts and
notes; a tab reference is a stable key plus title plus origin, never a page dump.

## Flagged-sensitive rejection

`ContextItem` has a `flagged_sensitive` flag set by the caller when the source is
known to contain sensitive data (a password field, an auth cookie, raw audio).
`AddItem` rejects any such item with `kSensitiveItemRejected` before validation.
The source comment frames this as the core guarantee: even if a caller mislabels
a sensitive source and routes it here, the flag stops it.

## Kind-specific validation

`ValidateItem` enforces well-formedness per kind: reference-bearing kinds
(`kTabReference`, `kFileReference`, `kSurfaceReference`, `kTaskOutput`,
`kWorkflowReference`, `kCitation`) require a non-empty `reference`; `kExcerpt`
requires non-empty text within `kMaxContextExcerptLength` (20000); `kNote` and
`kDecision` require non-empty text within `kMaxContextNoteLength` (8000). Titles
are bounded. Failures return precise `ContextError` values.

## MinimizeForCloud

`MinimizeForCloud` in `context_thread.cc` produces the `CloudContextScope`, which
the header calls the only path by which thread content reaches a cloud model. It
enforces four rules directly in code:

- Archived threads are never sent: an archived thread yields an empty scope
  immediately.
- Sensitive-flagged items are skipped as defense in depth, even though they
  should be impossible to add to a thread.
- When `include_bodies` is false, excerpt, note, and decision bodies are
  stripped while their references are kept, so a model can ask for a specific
  source rather than receiving everything.
- The byte budget drops rather than truncates: when adding an item would exceed
  `max_bytes`, the loop breaks and the remaining items are omitted whole,
  keeping every included item intact.
