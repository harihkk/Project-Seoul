# Seoul product readiness

This report is the single source of truth for what exists, what was verified,
and what remains. It uses ASCII punctuation only and makes no claim of
compilation, runtime behavior, or superiority that was not actually produced.
Where a subsystem is source-only, it says so; where a step needs a build host,
it says so.

## Verdict

SEOUL PRODUCT BUILD INCOMPLETE

The build gate was run again on this host (2026-07-02) and hard-fails on
physical resource limits, not policy. Exact output of
`native/scripts/build-host-check.sh` (exit 1):

```
==> Seoul build-host readiness gate
[seoul-native] minimums: RAM >= 16 GiB, free fast storage >= 150 GiB
PASS  host is macOS arm64
FAIL  RAM 8 GiB is below the 16 GiB minimum; the chrome link will thrash or OOM
FAIL  free storage 60 GiB is below the 150 GiB minimum for a build
PASS  full Xcode active: Xcode 26.6
PASS  macOS SDK reachable (26.5)
PASS  checkout verification passed

==> build-host result
[error] this host is NOT cleared to build Chromium; do not run gen.sh/build.sh here
```

Everything the gate permits on this host was executed and is green: checkout
verification (`verify-checkout.sh`, exit 0, HEAD at the locked revision,
`gclient validate` clean), materialization (`materialize.sh apply` + `verify`,
now also mirroring the canonical `protocol/` contract to
`src/seoul/protocol/`), and patch application (`patches.sh verify`: the series
applies and reverses cleanly). The gate was NOT bypassed and no ceiling was
weakened. Per this project's rule, the only permitted deferral is heavy
Chromium compilation and execution the host cannot support; that is exactly
what is deferred here: `gn gen`, `gn check`, compilation, linking, unit-test
and browser-test execution, the Chrome build, and a profile launch. None of it
was faked. The verdict is `SEOUL PRODUCT BUILD INCOMPLETE` and cannot honestly
be raised; `SEOUL PRODUCT FUNCTIONAL ALPHA` additionally requires the full
native product spine (typed task snapshots over Mojo, every declared surface
action handled, the Lit Canvas WebUI and visual engine, measured local voice,
the shell differentiators, origin-scoped agent permissions) verified by the
production browser-test scenario - see sections 13 and 14. A standalone
prototype cannot earn that verdict, and this report does not claim it.

Progress this pass (2026-07-02, convergence and product closure; all
verifiable on this host):

- ONE CANONICAL CROSS-LANGUAGE PROTOCOL. `protocol/` now holds the versioned
  wire contract: `semantic-result`, `adaptive-surface`, `surface-patch`,
  `component-event`, `task-snapshot`, and `capability-descriptor` schemas
  covering every native semantic shape (24) and role (30), field metadata,
  provenance, citations, conflicts, streaming/partial state, continuation,
  unavailable fields, structured errors, task receipts and status, surface
  actions, and all ten patch ops. TypeScript types are GENERATED from the
  schemas (`protocol/ts/types.ts`, drift-gated); the shared fixture corpus
  (`protocol/fixtures/`: the twenty semantic cases from scalar through
  error-result, plus surface/patch/event/task/capability, version-compat, and
  invalid corpora) is consumed by BOTH the TypeScript tests
  (`npm run test:protocol`, running green) and new native conformance test
  sources (`semantic_wire_unittest.cc`, `saui_protocol_fixtures_unittest.cc`,
  `task_snapshot_wire_unittest.cc`, `tool_descriptor_wire_unittest.cc` -
  source-complete, compile blocked by the host gate). New native codecs close
  the wire gaps: `semantic/semantic_wire.*` (canonical SemanticResult JSON),
  `product/task_snapshot_wire.*`, `tools/tool_descriptor_wire.*`, and a
  `set_bindings` patch op in `saui/saui_patch.*`. `npm run check:protocol`
  (in `npm run ci`) extracts the enum wire names from the native sources and
  fails if the schemas ever drift from them - 23 enum families are parity-
  checked today.
