# Seoul Canvas Design Lab

The Canvas **Design Lab** is a runnable TypeScript environment for the Seoul
Canvas's visual and interaction design, built to run on an ordinary machine
(it does not need a Chromium build). It exercises the design loop over the
**canonical cross-language protocol** (`protocol/`):

```
goal -> lexical fixture router -> fixture capability (canned canonical result)
     -> fixture contract validated -> Lab interface compiler
     -> canonical adaptive surface -> artifact
     -> every change (representation switch, stream batch) = canonical
        surface patch, applied atomically, reconciled per component
```

Build and open it with `npm run canvas`; test it with `npm run test:canvas`
(type-check, logic tests, patch-engine tests, and a headless-browser smoke
test - all part of `npm run ci`).

## What the Design Lab is NOT

This is the design lab, **not the shipping runtime**. The shipping Seoul is
the native Chromium product under `native/seoul/`; its Canvas is a WebUI fed
by the real task runtime. Boundaries, stated plainly:

| Design Lab | Shipping runtime (`native/seoul/`) |
| --- | --- |
| Twenty **fixed synthetic fixture capabilities**, registered from `protocol/fixtures/catalog.json`. Running one returns a canned payload. | A dynamic Capability Graph (`tools/`): registered, permission-scoped, health-tracked capabilities from the browser runtime, connectors, and models. |
| A **lexical token-overlap router** over descriptor text selects the fixture. | A planner that builds validated multi-step plans with approval gates, budgets, guards, and replanning (`product/planner.*`). |
| "Verification" means **the canned payload validates against the canonical semantic-result schema**. Receipts record method `fixture_contract` with detail `fixture contract validated`. **No browser, provider, or network state is observed.** | Real execution with an observe-verify step: receipts record postconditions and observations of actual browser state (`tasks/task_types.h`, executors in `product/browser/`). |
| The **Lab interface compiler** (`src/compiler.ts`) is a separate, simplified implementation of the shape-and-role selection policy. Shapes the Lab cannot render well (geo, media) fall back to an **explained** table with a named reason. | The production compiler is `saui/interface_compiler.cc`, with the full component catalog (maps, media, candlestick, heat map, ...) and chart-honesty validation. |
| Renders in a plain page with hand-rolled DOM renderers. | Renders in the Seoul side-panel Canvas WebUI inside Chromium. |

What the two sides **share** - and may never fork - is the protocol:
`protocol/*.schema.json` defines the semantic result, adaptive surface,
surface patch, component event, task snapshot, and capability descriptor wire
formats. The Lab consumes generated types (`protocol/ts/types.ts`) and
validates every document against the actual schema files at runtime; the
native side implements the same contract in C++ with conformance tests over
the same fixtures (`protocol/fixtures/`). `npm run check:protocol` fails CI if
schema enums and native wire names ever drift. There is no independently
authored Design Lab protocol model.

## What IS real here

- **Real incremental surface patches.** Every change flows through
  `src/surface-store.ts`, which applies canonical patch documents with the
  native engine's semantics: schema-validated at the door, atomic
  (all-or-nothing, invalid targets and dangling bindings roll back), revision
  bumped once, and a precise changed-components/changed-entries summary. The
  DOM reconciler re-renders only the components the summary names: the
  artifact element, its margin receipt, and unaffected siblings keep their DOM
  identity, and focus, scroll, and text selection survive (proved by
  `tests/render.smoke.mjs` in a real browser). A representation switch is a
  `replace_component` patch on the stable root component - it does not replace
  the artifact element.
- **Real contract validation.** Fixture descriptors and payloads are validated
  against the canonical schemas at registration and on every run; drift fails
  loudly.
- **The generalization property.** The compiler decides from shape and roles
  only; held-out schemas render without being special-cased anywhere
  (`tests/generalization.test.mjs`).

## Honest wording rules

Fixture results are labeled **synthetic demo data** in the catalog, the
masthead, and every artifact margin. Receipts say `fixture contract validated`
- never "verified" - because no real postcondition was observed. The one
fixture that reports a structured error renders a failed task and an error
artifact, honestly.

## Layout

`src/` is split by responsibility: `state.ts` (application state),
`runtime.ts` (task fixture runtime), `surface-store.ts` (surface store +
patch engine), `renderers.ts` (component renderers), `charts.ts` (SVG chart
layer), `representation.ts` (representation controls), `provenance-ui.ts`
(provenance/conflict/gap UI), `receipts.ts` (task receipts), `catalog.ts`
(fixture capability catalog UI), `compiler.ts` (Lab interface compiler),
`fixtures.ts` (shared-corpus imports), `protocol.ts` (canonical contract
access), and `canvas.ts` (composition + patch reconciler). The worklog
typography (numbered entries, margin receipts, hairline rules) is the design
under study and is shared visual language with the future shipping Canvas.
