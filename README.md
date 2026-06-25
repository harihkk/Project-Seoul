# Project Seoul

This repository currently contains a **development harness**, not the final
Project Seoul browser. The harness is a runnable Manifest V3 Chrome extension
used to prove a browser-control runtime slice: side-panel integration,
user-gated tab access, semantic page observation, typed browser actions,
restricted action execution, persistent task state, a visible action timeline,
and deterministic tests for the protocol and task state machine.

The final product will later integrate equivalent capabilities natively into
Project Seoul's Chromium build. This extension is a temporary harness.

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

This installs exactly two development dependencies: `typescript` and
`@types/chrome`.

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
task-state-machine modules. To run type checking, build, static validation, and
tests together:

```
npm run check
```

Other scripts: `npm run validate` (static extension checks) and `npm run clean`
(remove `dist/`).

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

## Current limitations

- DOM behavior (observation, clicking, typing, scrolling) is verified manually
  against the local fixture; only the pure modules have automated tests.
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
