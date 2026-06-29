# native/patches - Chromium integration patches

Seoul integrates with Chromium through two tracked mechanisms:

1. **Repository-owned source** under `../seoul/`, materialized into the checkout at
   `src/seoul/` by `../scripts/materialize.sh`. This is where Seoul's own code
   lives. It never edits upstream files.
2. **A minimal, ordered patch series** here, applied over the pinned base revision
   by `../scripts/patches.sh`, for the unavoidable cases where an upstream Chromium
   file must be touched (a dependency edge, a registration call site).

Files:
- `manifest.json` - the machine-readable, ordered series and its per-entry schema.
  `baseRevision` must equal the pinned revision in `../chromium.lock.json`.
- `chromium/` - the actual `.patch` files referenced by the manifest.

**This baseline contains zero Chromium patches.** When a Chromium modification
becomes unavoidable it must be:

- **Minimal** - the smallest change that achieves the goal; never a broad refactor.
- **Documented** - the manifest entry records description, rationale, and the
  upstream alternative that was rejected.
- **Ordered** - a unique ascending `order`; applied and reversed deterministically.
- **Checksummed** - a `sha256` in the manifest that matches the file on disk.
- **Verifiable** - applies with `git apply --check` and reverses with
  `git apply -R --check`.
- **Independently reviewable** - each patch stands on its own, against the exact
  locked revision, without depending on later patches.

Validate the manifest with `node native/scripts/check-patch-manifest.mjs` and the
apply/reverse behavior with `native/scripts/patches.sh verify`. Patches are an
overlay over a pinned upstream tree; they are never a substitute for extending the
existing upstream vertical-tab and split-view implementations, which must be
exhausted first. See `../README.md` for the architecture boundary.
