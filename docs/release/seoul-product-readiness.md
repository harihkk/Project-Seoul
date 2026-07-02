# Seoul product readiness

This report is the single source of truth for what exists, what was verified,
and what remains. It uses ASCII punctuation only and makes no claim of
compilation, runtime behavior, or superiority that was not actually produced.
Where a subsystem is source-only, it says so; where a step needs a build host,
it says so.

## Verdict

SEOUL PRODUCT SOURCE INCOMPLETE

This verdict is narrower than the prior pass. The native browser-test blocker
is now cleared, the product runtime and Canvas WebUI are source-connected into
the integration patch, and Canvas turns no longer infer their target window
from active or last-focused browser state. The product is still not complete:
the first-party Canvas side-panel entry is not registered, no native target has
compiled or run on this host, and shell interaction polish remains. The stricter
verdict "SOURCE COMPLETE - BUILD BLOCKED BY HOST" is defined to require that no
empty test and no placeholder integration remain; the items below are real, so
it cannot honestly be used.

Cleared since the prior pass:

- The four native browser-test files now contain real
  `IN_PROC_BROWSER_TEST_F` bodies (16 cases, zero empty) that assert
  load-bearing integration invariants against a real `Browser`, `Profile`, and
  `TabStripModel`, and they are connected to `//chrome/test:browser_tests`
  through the single integration patch (a new `chrome/test/BUILD.gn` hunk). The
  patch applies and reverses cleanly against the pinned checkout.
- Domain hardcoding was removed from core. The weather/market/product/travel
  contracts and the domain component catalog are gone, replaced by a
  domain-neutral semantic data fabric, an adaptive interface compiler, and a
  capability graph. A CI gate (`check:neutrality`) enforces domain neutrality.
- Canvas ownership no longer uses an active-window fallback. `SeoulRuntimeService`
  now creates opaque per-window binding tokens from Chromium's WebUI embedding
  context, Canvas turns resolve the token before task execution, stale/unbound
  Canvas documents fail closed, and `check:product-arch` rejects any future
  `ActiveWindow()` shortcut in product or Canvas source.

Correctness review pass (adversarial self-review of the product layer):

Seven real defects were found by reading the new source against the pinned
checkout (two by an independent review of each subsystem) and fixed; each has a
regression guard where one is expressible:

- HIGH: the task drive loop (`TaskService::Pump`) re-entered itself when an
  executor completed synchronously, firing `OnTaskFinished` N+1 times for an
  N-step plan and growing the stack per step. Fixed with a re-entrancy guard
  plus an idempotent finish; covered by a synchronous multi-step test.
- HIGH: the page agent bumped its per-tab observation generation synchronously
  but the accessibility snapshot is async, so an interleaved observe/navigation
  could register a superseded tree's element handles under the current
  generation - defeating the "expired handle" guarantee. Fixed by capturing the
  expected generation per request and dropping a stale snapshot.
- HIGH ("byhearting"): a declared Canvas `tool_call` action round-tripped its
  capability id through the lexical planner as goal text, dropping the declared
  payload and mis-planning under a model. Fixed with a direct
  `StartCapability(id, args, window)` that runs exactly that capability with its
  declared arguments through the validated plan path.
- HIGH: `browser.tabs.activate`/`close` read arg `tab`, but the descriptors
  declare `tab_key`, so both were unrunnable; the `page.act.*` descriptors used
  a free-text `target` selector no executor implemented. Fixed by aligning the
  descriptors to the handle-based executor model, and a new `check:product-arch`
  rule fails CI if any executor reads an arg no descriptor declares.
- MEDIUM: the planner sent `response_schema = {"type":"plan"}`, which a strict
  structured-output endpoint would reject as invalid JSON Schema. Replaced with
  a real JSON Schema describing the plan shape.
- MEDIUM: the surface event resolver let the renderer's value override the
  server-declared action payload. Fixed so declared arguments are authoritative.
- LOW: surface create/restore notified observers before inserting the surface;
  the page-agent URL was malformed (double slash); a missing `net_errors.h`
  include. All fixed.

Why the verdict is still INCOMPLETE:

- The `SeoulRuntime` composition is source-connected through a profile-keyed
  `SeoulRuntimeService`, with `SceneResolvers`, concrete HTTP transports,
  Keychain credential store, page agent, providers, planner, task/surface/
  thread/workflow services, and builtin executors. It is not compiled or
  runtime-verified, so reachability is source evidence only.