- THE CANVAS PROTOTYPE IS NOW THE HONESTLY-BOUNDED DESIGN LAB. Renamed and
  documented as such (`apps/canvas-prototype/README.md` states what is
  synthetic: twenty fixed fixture capabilities, a lexical token-overlap
  router, fixture-contract validation instead of real observation, a separate
  simplified compiler policy, explained fallbacks). Its independently
  authored TypeScript semantic model is DELETED; the Lab consumes the
  generated canonical types and validates every document against the actual
  schema files. The six hardcoded example rows are gone - the catalog UI is
  generated from the registered fixture capability descriptors. The
  "verified" wording is gone - receipts say `fixture contract validated`
  (method `fixture_contract`), and every artifact is labeled synthetic demo
  data.
- REAL STABLE-ID SURFACE PATCHES IN THE LAB. The monolithic `canvas.ts` is
  split into cohesive modules (state, fixture runtime, surface store,
  renderers, charts, representation controls, provenance UI, receipts,
  catalog). Every change - representation switch, synthetic stream batch -
  is a canonical patch document validated against the schema and applied
  atomically (invalid paths and dangling bindings roll back; revision bumps
  once). The reconciler updates only the components the change summary names;
  a browser smoke test proves the artifact element, its margin, focus, text
  selection, and scroll position all survive a patch and that a
  representation switch does NOT replace the artifact element. Full-DOM
  replacement is gone. `npm run test:canvas` (type-check + 28 tests including
  the headless-browser suite) is now part of `npm run ci`.

Hardening pass (re-run 2026-07-11 against the REAL
pinned checkout headers): a new gate, `native/scripts/syntax-check.sh`
(`npm run check:syntax`, in `npm run ci`), parses every Seoul .cc with clang
-fsyntax-only against the actual M149 headers in the external checkout,
stubbing only gn-generated buildflag headers. It now enumerates both tracked
and untracked Seoul sources, including native Views. Result: 150 files parse
clean; 36 files that need gn-generated code are SKIPPED, each with the exact missing
generated header named in the output (mojom-forward headers, optimization-
guide protos, perfetto tracing protos). Skip classification is
EVIDENCE-BASED - a file is skipped only when the parse itself demonstrates a
generated-header dependency; there is no path allowlist - and a header that
exists in the checkout but fails to resolve fails the gate itself. This gate
immediately invalidated the tree's previous "source-complete, structured to
build" confidence and found real defects, all now fixed:

- The pinned Chromium has REMOVED `base::Value::Dict`/`base::Value::List`;
  the entire Seoul tree used them (82 files, ~500 occurrences). Migrated to
  `base::DictValue`/`base::ListValue`. Without this, nothing under
  `native/seoul` would have compiled.
- `base::JSONReader::Read/ReadDict/ReadList` now require the `options`
  argument; 12 Seoul call sites migrated to strict `base::JSON_PARSE_RFC`.
- `GURL::host()` now returns `std::string_view`
  (intelligence/provider_protocol.cc fixed).
- Four observer interfaces used with `base::ObserverList` did not derive
  from `base::CheckedObserver` and could not compile (and would not have
  caught use-after-destroy): CommandCompletionObserver, ShellObserver,
  ProjectionObserver, LiveWindowStateObserver. All are checked observers now.
- Missing includes that only ever worked transitively (or never):
  data_validation.cc/_unittest (data_errors.h), model_provider.h
  (base/types/expected.h), organization_model.h (base/functional/callback.h),
  expected_observation_registry.h (command_errors.h), projection_types.h
  (organization_ids.h, organization_types.h), shell_controller.h
  (shell_errors.h), shell_controller.cc (lifecycle_coordinator.h),
  organization_recovery_unittest.cc (organization_model.h).
- `WorkflowService` called the workflow editor with stale pre-clock
  signatures and could not compile; it now injects a clock (matching
  TaskService) and threads it through every editor call; `AddNode` takes the
  editor's `after_node_id`. Runtime and test call sites updated.
- Two test fakes were abstract (missing `OpenNewTab` override):
  command_executor_unittest.cc, workspace_switcher_unittest.cc.
- Two tests misused `EnsureDefaultWorkspace()` (it returns a status; the id
  accessor is `default_workspace()`), and surface_service_unittest.cc used a
  `FieldPrimitive::kText` enum value that does not exist (now kString).
- `SchemaField` lacked the equality WorkflowParam's defaulted `==` requires.
- In the organization service (reached once skip classification became
  evidence-based instead of path-based): a missing lifecycle_coordinator.h
  include, an invalid `override` on the non-virtual SessionRestoreObserver
  destructor, a missing shell_types.h include in shell_service.h, and a
  `base::WeakPtr` receiver bound to the bool-returning prefs writer - which
  Chromium's bind machinery statically forbids - now bound through a
  WeakPtr-argument adapter that reports a failed write after destruction.
