# Chromium patches

**This baseline contains zero Chromium patches.** The pinned revision in
`../chromium.lock.json` is fetched, synced, generated and built completely
unmodified.

When a Chromium modification eventually becomes unavoidable, it must be:

- **Minimal** - the smallest change that achieves the goal; never a broad refactor.
- **Documented** - each patch file is accompanied by a note stating what it changes,
  why it is necessary, and what upstream alternative was rejected.
- **Independently reversible** - each patch applies and reverts cleanly on its own,
  against the exact locked revision, without depending on other patches.

Patches are an overlay over a pinned upstream tree. They are never a substitute for
extending the existing upstream vertical-tab and split-tab implementations, which
must be exhausted first. See `../README.md` for the architecture boundary.
