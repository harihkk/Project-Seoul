# Seoul product readiness

This report is the single source of truth for what exists, what was verified,
and what remains. It uses ASCII punctuation only and makes no claim of
compilation, runtime behavior, or superiority that was not actually produced.

## Verdict

SEOUL PRODUCT SOURCE INCOMPLETE

Rationale for this exact verdict: this pass added twelve product subsystems as
source, each unit-tested at the source level, and passed every available
static check. However, required source and real tests remain incomplete, which
this verdict is defined to cover:

- The three native browser-test files (`shell/shell_browsertest.cc`,
  `commands/chromium_mutation_adapter_browsertest.cc`,
  `projection/vertical_presentation_browsertest.cc`) still contain empty
  `IN_PROC_BROWSER_TEST_F` bodies and are not connected to a runnable
  `//chrome/test:browser_tests` target (prior audit items 10 and 11).
- Shell interaction areas remain unimplemented placeholders: Essential
  live-association resolver and duplicate-open prevention, native workspace
  create/rename dialogs and full menu operations, the explicit split-partner
  chooser, the searchable command launcher with full dispatch, per-window
  action/accelerator registration, tab-role decoration, and the collapsed-mode
  shell (prior audit items 3, 4, 5, 6, 7, 8, 9, 16, 17).
- The first-party Seoul Canvas WebUI is not built; the SAUI protocol, catalog,
  validation, patches, and selection exist as pure model, but no renderer or
  Mojo boundary is authored.
- Concrete provider adapters (platform/local/cloud speech, local model runtime,
  official cloud model APIs, real connectors) are interfaces plus test fakes
  only; no production adapter is implemented.

Because empty browser-test bodies and unimplemented shell placeholders remain,
the stricter verdict "SOURCE COMPLETE - BUILD BLOCKED BY HOST" cannot honestly
be used. It requires that no empty test and no placeholder action remain.

## 1. Repository state

- Branch `main`; no commits made, no history rewritten, no push, no reset. The
  working tree is uncommitted as required.
- New untracked source: 90 C++ files across 12 module directories under
  `native/seoul/browser/`, 12 `BUILD.gn` files, 19 unit-test files with 174
  `TEST`/`TEST_F` cases, plus new product/quality/release documentation.
- Modified tracked files: the pre-existing native modules from earlier passes
  (unchanged in behavior this pass except the added `import("//testing/test.gni")`
  in five BUILD files), `README.md`, `native/scripts/patches.sh` (stale comment
  corrected), and the working-tree files already present at session start.

## 2. Preserved local changes

All correct local repairs from earlier passes are preserved and revalidated:
dedicated `kOpenNewTab` via `chrome::AddAndReturnTabAt`; real lifecycle
reconciliation delegation; semantic `ShellSnapshot` equality excluding revision;
direct `WorkspaceSwitchObserver` observation; projection/shell registration after
`RootTabCollectionNode::Init()`; `ResetTabStrip` unregistration; production
`GetSeoulRootNode()` seam with no testing accessor in production; scoped
per-window shell-region host ownership; the single reversible integration patch;
user-facing status strings. None were discarded or rebuilt in another style.

## 3. Product architecture

Twelve new pure-model subsystems compose with the existing organization,
projection, command, and shell layers. Each new subsystem is `//base`-only (some
also `//url`), unit-tested, and free of Chromium UI dependencies, so the product
logic is testable without a build. See `docs/product/seoul-product-definition.md`.

## 4. Seoul Shell

Existing native Views shell (`shell/`) with the preserved repairs above. Open:
Essential live association, workspace dialogs, split chooser, searchable
launcher, action/accelerator registration, tab-role decoration, collapsed mode.
Status: partial source; native Views work requires a capable host.

## 5. Seoul Canvas

SAUI protocol, catalog, structural parser, semantic validator, typed patches,
typed events, and presentation selection are source-complete and unit-tested
(`saui/`). The first-party WebUI renderer and Mojo boundary are not built.
Status: protocol source complete; renderer not implemented. See
`docs/product/seoul-canvas-spec.md`.