- The Seoul Canvas WebUI (controller, `WebUIConfig`, Mojo `PageHandlerFactory`,
  page handler, packaged resources) is source-connected and registered at
  `chrome://seoul-canvas`, and the page handler routes bound turns into the
  runtime. The remaining integration gap is the actual window-scoped Seoul
  side-panel entry; tab-loaded Canvas documents intentionally fail closed as
  unbound.
- Provider adapters (local model, cloud model, Apple speech-to-text and
  text-to-speech) are authored with injected transports and are unit-tested
  through deterministic fakes. Concrete loopback/cloud HTTP transport and
  Keychain credential storage source exists, but real endpoint, credential,
  and audio behavior is not runtime-verified.
- Shell interaction placeholders remain: the Essential live-association resolver
  and duplicate-open prevention, the explicit split-partner chooser, the
  searchable command launcher with full dispatch, per-window action/accelerator
  registration, tab-role decoration, and the collapsed-mode shell. (Workspace
  create/rename now use a real `ui::DialogModel` text-input dialog instead of a
  fixed name; `check:product-arch` fails CI on a hardcoded workspace name.)
- Scene reference validation for themes and site layers is a V1 stub
  (`MakeSceneResolvers` accepts any non-empty id) because those catalogs are not
  wired yet; Scenes are not user-reachable, so this path is inert, but it is not
  real validation and is listed here rather than hidden.
- Nothing was built or run on this host: no GN generation, no `gn check`, no
  compilation, no linking, no browser launch (the build-host gate refuses this
  host).

## 1. Repository state

- Branch `main`; no commits, no history rewrite, no push, no reset, no
  discarded local changes. The working tree is uncommitted as required.
- 341 tracked files. Source spans 21 module directories under
  `native/seoul/browser/`: `canvas`, `commands`, `connectors`, `context`,
  `data`, `intelligence`, `lifecycle`, `organization`, `projection`, `runtime`,
  `saui`, `scenes`, `semantic`, `shell`, `site_layers`, `tasks`, `themes`,
  `tools`, `voice` (with `voice/platform` Apple adapters), and `workflows`.
- The single integration patch now touches five upstream files:
  `chrome/browser/BUILD.gn`,
  `chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.cc`,
  `chrome/browser/ui/views/frame/vertical_tab_strip_region_view.cc` and `.h`,
  and `chrome/test/BUILD.gn` (the browser-test wiring added this pass). The
  manifest `affectedPaths`, `sha256`, and description were updated to match.

## 2. Architecture correction (domain neutrality)

The core is intent-native and domain-neutral. Removed: the weather, market, and
product data contracts and the domain component catalog entries. Added:

- Semantic data fabric (`semantic/`): shapes, roles, field specs, provenance,
  streaming-row merge, and inspection predicates (chart-would-mislead,
  comparable-entities, temporal-axis, geo/ohlc eligibility) that handle unseen
  schemas by shape and role rather than by name.
- Adaptive interface compiler (`saui/interface_compiler.*`): selects components
  from shape and semantics with no domain conditionals, with request-compat and
  chart-provenance guards and fallbacks; validated by held-out unseen-schema
  tests.
- Capability graph (`tools/`): typed operations with availability, health,
  version, freshness, and retry metadata; compatibility lookup; descriptor
  validation.

`check:neutrality` (a CI gate) scans core production source for industry terms,
phrase-routing, and website patterns and passes.

## 3. Preserved local changes

All correct local repairs from earlier passes are preserved and revalidated:
dedicated `kOpenNewTab` via `chrome::AddAndReturnTabAt`; real lifecycle
reconciliation delegation; direct `WorkspaceSwitchObserver` observation;
projection/shell registration only after `RootTabCollectionNode::Init()` and
unregistration at the start of `ResetTabStrip`; production `GetSeoulRootNode()`
seam with no testing accessor in production; scoped per-window shell-region host
ownership; the single reversible integration patch; user-facing status strings.
None were discarded or rebuilt in another style.

## 4. Native browser tests (cleared this pass)

Four files, 16 `IN_PROC_BROWSER_TEST_F` cases, zero empty bodies, all grounded
in APIs verified by reading the source and the pinned checkout:

- `shell/shell_browsertest.cc` (3): the profile-scoped services are constructed
  and wired for a regular profile; the Chromium tab strip remains the owner of
  tabs (Seoul projects, it does not replace); the organization model is
  reachable and has its default workspace.
- `commands/chromium_mutation_adapter_browsertest.cc` (6): the profile-scoped
  `CommandExecutor` applies a model-only command to the real model; the
  `ChromiumMutationAdapterImpl` resolves a Seoul `LiveWindowKey` to the real
  `Browser` and inserts a real tab; an unknown window key is rejected with
  `kWindowNotFound` and mutates nothing; activate, set-pinned, and move operate
  on the real tab strip through resolved targets.