- An independent adversarial review of the full diff (own-context agent,
  every finding verified by execution or against the pinned headers) found
  nine real defects; all are fixed: a wrong child-count assert in the new
  saui fixtures test; a Lab compiler path that bound a series entry
  conversion never emitted (freezing the surface for all future patches); a
  synthetic stream that fed the chart different values than the rows table;
  the TS patch engine accepting patches native rejects (container rule,
  duplicate ids, binding kinds, depth/count caps) - closed by GENERATING the
  component catalog + limits from the native sources into
  protocol/component-catalog.json (drift-gated) and enforcing it in the Lab
  engine, which immediately also caught the Lab's missing bar-chart
  `baseline_zero` prop; set_state message-clearing divergence (mirrored to
  native semantics); int64 cost fields silently truncated through a 32-bit
  wire read (now double-carried with integral/range rejection and schema
  maxima); replace-under-new-id leaving a stale DOM element (both engines
  now report the old id); the descriptor schema advertising a `url` field
  type the native importer rejects (removed); and a shared depth-cap gap on
  structural patch ops (native ApplyOp now bounds depth like parse does,
  with tests). The final finding in that review is now CLOSED: semantic field
  ids may use their full 64-character budget while tighter SAUI keys are
  derived collision-free in both compilers. Safe keys stay unchanged; longer
  or reserved identifiers receive deterministic schema-local `field_N` keys,
  composite entries use bounded ordinal paths, and adversarial native/TS tests
  pin the mapping.
- Design Lab chart policy brought to parity with the native compiler's
  honesty guards: single-point data and data without complete provenance
  (source, retrieved-at, effective-at) never chart and never appear in the
  representation switch; they fall back to an explained table (new
  generalization tests pin this).

The prior "browser tests wired and structured to build" and "source-complete"
statements in this report were written before any real header contact and
overstated confidence; parse verification is now the minimum bar for any
native source claim in this repository. Compilation, linking, and execution
remain deferred to the capable host and nothing here claims otherwise.

Prior pass (source-level, fully verifiable without a build): the
task-to-surface production path is now closed. `SurfaceService::CreateFromSemantic`
was previously called only by tests, so a completed task produced no artifact.
A runtime-owned `TaskSurfaceBridge` now observes the task service and compiles
each verified semantic result into a surface (creating once, patching the same
surface in place on streaming/terminal updates, one surface per task, nothing
for a data-less or failed task). Three unit tests prove a surface appears with
no test calling `CreateFromSemantic`, and a new `check:product-arch` rule fails
CI if the production caller ever disappears. The Canvas `kNavigate` action, which
previously did nothing, now opens its validated URL through the same capability
path; `kBrowserCommand`/`kWorkflowEdit` report an explicit unsupported state
instead of failing silently.

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
- The Seoul Canvas WebUI (top-chrome controller/config, Mojo
  `PageHandlerFactory`, Lit renderer, page handler, packaged resources) is
  source-connected and registered at `chrome://seoul-canvas`. The pinned patch
  registers one window-scoped side-panel entry per regular browser and the
  Shell launcher opens that exact entry; tab-loaded Canvas documents
  intentionally fail closed as unbound. None of this path has compiled or run.
- Provider adapters (local model, cloud model, Apple speech-to-text and
  text-to-speech) are authored with injected transports and are unit-tested
  through deterministic fakes. Concrete loopback/cloud HTTP transport and
  Keychain credential storage source exists, but real endpoint, credential,
  and audio behavior is not runtime-verified.
- Shell source now includes exact current- and cross-window Essential reuse,
  an explicit split-partner chooser, searchable typed-action dispatch, a
  window-scoped launcher accelerator, a persistent Task Deck status foothold,
  and compact-shell presentation. These Views paths remain uncompiled and need
  native focus, accessibility, geometry, multi-window, and postcondition tests.
  Tab-role decoration and the remaining shell surfaces are still absent.
- Scene reference validation now resolves workspaces, Themes, and Site Layers
  against their authoritative registries; phantom non-empty Theme IDs no longer
  pass. Scene and catalog metadata are reachable through Studio's read-only
  index. Scene, Theme, and Site Layer registries round-trip validated, bounded
  state in the product pref; Scenes restore only after referenced catalogs and
  skip removed references. Creation/editing, activation, mutation scheduling,
  and reference application remain unreachable.