## 6. Voice implementation

Voice session state machine (14 states), barge-in, push-to-talk and conversation
modes, mic policy (no always-on, no raw-audio type), local/cloud route
visibility, and spoken-reference resolution are source-complete and unit-tested
(`voice/`, 24 tests). Status: source complete; no real capture or platform
integration. See `docs/product/seoul-voice-spec.md`.

## 7. Speech providers

`SpeechToTextProvider` and `TextToSpeechProvider` interfaces plus deterministic
test fakes exist; no platform (Apple Speech), local (whisper.cpp), or cloud
adapter is implemented, and no benchmark has been run. Status: interface only.

## 8. Adaptive UI protocol

`saui/` document model, `kSauiSchemaVersion` 1, bounded parser, semantic
validator, atomic typed patches, typed events. 46 unit tests including malicious
payload rejection, forbidden handler/markup keys, non-http URL rejection, depth
and limit bounds. Status: source complete. See
`docs/product/seoul-adaptive-ui-spec.md`.

## 9. Component catalog

Trusted catalog in `saui/saui_catalog.cc` covering foundation, layout, input,
data, chart, domain (weather/markets/products/travel/calendar/research/files),
workflow, task, map, and code components, with a compile-time `static_assert`
tying the table to the `ComponentType` enum and per-type binding rules. Status:
source complete.

## 10. Weather visual result

Provider-neutral `WeatherReport` contract with provenance and validation
(`data/weather_types.h`, `data/data_validation.cc`); the `weather_current`,
`weather_hourly`, `weather_daily`, and `weather_alert` SAUI components require a
bound record/table with provenance. No weather provider adapter is implemented,
so no live weather is produced; unavailable weather is reported as unavailable by
contract. Status: contract and validation complete; provider not implemented.

## 11. Market visual result

Provider-neutral `MarketQuote`/`PriceSeries` with exact integer money, exchange,
currency, delayed-vs-live flag, and split-adjustment metadata; series chart
eligibility requires two or more strictly increasing bars with one currency and
provenance (`data/market_types.h`, `data/data_validation.cc`). No market provider
adapter. Status: contract and validation complete; provider not implemented.

## 12. Product visual result

Provider-neutral `ProductOffer`/`ProductPriceHistory` with merchant, currency,
timestamp, stock, and source URL; variants are distinct; price history exists
only when provider-supplied (never synthesized). Status: contract and validation
complete; provider not implemented.

## 13. Workflow Canvas

Versioned typed workflow graph, structural validation with cycle detection and
bounded loops, deterministic topological order, compilation onto typed plans,
typed edit operations, and import/export (`workflows/`, 16 tests). The visual
canvas rendering is via SAUI (workflow components); the interactive editor UI is
not built. Status: model and compilation complete; interactive UI not built. See
`docs/product/seoul-workflow-spec.md`.

## 14. General planner

`Plan`/`PlanStep` model and `ValidatePlan` enforce registered permitted tools,
schema-valid arguments, approval gates for risky tools, read-only parallelism,
bounded loops, and guards (`tasks/plan_types.h`, `tasks/plan_validator.cc`). The
planner produces only registered typed tool calls; it cannot invent tools or emit
executable code. The model-driven plan-construction step (turning a goal into a
candidate plan via the reasoning router) is specified but has no model adapter.
Status: plan model and validation complete; model-backed construction pending
providers.

## 15. Tool Registry

`ToolRegistry` with namespaced `ToolId`, typed schemas, permission-scoped
discovery, reserved-namespace and connector-ownership enforcement, and unknown-id
rejection (`tools/`, 9 tests). Status: source complete.

## 16. Deterministic actions

Deterministic-first routing is enforced in `intelligence/reasoning_router.cc`:
exact browser commands, known Scene operations, arithmetic, sorting, filtering,
chart rendering, workflow execution, structured formatting, and
already-selected tool calls never reach a model. Status: source complete.

