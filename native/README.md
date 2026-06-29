# Seoul native Chromium baseline

This directory bootstraps a **reproducible, unmodified native Chromium baseline**
for Project Seoul on macOS arm64. It contains scripts, a pinned-revision lock, the
baseline GN args, and documentation. It contains **no Chromium source and no build
output**.

## Architecture boundary

- **Chromium source and build output live OUTSIDE this repository.** They are
  checked out into an external root (`$SEOUL_CHROMIUM_ROOT`, default: a sibling
  directory `../seoul-chromium`). Nothing under that root is tracked by Project
  Seoul Git.
- **`chromium.lock.json` pins the exact upstream source** - product version, full
  Chromium commit SHA, and the depot_tools commit SHA. The build is reproduced
  from this pin, never from a floating `main`.
- **`scripts/` reproduces the checkout and build** non-destructively: `doctor.sh`
  (preflight), `fetch.sh` (depot_tools + initial checkout), `sync.sh` (pin + deps),
  `gen.sh` (GN), `build.sh` (the `chrome` target only, `-j 2`), `run.sh` (launch
  with a disposable profile), `smoke.mjs` (launch verification via the repo's
  pinned Puppeteer).
- **`patches/` is currently empty.** The baseline applies zero Chromium patches.
- **Future Seoul-owned code will live in a separately mounted `seoul-core`
  directory**, overlaid onto the Chromium tree - not committed into this repo and
  not created in this milestone.
- **Future unavoidable Chromium modifications will be isolated as small, reviewable,
  independently reversible patches** under `patches/`, documented one-by-one. This
  is the overlay-and-patch principle only; no Brave source or naming is copied.
- **Upstream vertical tabs and split tabs must be EXTENDED before any replacement is
  considered.** The pinned revision already ships both (see
  `../docs/native/chromium-baseline.md`); Seoul builds on them rather than
  reimplementing them.
- **The Manifest V3 extension harness (`apps/browser-harness/`) remains frozen** as
  a browser-control protocol reference. It receives only critical correctness
  fixes and no new features.

## Configuration

- `SEOUL_CHROMIUM_ROOT` - absolute path to the external checkout root. When unset,
  a sibling directory `seoul-chromium` next to this repo is used. Paths containing
  spaces (or that resolve inside this repo) are rejected.
- Build parallelism is capped at `-j 2`. This is a cautious **local** resource
  setting for a memory-constrained machine, not an upstream Chromium requirement.

## Status / prerequisites

`scripts/doctor.sh` reports whether this host can build. As recorded in
`../docs/native/chromium-baseline.md`, the build is currently **gated by missing
prerequisites** (full Xcode, sufficient RAM). The scripts and the resolved pin are
in place so the build can proceed unchanged once those are satisfied.

## Reproduce

```
native/scripts/doctor.sh        # preflight; must pass before fetching
native/scripts/fetch.sh         # clone depot_tools + initial checkout (external)
native/scripts/sync.sh          # pin to chromium.lock.json + sync deps
native/scripts/gen.sh           # generate out/SeoulBaseline
native/scripts/build.sh         # autoninja -C out/SeoulBaseline -j 2 chrome
native/scripts/run.sh           # launch with a disposable profile
node native/scripts/smoke.mjs   # launch smoke test
```