- Nothing was built or run on this host: no GN generation, no `gn check`, no
  compilation, no linking, no browser launch (the build-host gate refuses this
  host).

## 1. Repository state

- Branch `main`; no commits, no history rewrite, no push, no reset, no
  discarded local changes. The working tree is uncommitted as required.
- All tracked and non-ignored untracked files are covered by the repository
  boundary gate. Source spans 21 module directories under
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

Four files, 18 `IN_PROC_BROWSER_TEST_F` cases, zero empty bodies, all grounded
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
- `product/browser/seoul_runtime_browsertest.cc` (7): the product runtime is
  wired for a regular profile; available capabilities are executor-backed;
  text goals create tasks through an explicit window binding; two browser
  windows resolve to separate binding tokens; and `chrome://seoul-canvas` is
  registered as a first-party WebUI config. Preview opens outside the tab strip
  and dismisses without residue. A bounded real-page AX snapshot classifies
  password, payment, and one-time-code fields, refuses model value mutations,
  and leaves an ordinary field writable.

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

The native page boundary no longer serializes current form-control values into
model-visible semantic results. `page_field_safety.*` classifies Chromium's
protected-password state plus standards-defined credential, one-time-code, and
payment `autocomplete` tokens. Sensitive controls remain focusable/clickable so
browser-owned autofill can operate, but model-driven type/clear/select actions
fail before an AX mutation reaches the renderer. The product architecture gate
prevents value projection or removal of this refusal path.

Gap: source-connected only. No native target has compiled or run, so profile
startup, shutdown ordering, credential behavior, AX/HTML sensitive-field
classification, provider health, and task runtime behavior still need
capable-host verification. Standards-nonconforming payment fields do not leak
their current values, but native Chromium Autofill field-type integration is a
future defense-in-depth improvement for action refusal.

## 6. Seoul Canvas

Authored (`canvas/`): `SeoulCanvasUI` (`TopChromeWebUIController`),
`SeoulCanvasUIConfig` (`DefaultTopChromeWebUIConfig`, host `seoul-canvas`), the
Mojo `PageHandlerFactory`/`PageHandler`/`Page` boundary, the page handler, and
packaged Chromium Lit resources (`CrLitElement`, checked-in `canvas.html.ts`,
generated CSS module) served under a strict CSP (script-src 'self', no eval, no
remote script). Payload values are escaped Lit interpolations; raw HTML and
imperative DOM rendering sinks are architecture-gated out. Every accepted
chart and non-input map primitive has a deterministic local SVG renderer with
the canonical source data retained as an accessible table. Catalog growth
fails CI until the new visual type is deliberately handled.

The patch adds `//seoul/browser/canvas` to browser deps, registers
`SeoulCanvasUIConfig`, wires the build_webui resource pack, and registers one
window-scoped `kSeoulCanvas` side-panel entry per regular-profile browser. The
Shell command launcher opens that exact entry. The page handler obtains its target browser
from Chromium's WebUI embedding context, creates a runtime window-binding
token, and routes component events and text turns only after that token resolves
to the same live browser window.

Canvas also contains a reachable read-only Studio index. A typed Mojo snapshot
projects the live profile's provider-route flags, Scene, Theme, and Site Layer
metadata into Lit cards and honest empty states. It deliberately excludes
credentials, local endpoints, selected model names, raw provider errors, page
content, and executable markup. This is an inspection surface only: Studio
editing, Scene activation, Theme management, and applying Site Layers to a live
page are not implemented.

Gap: none of the top-chrome controller, generated Mojo/Lit resources, entry
registration, or launcher path has compiled or run in Chromium on this 8 GiB
host. A direct `chrome://seoul-canvas` tab remains intentionally
`window_unbound`. The responsive shipping stylesheet was visually checked at
the default viewport and 420x800 through a temporary local fixture (no overflow
or console warnings), but this is not a substitute for capable-host WebUI,
multi-window, accessibility, and production-data visual tests.

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