- `projection/vertical_presentation_browsertest.cc` (2): a real tab-strip change
  publishes a live snapshot that the projection service turns into a controller
  and switcher keyed to the real window; an unknown window is never projected.
- `product/browser/seoul_runtime_browsertest.cc` (5): the product runtime is
  wired for a regular profile; available capabilities are executor-backed;
  text goals create tasks through an explicit window binding; two browser
  windows resolve to separate binding tokens; and `chrome://seoul-canvas` is
  registered as a first-party WebUI config.

Wiring: `chrome/test/BUILD.gn` gains the four `*_browser_tests` source_sets in
the `browser_tests` target's deps via the integration patch. These have NOT been
compiled or run on this host (no build host); they are structured to build under
the standard `browser_tests` binary once a host is available.

## 5. Runtime composition and GN graph

`runtime/seoul_runtime.*` composes the capability graph, connector registry,
scene registry, and reasoning-router policy as one pure per-profile owner, with
documented state ownership and a defined teardown order; it is unit-tested.
`product/browser/seoul_runtime_service.*` instantiates it as the profile-keyed
product owner, wires `SceneResolvers`, concrete transports, provider registry,
planner, task/surface/thread/workflow services, page agent, and builtin
executors, and registers the service through the integration patch. The GN
layering keeps Chromium-facing code in `_chromium` targets, and
`check:native-arch` confirms no unscoped process-global singletons or
namespace-scope mutable globals in production source.

Gap: source-connected only. No native target has compiled or run, so profile
startup, shutdown ordering, credential behavior, provider health, and task
runtime behavior still need capable-host verification.

## 6. Seoul Canvas

Authored (`canvas/`): `SeoulCanvasUI` (`MojoWebUIController`),
`SeoulCanvasUIConfig` (`WebUIConfig`, host `seoul-canvas`), the Mojo
`PageHandlerFactory`/`PageHandler`/`Page` boundary, the page handler, and
packaged resources (`canvas.html`, `canvas.ts`, `canvas.css`) served under a
strict CSP (script-src 'self', no eval, no remote script) that render SAUI with
safe DOM only.

The patch adds `//seoul/browser/canvas` to browser deps, registers
`SeoulCanvasUIConfig`, wires the build_webui resource pack, and adds the
`kSeoulCanvas` side-panel id. The page handler now obtains its target browser
from Chromium's WebUI embedding context, creates a runtime window-binding
token, and routes component events and text turns only after that token resolves
to the same live browser window.

Gap: the actual Seoul side-panel entry is not registered yet. A direct
`chrome://seoul-canvas` tab is intentionally `window_unbound`; the product still
needs a first-party, window-scoped side-panel entry plus capable-host WebUI and
multi-window tests.

## 7. Provider adapters

Authored (`intelligence/`): a provider protocol with `IsLocalOnlyEndpoint`
enforcement, chat-completions and messages request builders and stream parsers,
an SSE parser, a streaming accumulator, and local and cloud model providers over
an injected `HttpTransport` and `CredentialStore`; unit-tested through
`FakeHttpTransport`/`FakeCredentialStore`. Apple platform speech adapters
(`voice/platform/*.mm`, macOS-only) wrap `SFSpeechRecognizer` and
`AVSpeechSynthesizer`.

Gap: real endpoint checks, real credential access, real audio capture, and
benchmarks were not run. Credentials are specified to live in the macOS
Keychain, never in the repository, never exposed to Canvas, never logged.

## 8. Stable subsystems (source complete, unit-tested)

These pure-model subsystems are unchanged in intent and remain source-complete:
organization model and lifecycle bridge; workspace projection; the outbound
command layer; the SAUI protocol/catalog/parser/validator/patches/events; the
versioned workflow graph with cycle detection and compilation; the plan model
and validator; the tool/capability registry; deterministic-first reasoning
routing; the task deck and observe-verify-decide execution with the
unknown-outcome-never-auto-retried rule; context threads with unrepresentable
forbidden classes and cloud minimization; scenes; themes with WCAG contrast
validation; site layers compiled to safe scoped CSS; and the connector seam with
namespace-ownership enforcement. Generic connector import paths (OpenAPI, MCP,
local file, browser, information) build capabilities into the graph.

## 9. Tests

- Unit: 414 `TEST`/`TEST_F` cases across 57 `*_unittest.cc` files. No empty
  bodies. Coverage includes semantic shape/role validation and streaming merge,
  interface-compiler held-out unseen-schema generalization, capability-graph
  availability/health/compat, connector imports, provider protocol and stream
  parsing, and all stable subsystems. Aggregated by
  `//seoul/browser:seoul_unittests`.
