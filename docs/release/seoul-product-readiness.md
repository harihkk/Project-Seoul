# Seoul product readiness

Last verified: 2026-07-23 on macOS arm64.

This report is the source of truth for the current product build. It separates
working product behavior from public-distribution work. A passing development
build is not described as a signed release.

## Verdict

SEOUL FUNCTIONAL DEVELOPMENT BUILD VERIFIED

The tracked Seoul source was materialized into the pinned Chromium checkout,
the reversible integration patches were applied, Chromium and every Seoul
native test target compiled, the local browser launched, and the shipping
`chrome://seoul-canvas` WebUI ran successfully.

The current build is not yet a public release artifact. It is a component
development build without final Seoul application branding, signing,
notarization, an installer, or production update infrastructure.

## Verified build

| Item | Verified value |
|---|---|
| Chromium version | `149.0.7827.201` |
| Chromium revision | `6a7b3dbec3b2ca25877c2553b5473b2f277ef644` |
| Platform | macOS arm64 |
| Output | `out/SeoulBaseline/Chromium.app` |
| Build mode | release component build, `symbol_level=0` |
| Seoul overlay | `native/seoul/` materialized to `src/seoul/` |
| Integration | 2 ordered, hash-verified patches |
| First-party Canvas | `chrome://seoul-canvas` |

The build host passed the RAM, storage, Xcode, SDK, architecture, and checkout
gates before generation or compilation. The build did not bypass or weaken a
host gate.

## Test evidence

All results below were produced locally from the source and binary described
above.

| Suite | Result |
|---|---|
| Native unit executables | 24 passed |
| Native unit tests | 514 passed |
| Focused Chromium browser tests | 20 passed |
| Protocol conformance | 8 passed |
| Canvas and renderer tests | 30 passed |
| Native syntax audit | 173 parsed, 36 generated-header files deferred to the native compiler |
| TypeScript and JSON checks | passed |
| Patch manifest, apply, and reverse verification | passed |
| GN header dependency check | passed |
| Architecture, boundary, and domain-neutrality gates | passed |

The 36 syntax-audit skips are not uncompiled gaps. They depend on GN-generated
Chromium headers and were compiled through their native build targets. The
focused browser suite also exercised their runtime integration.

### Focused browser coverage

The 20 passing in-process browser cases cover:

- regular-profile runtime and service wiring;
- the invariant that every available capability has an executor;
- text goals producing bound native tasks;
- sensitive field redaction and model-write refusal;
- exact per-window bindings;
- ephemeral preview lifecycle outside the tab strip;
- first-party Canvas registration, Lit rendering, and starter-command focus;
- real tab open, activate, pin, and move mutations plus unknown-window refusal;
- live vertical projection;
- default-on vertical shell behavior;
- continued Chromium ownership of the tab strip;
- reachable Seoul organization state.

## Product smoke

The isolated smoke test launched only the explicit local Seoul binary with a
new disposable profile. It did not discover or launch an installed browser.

Observed on 2026-07-23:

| Check | Result |
|---|---|
| Isolated launch | 1248 ms |
| Local navigation | 507 ms |
| Total smoke | 3067 ms |
| JavaScript and DOM | passed |
| Second tab open and activation | passed |
| Canvas product heading | passed |
| Four Canvas views | passed |
| Voice default-off | passed |
| Empty-send refusal | passed |
| Canvas console errors | 0 |
| Browser disconnects | 0 |
| Page crashes | 0 |

These are development-host smoke measurements, not broad performance
benchmarks or service-level guarantees.

## Shipping Canvas state

The browser packages and serves a first-party Lit WebUI from
`chrome://seoul-canvas`. It is connected to the native runtime through Mojo;
it is not the standalone design lab.

Verified behavior:

- a distinctive first-run command surface;
- the Observe, Plan, Approve, Verify control model;
- real starter commands that populate and focus the native composer;
- Canvas, Library, Boards, and Studio views;
- task state, approval, input, pause, resume, reject, and cancel controls;
- validated SAUI component rendering with escaped payloads;
- bounded chart and table rendering;
- voice explicit and off by default;
- no remote script, inline script, or eval permission in the WebUI CSP;
- zero console errors in both preview capture and product smoke.

The standalone `apps/canvas-prototype/` remains a design lab. It is tested but
is not used as evidence that the shipping Canvas works.

## Native product state

### Working and verified

- profile-scoped runtime composition;
- domain-neutral capability registry and planner;
- executor-backed browser tab and page capabilities;
- exact window and tab identity;
- typed tasks, approvals, input replanning, receipts, and task-to-surface
  projection;
- browser mutation confirmation and postcondition observation;
- ephemeral previews that do not silently become retained tabs;
- vertical workspace projection and default-on vertical product shell;
- Library and Boards persistence paths;
- read-only Studio inventory;
- adaptive SAUI compilation and stable patches;
- semantic data, provenance, and chart-honesty validation;
- exact-scope agent permissions and sensitive-field refusal;
- workflow, scene, theme, Site Layer, connector, context, and voice core models;
- local and cloud provider routing with injected transports;
- realtime voice session plumbing and tool-call bridge.

### Deliberately bounded

- Studio exposes a read-only runtime inventory; it is not a complete editor.
- Live microphone, external model, and connected-tool behavior requires user
  credentials, real endpoints, permissions, and hardware. The code paths are
  compiled and deterministically tested, but this report does not claim a
  successful production-account session.
- Scene, Theme, Site Layer, and workflow core models are present and tested.
  Their full end-user creation and editing surfaces are not release-complete.

## Reproducible commands

From the Project Seoul repository:

```sh
npm run ci
npm run test:native
npm run test:native:browser
npm run preview:native
node native/scripts/smoke.mjs
```

The native commands default to the pinned sibling checkout. Set
`SEOUL_CHROMIUM_ROOT` or `SEOUL_CHROMIUM_BINARY` only when deliberately using
another Seoul checkout or binary. The preview and smoke tools never fall back
to installed Google Chrome.

Checkout and patch reproducibility:

```sh
native/scripts/verify-checkout.sh
native/scripts/patches.sh verify
native/scripts/patches.sh apply
native/scripts/materialize.sh apply
native/scripts/materialize.sh verify
```

## Public release gates

The following work remains before Seoul can be called a distributable release:

1. Build and repeat the full test matrix as a non-component release build.
2. Replace Chromium application identity with approved Seoul product name,
   bundle identifier, icons, legal assets, and update configuration.
3. Produce the release package and test clean install, upgrade, rollback, and
   profile migration.
4. Sign with the intended Apple Developer identity and notarize the package.
5. Run real credentialed model, realtime voice, and connected-tool acceptance
   tests without recording secrets.
6. Complete keyboard, screen-reader, contrast, reduced-motion, multi-window,
   and long-session soak passes on supported hardware.
7. Complete privacy, security, dependency, network-endpoint, and release-policy
   review.

Those gates require release identities, credentials, external services, and
product decisions that are not stored in this repository. They must not be
simulated or marked complete by a development build.

## Current handoff

The functional development build is reproducible and green. Continue from this
state; do not restart from the standalone prototype, do not weaken the patch or
build gates, and do not use an installed browser as a Seoul test substitute.