## 17. Local model path

`ModelProvider` interface with capability, context-limit, cost, retention, and
locality metadata, plus a test fake; the router prefers local when it meets the
quality threshold and forbids sensitive work from the cloud. No local runtime
(llama.cpp / Core ML / MLX) adapter is implemented; no benchmark run. Status:
interface and router complete; runtime not implemented.

## 18. Cloud model path

Same `ModelProvider` interface; the router escalates to cloud only when enabled,
non-sensitive, within budget, and quality-justified. No official cloud API
adapter is implemented; BYOK secure-store handling is out of this module.
Status: interface and router complete; adapter not implemented. See
`docs/product/seoul-hybrid-intelligence-spec.md`.

## 19. Task Deck

`TaskDeckModel` with legal state transitions, active/monitoring/finished
filters, receipts with cost and route accumulation, and active-task eviction
protection (`tasks/task_deck_model.cc`). Task execution is observe-verify-decide
with checkpoints and the unknown-outcome-mutation-never-auto-retried rule
(`tasks/task_execution.cc`). 26 task-layer tests. Status: source complete. See
`docs/product/seoul-task-deck-spec.md`.

## 20. Context Threads

`ContextThread` holds only approved item kinds; forbidden classes (passwords,
cookies, tokens, raw audio, full history) are unrepresentable; `MinimizeForCloud`
drops archived threads, optionally strips bodies, and drops rather than truncates
at the byte budget (`context/`, 7 tests). Status: source complete. See
`docs/product/seoul-context-threads-spec.md`.

## 21. Scenes

`SceneRegistry` composes id-references into other subsystems, validates against
resolver callbacks, and builds a deterministic activation plan re-validated at
activation time (`scenes/`, 6 tests). Status: source complete. See
`docs/product/seoul-scenes-spec.md`.

## 22. Themes

Theme token model with WCAG relative-luminance contrast validation (AA
thresholds 4.5 normal, 3.0 large/UI) rejecting unreadable themes, plus JSON round
trip (`themes/`, 8 tests). Status: source complete. See
`docs/product/seoul-theme-system-spec.md`.

## 23. Site Layers

Declarative per-site adjustments compiled to safe scoped CSS with a validated
selector subset and no JavaScript; injection attempts rejected at validation and
import (`site_layers/`, 9 tests). Status: source complete. See
`docs/product/seoul-site-layers-spec.md`.

## 24. Essentials

Existing organization model supports the Essential record and states; the shell
live-association resolver, duplicate-open prevention, and management UI remain
open (prior audit 3, 4). Status: model present; live association not implemented.

## 25. Splits

Existing two-pane split via the command/adapter layer is preserved; the explicit
partner chooser is open (prior audit 7). Status: partial.

## 26. Preview

Not implemented in native source this pass; specified in the product plan.
Status: not implemented.

## 27. Routing

Organization routing model exists (`organization/organization_types.h` routing
rules and evaluation); Scene-level routing-rule references are modeled in
`scenes/`. Inspectable routing UI is not built. Status: model present; UI not
built.

## 28. Archive

Organization model supports archived tabs with protection checks (existing).
Status: model present; UI not built.

## 29. Connected tools

Connector seam and `ConnectorRegistry` mirror connector-owned typed tools into
the shared registry with namespace ownership enforcement and atomic
connect/rollback (`connectors/`, 6 tests). No real connector or MCP adapter is
implemented. Status: seam complete; adapters not implemented. See
`docs/product/seoul-connected-tools-spec.md`.

## 30. Tests authored

174 new `TEST`/`TEST_F` cases across 19 files, plus the existing suites. No new
test has an empty body. Coverage includes SAUI parse/validate/patch/select,
voice state machine and references, tool schema/registry, plan validation, task
execution (retry rules, unknown-outcome, budgets, approval-skip, checkpoint),
workflow graph/editor, themes contrast, site-layer safety, scenes activation,
reasoning routing, data contract validation, context threads, and connectors.

## 31. Tests runnable

