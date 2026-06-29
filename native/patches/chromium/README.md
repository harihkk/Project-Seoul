# native/patches/chromium - Chromium integration patch files

This directory holds the actual `.patch` files for the ordered Chromium
integration series. The series and its metadata live in `../manifest.json`; this
directory holds only the patch payloads referenced by that manifest.

**The series is empty in this milestone.** The baseline applies zero Chromium
patches.

Each future patch:
- is referenced by exactly one `manifest.json` entry (see that file's
  `entrySchema`);
- is authored against the locked `baseRevision`
  (`6a7b3dbec3b2ca25877c2553b5473b2f277ef644`);
- is minimal and touches only the upstream paths listed in its `affectedPaths`;
- applies cleanly with `git apply --check` and reverses cleanly with
  `git apply -R --check` (verified by `../../scripts/patches.sh verify`);
- carries a `sha256` in the manifest that must match the file on disk;
- is independently reviewable on its own.

Add Chromium edits here, never as direct commits inside the external checkout.