- `npm run ci`: every deterministic/static stage is green. This chains
  `tsc --noEmit`, the harness build, extension validation, and 110 harness tests
  (110 pass, 0 fail), then
  `check:scripts` (`bash -n` over all shell scripts), `check:json`,
  `check:manifest` (patch sha256 consistency), `check:boundary` (tracked and
  non-ignored untracked files, no Chromium source/build/profile/secret/evidence), `check:neutrality`
  (core domain-neutral), `check:harness-arch`, and `check:native-arch`. The last
  Puppeteer smoke test is sandbox-blocked; the identical elevated command passes
  28/28, as recorded below.
- The pinned checkout has no clang-format binary and none is installed on this
  host, so a formatter gate was not run. Native parse checks and
  `git diff --check` are not substitutes; formatting remains a build-host gate.
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

Blocker 1 gates everything after it; per the convergence directive, major
native feature development does not continue until Chrome builds and launches.

1. A capable Apple-silicon build host (>= 16 GiB RAM, >= 150 GiB free fast
   storage, full Xcode) - mandatory to compile, run every Seoul unit-test and
   browser-test executable, build Chrome, and launch a disposable profile.
   Runtime-service registration and the Canvas host are connected in source:
   the integration patch registers `SeoulRuntimeServiceFactory`, the top-chrome
   `SeoulCanvasUIConfig`, and one `kSeoulCanvas` `SidePanelEntry` per regular
   browser window; the Shell launcher opens the entry for its exact bound
   window. `TaskSurfaceBridge` projects task results to surfaces in production.
   None of this has compiled or run.
2. Typed task snapshots over Mojo - SOURCE COMPLETE, awaiting build-host
   compile. `canvas.mojom` now carries `PushTaskSnapshot` plus
   ListTasks/GetTask/Pause/Resume/Cancel/ApproveStep/ProvideTaskInput/
   ListTaskSurfaces/SaveTaskAsWorkflow; the handler pushes
   `task_snapshot_wire` JSON on every task observer event (window-filtered),
   and the WebUI renders pause/resume/approve/reject plus a typed missing-input
   form. Input is bounded, tied to the exact pending `kUserInput` step, recorded
   as a receipt, and replanned through the configured reasoning route; it is
   not discarded or applied to a different step. This pass also fixed
   TaskService's failure to consume an already-recorded initial approval/input
   wait, which could otherwise become a false assumption-invalid stop.
   Mojom/handler cannot be
   parse-verified on this host (mojo codegen); they compile first on the
   build host.
3. `SurfaceActionKind` completeness - SOURCE COMPLETE at the dispatch layer:
   `kBrowserCommand` routes through the fail-closed capability registry and
   `kWorkflowEdit` through `WorkflowService` ops; nothing silently drops.
   `SurfaceActionCompletenessTest.EveryDeclaredActionKindHasAnOutcome` proves
   every declared kind resolves to a non-silent outcome. Runtime behavior
   still needs the build host.
