# Seoul native Chromium baseline

This directory holds Project Seoul's native browser foundation on macOS arm64: the
pinned-revision lock, baseline GN args, repository-owned Seoul source, the Chromium
integration patch series, and deterministic scripts. It contains **no Chromium
source and no build output**.

## Architecture boundary

- **Chromium source and build output live OUTSIDE this repository.** They are
  checked out into an external root (`$SEOUL_CHROMIUM_ROOT`, default sibling
  `../seoul-chromium.noindex`; the `.noindex` suffix keeps Spotlight from
  indexing the checkout). Nothing under that root is tracked by Project Seoul
  git, and it is never the source of truth.
- **`chromium.lock.json` pins the exact upstream source** - product version, full
  Chromium commit SHA, and the depot_tools commit SHA. Reproduced from this pin,
  never from a floating `main`.
- **Seoul-owned native code is tracked here**, under `seoul/` (C++, WebUI,
  resources, config). `scripts/materialize.sh` mirrors it into the checkout at
  `src/seoul/`. It is the source of truth; the checkout copy is disposable.
- **Unavoidable upstream Chromium edits are minimal, ordered, reversible patches**
  under `patches/chromium/`, described by `patches/manifest.json` and applied by
  `scripts/patches.sh`. The series is currently empty.
- **Upstream vertical tabs and split view must be EXTENDED before any replacement
  is considered.** The pinned revision already ships both (see
  `../docs/native/chromium-baseline.md`); Seoul builds on them.
- **The canonical `protocol/` directory is the browser-control protocol
  reference.** The retired Manifest V3 extension harness has been removed.

## Scripts

- `doctor.sh` - checkout-readiness preflight (does not gate building).
- `fetch.sh` - clone+pin depot_tools and perform the initial checkout (external).
- `sync.sh` - pin to the lock, sync deps, run hooks. `sync.sh --verify-only`
  delegates to `verify-checkout.sh`.
- `verify-checkout.sh` - read-only verification of the checkout against the lock.
- `materialize.sh` - apply/verify/reverse the Seoul source overlay.
- `patches.sh` - list/verify/apply/reverse the Chromium integration patch series.
- `check-patch-manifest.mjs` - validate `patches/manifest.json`.
- `build-host-check.sh` - **build-readiness gate** (hard RAM/disk/Xcode/checkout
  requirements). `gen.sh` and `build.sh` refuse to run unless it passes.
- `gen.sh` - GN generation (gated by `build-host-check.sh`). `gen.sh --verify`
  prints the effective args without a checkout.
- `build.sh` - build the `chrome` target (gated). Job count via `SEOUL_NINJA_JOBS`.
- `run.sh` / `smoke.mjs` - launch with a disposable profile / launch smoke test.

## Configuration

- `SEOUL_CHROMIUM_ROOT` - absolute path to the external checkout root (default
  sibling `seoul-chromium.noindex`, with a legacy `seoul-chromium` sibling still
  honored). Paths with spaces, or that resolve inside this repo, are rejected.
- `SEOUL_NINJA_JOBS` - explicit Ninja job count (validated positive integer). When
  unset, a conservative memory-aware default is used (about one job per 4 GiB of
  RAM, floored at 2). There is no unconditional hard-coded value.
- `SEOUL_MIN_RAM_GIB` (default 16) and `SEOUL_MIN_BUILD_FREE_GIB` (default 150) -
  the build-host minimums enforced by `build-host-check.sh`.

## Status

The lock is resolved and the external checkout is complete and `gclient validate`
clean (see `../docs/native/chromium-baseline.md`). Full Xcode is present on the
current machine. **The build is intentionally deferred to a stronger host**: this
machine has 8 GiB RAM (below the 16 GiB build minimum) and limited free disk, so
`build-host-check.sh` fails here and `gen.sh`/`build.sh` refuse to run.

GN generation, compilation, launch, smoke testing, and runtime vertical/split
validation have **not** been performed on any machine. The component-build args in
`gn/macos-arm64-baseline.gn` are development settings, not the shipping
configuration.

## Reproduce (on a capable host)

```
native/scripts/doctor.sh            # checkout readiness
native/scripts/fetch.sh             # clone depot_tools + initial checkout (external)
native/scripts/sync.sh              # pin to the lock + sync deps + hooks
native/scripts/verify-checkout.sh   # read-only verification against the lock
native/scripts/materialize.sh apply # overlay Seoul source (empty product code now)
native/scripts/patches.sh verify    # validate + apply-check the patch series (empty)
native/scripts/build-host-check.sh  # BUILD gate (RAM/disk/Xcode/checkout)
native/scripts/gen.sh               # gn gen out/SeoulBaseline
native/scripts/build.sh             # build chrome (SEOUL_NINJA_JOBS configurable)
node native/scripts/smoke.mjs       # launch smoke test
```

See `../docs/native/remote-build-runbook.md` for the full remote build runbook.