Each module exposes a `test()` target importing `//testing/test.gni`, aggregated
by `//seoul/browser:seoul_unittests`. These are structured as runnable Chromium
`test()` targets. They have NOT been generated or executed on this host (no GN,
no compiler). The three native browser-test files remain empty and unconnected.

## 32. Tests actually run

None of the native C++ tests were executed on this host. The TypeScript harness
suite (`npm run check`) and repo static checks were run and pass. No claim of a
passing native test run is made.

## 33. Build state

Not built. GN generation, `gn check`, compilation, and linking were not
performed (the build-host gate refuses this 8 GiB host). Materialize apply/
verify/reverse and the patch apply/reverse round trip against the pinned checkout
were performed and are clean.

## 34. Runtime state

No runtime. The browser was not launched from a Seoul build.

## 35. Performance

No performance numbers. The performance plan (startup overhead, Canvas memory
and latency, mic-to-partial and mic-to-final latency, model load, generation
speed, interruption latency, adaptive-UI render/update, chart render, workflow
update, task overhead, Scene switch, tab-count scaling, unload, cleanup) requires
a build and is unmeasured. Lazy-initialization is a design requirement recorded
in the specs; no heavy resource is loaded at startup by construction (interfaces
and models are pure until an adapter is attached).

## 36. Competitive evidence

`docs/product/seoul-competitive-review.md` records current competitor behavior
(researched 2026-07-01) with per-claim source tags and honest uncertainty, and
states Seoul's structural differences as hypotheses to test, not measured
advantages. No superiority claim is made.

## 37. Remaining blockers

1. A capable Apple-silicon build host with full Xcode and the pinned toolchain
   (mandatory to compile, run tests, and proceed).
2. Real browser-test bodies connected to `//chrome/test:browser_tests`.
3. Native Views work for the shell (Essential resolver, workspace dialogs, split
   chooser, searchable launcher, action/accelerator registration, tab-role
   decoration, collapsed mode).
4. The first-party Canvas WebUI renderer and Mojo boundary.
5. Concrete provider adapters (speech, local model, cloud model, connectors) and
   their benchmarks.

## 38. Exact next action

On a capable host: run `native/scripts/verify-checkout.sh`, then
`materialize.sh apply`, `patches.sh apply`, `gen.sh`, `gn check
out/SeoulBaseline //seoul/...`, and `autoninja -C out/SeoulBaseline
seoul/browser:seoul_unittests`. Fix the first real compile error, iterate to a
green unit-test run of the twelve new suites, then author the three browser-test
bodies against the running build. Full procedure:
`docs/native/seoul-product-build-runbook.md`.

## Static verification performed this pass (all clean)

- clang-format (Chromium style) across all native C++: 0 non-clean.
- `git diff --check`: clean.
- Header include resolution against the pinned checkout: 90 Chromium includes
  resolved; direct-include audit passed.
- JSON parse, patch-manifest, repo-boundary (224 tracked files pre-new-source),
  shell-syntax (`bash -n`): pass.
- Placeholder/testing-accessor/fake-data/hardcoded-URL/attribution/non-ASCII
  scans over new production source: none found.
- Direct-library include audit over new source: `<map>`/`<vector>` added where
  used directly. base::Value/Dict/List are move-only in the pinned checkout
  (copy constructors deleted; `Clone()` provided), so the SAUI, workflow, and
  task structs that hold them by value declare explicit clone-based copy
  semantics; the aggregates above them (AdaptiveSurface, WorkflowDefinition,
  Plan) are then copyable, which the patch working-copy and edit-then-validate
  paths require. This was verified by inspection against `base/values.h`, not by
  compilation.
- Materialize apply/verify/reverse: clean. Patch `git apply --check`/apply/
  reverse against the pinned checkout: clean, zero tracked edits after reverse.
- Checkout restored: `src` HEAD at the lock
  `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`, depot_tools at the lock, no
  `src/seoul` overlay, no applied patch; `verify-checkout.sh` PASS.
