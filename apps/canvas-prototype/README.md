# Seoul Canvas prototype

A runnable prototype of Project Seoul's differentiating loop, built to run on an
ordinary machine (it does not need a Chromium build). It exercises the real
design end to end:

```
goal -> deterministic plan -> capability (returns an unseen semantic schema)
     -> observe + verify -> adaptive interface compiler -> Canvas artifact
     -> switch representation (patched in place)
```

Nothing here is domain-specific. A capability returns a `SemanticResult`
described only by a shape (scalar, record, entity collection, time series,
hierarchy, citations, ...) and per-field roles (identifier, measure, category,
timestamp, ...). The adaptive interface compiler chooses the presentation -
metric, card, table, bar/line/area/scatter chart, tree, source list, timeline -
from the shape and roles alone, so a schema it has never seen still renders and
an incompatible "show as chart" request falls back to a table instead of
misleading. This mirrors the native modules under
`native/seoul/browser/semantic`, `.../saui/interface_compiler.cc`, and
`.../product`.

## Run it

From the repository root:

```
npm run canvas
```

That bundles the app to a single self-contained `dist/index.html` (no external
requests - CSP-safe, works from `file://`; the two OFL typefaces in `fonts/`
are embedded as base64) and opens it. Or open
`apps/canvas-prototype/dist/index.html` in any browser after
`npm run build:canvas`.

The Canvas is a worklog document, not a dashboard: each run appends a numbered
entry whose receipt (capability id, freshness, verification) is typeset as
marginalia in the left margin column, and the result is set directly on the
page. Click a row in the index of capabilities, or state a goal in the command
bar at the bottom (e.g. "show the pipeline latency timeline"). On an entry,
use the Table / Bar / Line text links - they re-compile the **same** artifact
in place, they do not create a duplicate.

## Test it

```
npm run test:canvas
```

Runs the strict TypeScript typecheck plus:

- `tests/generalization.test.mjs` - the adaptive compiler against held-out
  schemas (representation chosen from shape/roles, chart-would-mislead fallback,
  compatible-request honoring, deterministic planner routing).
- `tests/render.smoke.mjs` - loads the built HTML in a headless browser and
  drives the full loop (chart renders, representation switch patches in place,
  a record renders as a card, a hierarchy as a tree, zero console errors).

## What this is and is not

This is a faithful, runnable demonstration of the product's core loop so the
design can be evaluated on a normal machine. It is a separate artifact from the
native C++ browser under `native/seoul`; it is not the shipping Chromium build.
The presentation rules and the generic pipeline are the same idea, implemented
in TypeScript for local runnability.

## Layout

- `src/semantic.ts` - the domain-neutral semantic model (shapes, roles,
  provenance).
- `src/compiler.ts` - the adaptive interface compiler (shape + roles ->
  components; no domain conditionals).
- `src/runtime.ts` - the capability registry, a set of generic capabilities
  returning unseen schemas, and the deterministic lexical planner + task runner.
- `src/charts.ts` - a small local SVG chart layer (axes, gridlines, legend,
  hover crosshair; no remote assets).
- `src/canvas.ts` - the surface renderer and the Canvas application.
- `src/styles.css` - the Seoul design system (tokens, scale, dark/light).
- `fonts/` - Space Grotesk and Geist Mono (OFL), embedded at build time.
- `build.mjs` - esbuild bundle to one self-contained HTML.
- `shoot.mjs` - captures reviewer screenshots of a populated worklog.

## Font licenses

The two typefaces in `fonts/` are redistributed under the SIL Open Font
License 1.1: Space Grotesk (copyright 2020 The Space Grotesk Project Authors,
https://github.com/floriankarsten/space-grotesk) and Geist Mono (copyright
2023 Vercel, https://github.com/vercel/geist-font).