4. Shipping Canvas Lit + visual engine - SOURCE COMPLETE, awaiting build-host
   compile/runtime proof. The production resource is a `CrLitElement` with a
   checked-in `.html.ts`, `build_webui` Lit dependency, generated CSS module,
   and no raw HTML/imperative rendering sinks. A deterministic local SVG engine
   covers every accepted chart and non-input map component and always exposes
   its source data as an accessible table. `check:canvas-webui` type-checks the
   source without a Chromium out directory; `check:product-arch` derives visual
   coverage from the canonical catalog and locks the canonical `actions` wire
   field (fixing the old renderer's incorrect `action_ids` lookup). Responsive
   fixture QA passed at desktop and 420x800 with zero console warnings/errors
   and no horizontal overflow. Generated-resource compile, actual Mojo data,
   keyboard/screen-reader QA, and side-panel runtime remain build-host work.
5. Seoul Voice from measured local components. `apps/voice-lab` exists and
   the first measurement rounds have RUN on the dev M2 (8 GiB) - weaker than
   any target host, so these numbers are upper bounds. Runtime and all model
   artifacts are sha256-pinned with licenses in `apps/voice-lab/manifest.json`
   (sherpa-onnx v1.13.3 runtime; process-spawn overhead included in all wall
   times):
   - TTS `piper en_US-amy-medium` (63 MB): first-audio upper bound 940 ms
     COLD including full model load per call; RTF 0.128. Resident in its own
     process this bounds first audio well under 200 ms.
   - TTS `kokoro-en-v0_19` (82M params, 311 MB): cold 3.4 s, warm short
     sentence ~2.0 s, RTF 0.83 - the richness tier; needs target-host
     re-measurement before it can gate the interactive path.
   - ASR `zipformer-streaming-en int8` (streaming/partials): cold 1.67 s,
     RTF 0.134, peak RSS 230 MB, self-play WER 18.4% (clean synthetic
     speech, upper bound; streaming trades accuracy for latency).
   - ASR `whisper-small.en int8` (final pass): cold 3.35 s, RTF 0.237, peak
     RSS 1.17 GB, self-play WER 4.3% (homophone spellings included).
   Self-play WER = recognizing this lab's own TTS output of known scripts -
   NOT user-voice WER; the recorded evaluation set (Indian + American
   English, noise, names, URLs, code terms, corrections, interruptions) is
   still required and is the number that picks the Voice Pack. Streaming
   partial latency needs the c-api paced-feed harness (build host). Then
   `SeoulVoiceService` (inference in a sandboxed utility process; Apple
   providers as fallback, never the identity). The Design Lab already ships
   the voice summarizer (`voice-summary.ts`, spoken twin of the interface
   compiler: insights from shapes and roles only, honesty riders for
   failure/partial/conflict/gaps) behind an explicit default-off toggle.
6. The shell differentiators, in the ordered list from the product definition
   (Essential associations, split chooser, dynamic launcher, Task Deck,
   Preview, Scenes, Themes, Site Layers, workflow editor, Context Threads,
   routing, archive, Studio, collapsed shell). The dynamic launcher slice is
   now source-connected as a native searchable bubble: bounded local filtering,
   Enter-to-run, Down-to-results keyboard traversal, accessible labels and
   disabled reasons, and typed `ShellUtilityAction` dispatch. The previous
   static menu's silent self-launcher entry was removed. It still needs native
   compile plus focus/accessibility runtime tests. Collapsed-shell presentation is also connected in
   source: workspace icon/initial, vertical Essential controls, compact utility
   glyphs, and preserved accessible names/tooltips replace text that previously
   overflowed the narrow rail; runtime geometry and screen-reader proof remain.
   Essential association is now connected across current and other live
   profile windows: the live
   snapshot carries only a bounded title and serialized origin (never path,
   query, or fragment), the shell matches an Essential's origin, and opening it
   activates the exact existing tab/window instead of duplicating it.
   Cross-window browser-test proof remains.
   Split creation now requires an explicit titled partner through a native
   keyboard menu. Candidate derivation excludes the active tab and tabs already
   in a split; the non-UI convenience path succeeds only when exactly one
   candidate exists and can no longer pick the first of several tabs. Native
   menu geometry and command postcondition tests remain build-host work.
   The launcher now has a window-scoped Shift+Cmd/Ctrl+K accelerator registered
   by the footer view itself. FocusManager ownership makes registration and
   teardown follow the exact shell window without a process-global command;
   cross-platform conflict and interactive focus tests remain.
   Live title/origin refresh is limited to `TabChangeType::kAll`; loading-only,
   attention-only, and blocked-only churn does not rebuild metadata or publish
   shell snapshots. Capable-host tracing still has to confirm frame-time and
   allocation budgets under heavy tab activity.
   Recovery, reconciliation, and workspace-switch status text is a polite
   accessibility live region, so state changes are announced without stealing
   focus. VoiceOver/NVDA behavior still requires runtime evidence.
   The persistent Task Deck now has a native shell foothold. `TaskService`
   publishes `planning` immediately before any provider round trip; the profile
   runtime observes task changes and sends only bounded per-window state counts
   (total/active/waiting/paused/failed) to `ShellService`. A permanently
   reachable, accessible Task Deck button shows attention first in compact mode
   and opens the detailed Canvas deck. Goals, prompts, receipts, and results do
   not cross into the shell summary. Native observer-lifecycle and visual tests
   remain build-host work. The bridge consumes `StateSummaries()` rather than
   full task snapshots, so an update scans at most 500 tiny `{window,state}`
   records and never copies receipts or semantic payloads into browser chrome.
   Essential association now searches deterministic live snapshots across the
   profile after preferring the current window. The controller submits the
   exact tab/window pair through `CommandExecutor`, then a separately validated
   `ProfileBrowserCollection` lookup activates that exact normal window—never
   an active/last-focused fallback. Only origin and bounded title participate;
   native multi-window postcondition tests remain.
   Studio's first production surface is also source-connected: Canvas requests
   a bounded read-only projection of provider-route state, Scenes, Themes, and
   Site Layers from the exact window-bound runtime. No fixture catalog is used, and
   secrets, endpoints, raw errors, page data, and model names stay browser-side.
   Editors, activation/application paths, generated Mojo compile, visual QA,
   and runtime evidence remain.
   Preview now has a bounded pure lifecycle rather than only a reserved routing
   enum: safe HTTP(S) admission, one ephemeral record per exact window/parent,
   replacement isolation, navigation bounds, lifecycle cleanup, and explicit
   begin/commit/abort promotion to tab or split. A local M149 source audit
   confirms `views::WebView` can embed a separately owned WebContents and
   Chromium can accept that same ownership during promotion. The profile
   runtime now owns this lifecycle and dismisses it on exact parent-tab/window
   removal. A Chromium `SeoulPreviewWebView` containment layer blocks popup
   creation, downloads, script dialogs, and fullscreen escape. The bubble/
   transfer service, gesture/capability entry point, focus restoration, and
   visible controls remain unconnected and uncompiled.
7. Origin-scoped agent permissions - PARTIAL SOURCE COMPLETE, awaiting native
   build/runtime proof and adversarial browser tests. `AgentPermissionService`
   grants only exact capability + window + tab + main-frame + source origin +
   destination origin + connector-service tuples, expires grants (30-minute
   default, 24-hour hard maximum), and observes live window snapshots to revoke
   on tab or window removal independently of Canvas bindings. Page observation
   is first-use-per-origin; a matching grant auto-approves only that exact
   scope. Irreversible mutations and external side effects are never reusable.
   Invalid scopes become failed receipts rather than skipped work that could
   look successful. Pure adversarial tests cover origin/destination/tab/frame
   mismatch, expiry, high-risk non-reuse, internal-page denial, and revocation;
   prompt-injection/exfiltration browser tests and real navigation/tab-close
   lifecycle proof remain.
8. The production browser-test scenario: open Canvas -> submit goal -> plan ->
   execute a real browser operation -> observe actual state -> verify
   postcondition -> receipt -> automatic surface -> render -> patch the same
   surface -> save as workflow; plus the isolation/reconnect/approval/
   cancellation/regression matrix. No test may depend on the public internet.

## 14. Continuation checkpoint (exact next action)

Update (2026-07-23, cleanup pass): the frozen MV3 harness, its fixtures, its
integration suite, and the root TypeScript build were removed from the tree;
`protocol/` is the browser-control protocol reference. Repo gates are now
`npm run check` and `npm test` (`npm run ci` chains both). The reasoning router
is capability-first: the privacy tier was removed and `prefer_local` is an
off-by-default preference. Harness test counts below describe the pre-cleanup
tree.

State at checkpoint (2026-07-11): tracked plus untracked native parse sweep is
150 files clean with 36 evidence-based generated-code skips. Static gates, 110
harness tests, protocol drift and 8 conformance tests, shipping Canvas strict
TypeScript, and 27 non-browser Design Lab tests pass in the sandbox. The one
Puppeteer smoke test cannot launch Chromium inside the sandbox; the exact
`npm run test:canvas` command passes 28/28 when run with browser-launch
permission. The
canonical protocol is in place on both sides; the native conformance suites
are authored and wired into GN but have never compiled.

On a capable host, in order:

1. `native/scripts/verify-checkout.sh` (read-only lock verification).
2. `native/scripts/materialize.sh apply` then `native/scripts/patches.sh apply`
   (this also mirrors `protocol/` into `src/seoul/protocol/` for the
   conformance tests' data dependency).
3. `native/scripts/gen.sh`, then `gn check out/SeoulBaseline //seoul/...`.
4. Build every Seoul unit-test executable
   (`autoninja -C out/SeoulBaseline seoul/browser:seoul_unittests`) and RUN
   each one; the four protocol conformance suites must pass against the same
   fixtures the TypeScript tests consume. Repair narrowly, rerun the smallest
   target, then rerun the full gate.
5. `autoninja -C out/SeoulBaseline browser_tests` and run every Seoul browser
   test; iterate to green.
6. `native/scripts/build.sh` (Chrome), then launch a disposable profile via
   `native/scripts/run.sh` and `native/scripts/smoke.mjs`.
7. Only after Chrome launches: work blockers 2-8 in order.

Full procedure: `docs/native/seoul-product-build-runbook.md`.