- Browser: 16 `IN_PROC_BROWSER_TEST_F` cases across 4 files (section 4), wired
  into `//chrome/test:browser_tests`.
- Not executed on this host: no native C++ test (unit or browser) was compiled
  or run (no GN, no compiler). The TypeScript harness suite and all repo static
  gates were run and pass (section 10).

## 10. Static verification performed this pass (all clean)

- `npm run ci`: green end to end. This chains `tsc --noEmit`, the harness build,
  extension validation, and 110 harness tests (110 pass, 0 fail), then
  `check:scripts` (`bash -n` over all shell scripts), `check:json`,
  `check:manifest` (patch sha256 consistency), `check:boundary` (341 tracked
  files, no Chromium source/build/profile/secret/evidence), `check:neutrality`
  (core domain-neutral), `check:harness-arch`, and `check:native-arch`.
- clang-format (Chromium style, via the pinned checkout's binary) on all C++
  authored this pass: clean.
- `git diff --check`: clean (a stray single-space blank context line in the
  patch was made a true empty line so the patch stays whitespace-clean and still
  applies).
- Header include resolution against the pinned checkout: 153 Chromium includes
  resolved; direct-include audit passed.
- Attribution/vendor-name scan: a vendor name in `local_model_provider.h` was
  reworded to a vendor-neutral description; the working tree now has zero
  prohibited assistant/vendor names.
- Patch round-trip against the pinned checkout: `git apply --check`, apply,
  `git apply -R --check`, and reverse all clean; zero tracked edits remain after
  reverse.
- Materialize round-trip: `apply`, `verify` (overlay matches source), and
  `reverse` all clean. `materialize.sh` was fixed to exclude `.DS_Store` so the
  overlay is deterministic.
- Checkout restored: `src` HEAD at the lock
  `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`, no `src/seoul` overlay, no applied
  patch.

## 11. Build, runtime, performance

- Build state: not built. GN generation, `gn check`, compilation, and linking
  were not performed on this host.
- Runtime state: no runtime verified; the browser was not launched from a Seoul
  build.
- Performance: no numbers. The performance plan requires a build and is
  unmeasured. Lazy initialization is a design requirement recorded in the specs;
  no heavy resource is loaded at startup by construction (registries and models
  are pure until an adapter is attached).

## 12. Competitive evidence

`docs/product/seoul-competitive-review.md` records competitor behavior with
per-claim source tags and honest uncertainty, and states Seoul's structural
differences as hypotheses to test, not measured advantages. No superiority claim
is made.

## 13. Remaining blockers

1. A capable Apple-silicon build host with full Xcode and the pinned toolchain
   (mandatory to compile, run tests, and proceed).
2. `SeoulRuntimeService` (profile-keyed) instantiating `SeoulRuntime`, with
   `SceneResolvers` wired to the organization/theme/site-layer stores, and its
   registration added to the integration patch.
3. Canvas host wiring: add `//seoul/browser/canvas` to `//chrome/browser` deps,
   register `SeoulCanvasUIConfig`, wire the build_webui resource pak, add a
   per-window side-panel entry, and route the page handler into the runtime.
4. Concrete provider transports (loopback and cloud HTTP over the injected
   transport) and real audio capture, plus their benchmarks.
5. Native Views shell work (Essential resolver, workspace dialogs, split
   chooser, searchable launcher dispatch, action/accelerator registration,
   tab-role decoration, collapsed mode).

## 14. Continuation checkpoint (exact next action)

State at checkpoint: all source and both test tiers are authored (browser tests
wired to `//chrome/test:browser_tests`); every available static gate is green;
the checkout is clean at the locked HEAD; the patch and materialize round-trips
are clean.

On a capable host, in order:

1. `native/scripts/verify-checkout.sh` (read-only lock verification).
2. `native/scripts/materialize.sh apply` then `native/scripts/patches.sh apply`.
3. `native/scripts/gen.sh`, then `gn check out/SeoulBaseline //seoul/...` to
   validate the dependency graph (this is the final graph check that could not
   run here).
4. `autoninja -C out/SeoulBaseline seoul/browser:seoul_unittests`; fix the first
   real compile error and iterate to a green unit-test run of all suites.
5. `autoninja -C out/SeoulBaseline browser_tests` and run the Seoul
   `IN_PROC_BROWSER_TEST_F` cases; iterate to green.
6. Author blockers 2 and 3 (runtime service and Canvas host wiring) against the
   running build, then blockers 4 and 5.

Full procedure: `docs/native/seoul-product-build-runbook.md`.
