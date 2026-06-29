# Project Seoul

Project Seoul is becoming a real, premium Chromium-based browser. This repository
is currently a **foundation**, not a completed browser. It contains three things:

1. A **frozen Manifest V3 browser-control harness** (`apps/browser-harness/`), a
   protocol and safety reference only (described in the rest of this README).
2. An **unmodified, pinned native Chromium baseline** (`native/`): the lock,
   baseline GN args, and reproduction/verification scripts. The Chromium source and
   build output live in an external, untracked checkout, never in this repo.
3. The **tracked Seoul-owned native source scaffold** (`native/seoul/`) and an
   ordered, reversible **Chromium integration patch series** (`native/patches/`),
   currently empty of product code by design.

Source-of-truth model: Seoul-owned code is tracked here (`native/seoul/`) and is
materialized into the external checkout; unavoidable upstream edits are minimal,
reversible patches (`native/patches/chromium/`) over a pinned Chromium revision.
The modified checkout is disposable and is never the source of truth.

**Not yet done / not verified on any machine:** GN generation, Chromium
compilation, launch, smoke test, runtime vertical-tabs and split-view validation,
packaging, signing, and notarization. The component-build development settings in
`native/gn/` are for local iteration and are **not** the shipping configuration.

See: `docs/native/chromium-baseline.md` (baseline + checkout state),
`docs/native/remote-build-runbook.md` (how to build on a capable Mac),
`docs/product/feature-matrix.md` and `docs/product/native-architecture.md`
(product direction and subsystem design). The harness integrates a browser-control
runtime slice (side panel, user-gated tab access, semantic page observation, typed
actions, restricted execution, persisted control-session state, action timeline)
and exists only as a temporary reference; the native product re-implements these
capabilities properly.

## Prerequisites (verified on this machine)

- macOS on Apple Silicon (`arm64`)
- Node.js `v26.0.0` (requires Node ≥ 23.6 for built-in TypeScript type stripping)
- npm `11.12.1`
- Google Chrome `149` (requires Chrome >= 116 for `chrome.sidePanel.open`)
- Python `3.14.6` (only to serve the local fixture)

## Install

```
npm install
```

This installs the development dependencies pinned in `package-lock.json`:
`typescript`, `@types/chrome`, and `puppeteer` (Puppeteer drives the optional
real-Chrome integration suite; nothing is installed at runtime).

## Build

```
npm run build
```

The build compiles TypeScript with `tsc` (no bundler) and writes a loadable
unpacked extension to `dist/browser-harness`.

## Test

```
npm test
```

Runs the Node built-in test runner against the pure protocol, validation, and
control-session-state-machine modules. To run type checking, build, static
validation, and tests together:

```
npm run check
```

Other scripts: `npm run validate` (static extension checks) and `npm run clean`
(remove `dist/`).

### Real-Chrome integration tests

A separate suite drives the unchanged, built extension in real Chrome for Testing
through Puppeteer, using only genuine action invocations (no test hooks, no extra
permissions, no permanent content script, no direct background-handler calls):

```
npm run itest             # required stable gate, headless
npm run itest:headful     # the same stable gate, visibly
npm run itest:resilience  # optional forced-worker-loss resilience tests
```

The required stable gate covers, end to end through the genuine toolbar action and
real `activeTab`: real action access and side-panel attachment; exact two-tab
isolation and grant revocation on navigation; the full panel lifecycle including
the sensitive-field refusal; navigation invalidation; and shipped-extension
security assertions.

The optional resilience suite injects a **forced** service-worker termination (CDP
target close) to prove storage persistence, control-session reconstruction, and no
action replay (`ACTION_OUTCOME_UNKNOWN`). Forced termination is not natural MV3
idle shutdown and is not extension reload. Extension-level `chrome.runtime.reload()`
is intentionally not part of any automated test, because the tested Puppeteer
25.2.1 + Chrome for Testing 150 combination did not reliably reattach the unpacked
extension after reload.

Each run starts its own Node fixture server on an available port, installs the
built extension, discovers its id dynamically, and tears down the browser, server,
and temporary profile afterward. Failure artifacts are written only on failure,
under the gitignored `apps/browser-harness/itest/artifacts/`. None of these suites
are part of `npm run check`.

## Load the extension in Chrome

1. Open `chrome://extensions`.
2. Enable **Developer mode**.
3. Choose **Load unpacked**.
4. Select the `dist/browser-harness` directory.

## Serve the local fixture for manual testing

```
python3 -m http.server 8765 --directory fixtures
```

Then open the manual test URL:

```
http://localhost:8765/interactive-page.html
```

Open the side panel from the extension's toolbar action, start a session,
inspect the page, and exercise the click / type / scroll / navigate actions.

## Status

This is a **frozen, temporary browser-control protocol harness**. It is not the
final Seoul UI and it is not Seoul's user-task architecture; it implements only
low-level browser-control sessions. The pure modules are unit-tested, the stable
real-Chrome gate covers action access, tab isolation, panel lifecycle, and
navigation invalidation, and the optional resilience suite covers forced
service-worker loss (which is not natural idle and not extension reload). After
this milestone the harness receives only critical correctness fixes; the next
milestone is the reproducible native Chromium baseline.

## Current limitations

- DOM behavior (observation, clicking, typing, scrolling) is covered by the
  real-Chrome integration suite and by the pure-module unit tests.
- A session is bound to a single active tab and ends after navigation; a new
  session must be started on the new page.
- Typing into a field that already contains a value is rejected, because no
  clear action exists in this milestone.
- No custom toolbar icon is shipped; Chrome supplies a default action icon.
- The harness uses only `activeTab`, `scripting`, `sidePanel`, and `storage`,
  and declares no host permissions; page access begins only after an explicit
  user gesture.

## No model provider yet

This milestone does **not** use any model provider, API key, subscription
authentication, or native messaging. No model-generated code is executed.
