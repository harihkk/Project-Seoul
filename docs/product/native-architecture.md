# Seoul native architecture

## What Seoul is

Seoul is a real, premium Chromium-based desktop browser. It is not a browser
extension, not an Electron app, and not an agent that drives someone else's
browser. It ships its own browser binary built from upstream Chromium plus
Seoul-owned native code.

Concretely, Seoul is built from two tracked code sources overlaid on a pinned,
otherwise unmodified Chromium checkout:

- **Repository-owned native source** under `native/seoul/`. This is original
  Seoul code (C++, WebUI assets, build files) that is compiled into the browser.
  It is owned, versioned, and reviewed inside the Project Seoul repository.
- **Minimal upstream integration patches** under `native/patches/chromium/`.
  These are small, ordered, independently reversible diffs applied over the
  pinned Chromium tree only where Seoul cannot integrate from the outside. The
  baseline currently carries zero patches (see `native/patches/README.md`).

The pinned upstream revision is recorded in `native/chromium.lock.json`
(Chromium 149.0.7827.201, commit `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`,
macOS arm64). The Chromium source and build output live OUTSIDE this repository,
under an external root (default sibling directory, validated against
`/Users/hk/Documents/Projects/seoul-chromium/src`). Nothing under that root is
tracked by Project Seoul Git.

Seoul has an original product interface and identity. It copies valuable feature
CAPABILITIES from other browsers; it never copies their proprietary source,
assets, branding, or layout.

Two product concepts define Seoul beyond stock Chromium:

- **A persistent activity/workspace model.** The user's browsing is organized
  into durable workspaces and activities that survive restarts, rather than a
  flat list of windows and tabs.
- **Native browser intelligence.** Page observation, typed browser actions,
  task state, and model-provider adapters are first-class browser subsystems
  compiled into the binary - not bolted on through an extension.

### Honest current state

Do not overclaim. As of this document:

- The Manifest V3 browser-control harness is frozen and serves only as a
  protocol and safety reference. It is not the product.
- The Chromium checkout is complete and gclient-validated.
- GN generation, compilation, launch, smoke, and runtime tab validation have
  NOT yet happened.
- There are zero Chromium patches so far.
- `native/seoul/` as described here is the planned home for Seoul-owned native
  source. The current baseline scripts refer to a future externally mounted
  overlay; this document defines the intended repository-owned layout that the
  build will mount onto the Chromium tree.

### Evidence convention

Every claim below about what upstream Chromium already provides was checked with
`git grep` / `ls` against the real local checkout at
`/Users/hk/Documents/Projects/seoul-chromium/src`, and the exact path is cited.
Anywhere a concrete Chromium integration point could not be confirmed in the
checkout, it is explicitly marked **research required** rather than invented.

---

## Subsystem boundaries

Each subsystem below states: its responsibility, where Seoul code will live, how
it integrates with Chromium, and the data it owns.

### 1. Workspace / activity model

- **Responsibility.** Own the top-level organizing concept above tabs and
  windows: durable workspaces, the activities inside them, and the mapping from
  activities to live browser state (tab groups, split layouts, focused content).
- **Where the code lives.** `native/seoul/workspace/` for the model and store;
  WebUI surfaces for the switcher live under `native/seoul/ui/` (see UI
  subsystem).
- **Chromium integration.** Builds on Chromium's tab and tab-group model rather
  than replacing it. Tabs are referenced through the public tab interface in
  `components/tabs/public/tab_interface.h` (verified). Split membership is keyed
  by `split_tabs::SplitTabId` defined in `components/split_tabs/split_tab_id.h`
  (verified - note this is under `components/split_tabs/`, NOT
  `components/tabs/public/`). Whether the workspace switcher is hosted in WebUI
  or Views is **research required** (see UI subsystem).
- **Data owned.** Workspace records, activity records, ordering, last-focused
  state, and the activity-to-tab/split mapping. Persisted via the session
  persistence subsystem; it does not own its own on-disk format independent of
  that subsystem.

### 2. Tabs and split layout

- **Responsibility.** Provide Seoul's tab presentation (including vertical tabs)
  and multi-pane split layout, extending upstream rather than reimplementing it.
- **Where the code lives.** Extensions live primarily as patches under
  `native/patches/chromium/` against the tab and frame view code, with any
  Seoul-only helper logic under `native/seoul/tabs/`.
- **Chromium integration (verified).**
  - Vertical tabs ship upstream behind a disabled-by-default feature:
    `BASE_FEATURE(kVerticalTabs, base::FEATURE_DISABLED_BY_DEFAULT)` in
    `chrome/browser/ui/tabs/features.cc` (verified). The pref
    `kVerticalTabsEnabled = "vertical_tabs.enabled"` is in
    `chrome/common/pref_names.h` (verified). Vertical views live in
    `chrome/browser/ui/views/tabs/vertical/` (verified; contains, e.g.,
    `vertical_split_tab_view.{h,cc}`, `vertical_pinned_tab_container_view.{h,cc}`).
  - Split layout uses `MultiContentsView` in
    `chrome/browser/ui/views/frame/multi_contents_view.{h,cc}` (verified; the
    two-pane limit is enforced there - confirm exact cap before relying on it).
    The split id type is `split_tabs::SplitTabId`
    (`components/split_tabs/split_tab_id.h`, verified). Split data/collection
    types are in `components/tabs/public/split_tab_data.h` and
    `components/tabs/public/split_tab_collection.h` (verified).
  - The split entry command is `IDC_NEW_SPLIT_TAB` (`= 34055`) in
    `chrome/app/chrome_command_ids.h` (verified), wired in
    `chrome/browser/ui/browser_command_controller.cc` (verified, handled at the
    `case IDC_NEW_SPLIT_TAB:` site and enabled via `UpdateCommandEnabled`). A
    bare `IDC_SPLIT_TAB` identifier was NOT found in the checkout; do not assume
    it exists - **only `IDC_NEW_SPLIT_TAB` is confirmed.**
- **Data owned.** Seoul-specific tab/split presentation preferences layered on
  top of the upstream prefs above. The authoritative tab and split model state
  remains owned by Chromium.

### 3. Session persistence

- **Responsibility.** Persist and restore workspaces, activities, windows, tabs,
  tab groups, and split layouts across restarts and crashes.
- **Where the code lives.** Seoul workspace/activity serialization in
  `native/seoul/session/`, integrated through patches under
  `native/patches/chromium/` where upstream session structures must carry Seoul
  fields.
- **Chromium integration (verified).** Restore flows through
  `chrome/browser/sessions/session_restore.cc` (verified); the session service
  is `chrome/browser/sessions/session_service.{h,cc}` (verified). Whether Seoul
  workspace/activity metadata can be attached via existing session extra-data
  hooks or requires a patch to the session command schema is **research
  required** - the exact extension point must be located in
  `session_service.cc` before committing to an approach.
- **Data owned.** The durable on-disk representation of workspaces and
  activities and their linkage to upstream session entities. This is the single
  store of record for the workspace/activity model.

### 4. Browser UI (WebUI vs Views)

- **Responsibility.** Seoul's original interface: the workspace switcher,
  activity surfaces, intelligence panels, and Seoul-specific chrome.
- **Where the code lives.** WebUI surfaces and their controllers under
  `native/seoul/ui/`; any native Views integration as patches under
  `native/patches/chromium/`.
- **Chromium integration.**
  - WebUI infrastructure is available: `content/public/browser/web_ui_controller.h`
    and `ui/webui/mojo_web_ui_controller.h` exist (verified). Chrome-level WebUI
    configs are registered in
    `chrome/browser/ui/webui/chrome_web_ui_configs.cc` (verified), which is the
    expected place to register a new Seoul WebUI host.
  - Views integration would attach to the browser frame views under
    `chrome/browser/ui/views/frame/` (directory verified via the
    multi_contents_view files).
  - **Research required:** the split between WebUI and native Views for each
    Seoul surface is not yet decided. The trade-off (WebUI for rich, iterable
    panels vs Views for tight integration with tab strip and frame) must be
    settled per surface; do not assume either until prototyped.
- **Data owned.** UI-only view state (panel open/closed, layout sizing). No
  authoritative product data; it reads from the workspace and intelligence
  subsystems.

### 5. Page observation

- **Responsibility.** Produce a structured, privacy-aware view of the current
  page for intelligence features: text content, structure, and actionable
  elements.
- **Where the code lives.** `native/seoul/observation/`.
- **Chromium integration (verified primitives).**
  - Per-page access is via `content/public/browser/web_contents.h` (verified).
  - Structural/accessibility data uses the accessibility tree types in
    `ui/accessibility/` - `ui/accessibility/ax_tree_update.h` and
    `ui/accessibility/ax_tree.h` exist (verified), and platform accessibility is
    `ui/accessibility/platform/browser_accessibility.h` (verified). The exact
    API for requesting an on-demand AX snapshot from a `WebContents` is
    **research required**.
  - A DevTools-protocol path exists via
    `content/public/browser/devtools_agent_host.h` (verified) as an alternative
    observation channel. Choosing between the accessibility-tree path and the
    DevTools path is **research required**.
- **Data owned.** Transient, in-memory page observations scoped to a tab.
  Nothing is persisted by default; any retention is governed by the
  telemetry/privacy subsystem.

### 6. Typed browser actions

- **Responsibility.** Expose a typed, validated set of browser actions
  (navigate, click, fill, open/close tab, split, switch activity) that
  intelligence and automation can invoke, with each action checked by policy
  before execution.
- **Where the code lives.** `native/seoul/actions/`.
- **Chromium integration (verified primitives).** Navigation and content
  interaction go through `content/public/browser/web_contents.h` (verified).
  Tab/window/split operations reuse the command ids in
  `chrome/app/chrome_command_ids.h` (verified, e.g. `IDC_NEW_SPLIT_TAB`) routed
  through `chrome/browser/ui/browser_command_controller.cc` (verified). The MV3
  harness under the frozen browser-harness app is the protocol reference for the
  action vocabulary, not the implementation.
- **Data owned.** The action schema/registry and per-action audit records.
  Actions are stateless executors; durable consequences belong to the task-state
  subsystem.

### 7. Permission and risk policy

- **Responsibility.** Gate sensitive actions (navigation to risky origins,
  credential use, downloads, automated form fills) behind an explicit risk
  policy and user consent. Central choke point for typed actions.
- **Where the code lives.** `native/seoul/policy/`.
- **Chromium integration (verified).** Builds on Chromium's permission stack:
  `components/permissions/permission_manager.{h,cc}` (verified) and the
  chrome-level client `chrome/browser/permissions/chrome_permissions_client.{h,cc}`
  (verified). How Seoul's action-level risk policy composes with the
  page-permission manager (wrap vs extend) is **research required**.
- **Data owned.** Seoul risk policy rules, per-origin/per-action decisions, and
  the consent ledger. Distinct from Chromium's site-settings store.

### 8. Credential mediation

- **Responsibility.** Mediate credential access for automated actions so secrets
  are never exposed to the model layer; broker fills through the existing
  password store with explicit policy checks.
- **Where the code lives.** `native/seoul/credentials/` (mediation/policy glue
  only).
- **Chromium integration (verified).** Reuses the upstream password manager:
  `components/password_manager/core/browser/password_manager.h` (verified). The
  exact brokering API (how a fill is requested and confirmed without surfacing
  the secret) is **research required** and must be located in the password
  manager component before design.
- **Data owned.** No secrets of its own. It owns mediation policy and an audit
  trail of credential-use requests and outcomes. Secrets remain in Chromium's
  password store.

### 9. Task state and recovery

- **Responsibility.** Track multi-step task progress, persist intermediate
  state, and recover or resume after navigation, crash, or restart.
- **Where the code lives.** `native/seoul/tasks/`.
- **Chromium integration.** Persistence rides on the session persistence
  subsystem and its restore path
  (`chrome/browser/sessions/session_restore.cc`, verified). The lifecycle hook
  for resuming a task after restore is **research required** (depends on the
  session extra-data decision in subsystem 3).
- **Data owned.** Task records, step history, and recovery checkpoints. This is
  the store of record for in-progress intelligence tasks.

### 10. Model-provider adapters

- **Responsibility.** Abstract one or more external model providers behind a
  stable internal interface so the rest of Seoul depends on a Seoul-owned
  contract, not a vendor SDK.
- **Where the code lives.** `native/seoul/providers/`.
- **Chromium integration.** Network egress uses Chromium's network service via
  the public content/services APIs; the exact client surface is **research
  required** (candidate paths under `services/network/public/` were NOT verified
  in this pass and must be confirmed before use). No upstream patch is expected;
  adapters should be self-contained Seoul code.
- **Data owned.** Provider configuration, endpoint/routing settings, and request
  accounting. No page or credential data is owned here; those arrive scrubbed
  from observation and policy.

### 11. Telemetry and privacy controls

- **Responsibility.** Govern what is measured and retained, default to privacy,
  and give the user enforceable controls over observation, task retention, and
  provider egress.
- **Where the code lives.** `native/seoul/telemetry/`.
- **Chromium integration (verified primitives).** Preferences are stored through
  `components/prefs/pref_service.h` (verified). Chromium's metrics stack exists
  (`components/metrics/metrics_service.h` and
  `components/metrics_services_manager/metrics_services_manager.h`, verified);
  whether Seoul reuses, restricts, or disables it is **research required** and a
  deliberate product decision, not an implementation default.
- **Data owned.** Privacy preferences, consent state, and retention policy. It
  is the authority other subsystems consult before retaining anything.

### 12. Testing and evidence

- **Responsibility.** Keep claims verifiable: unit tests for Seoul code,
  integration/smoke checks of the built browser, and an evidence trail for every
  upstream assertion.
- **Where the code lives.** Tests colocated with each subsystem under
  `native/seoul/<subsystem>/` (following Chromium's `*_unittest.cc` /
  `*_browsertest.cc` convention seen in the checkout, e.g.
  `chrome/browser/ui/views/tabs/vertical/tab_collection_node_browsertest.cc`,
  verified). Build/launch/smoke scripts live in `native/scripts/`
  (`build.sh`, `gen.sh`, `run.sh`, `smoke.mjs`, verified present).
- **Chromium integration.** Seoul targets build via GN/Ninja against the pinned
  tree; the frozen MV3 harness remains the browser-control protocol reference
  only.
- **Data owned.** Test fixtures and recorded evidence (cited upstream paths,
  smoke output). The evidence convention in this document - verify against the
  local checkout or mark research required - is the standing rule.

---

## Mapping to the native/seoul/ layout and patch-overlay model

Planned directory layout for repository-owned native source:

```
native/
  seoul/
    workspace/     # subsystem 1: workspace/activity model + store
    tabs/          # subsystem 2: Seoul-only tab/split helpers
    session/       # subsystem 3: workspace/activity serialization
    ui/            # subsystem 4: WebUI surfaces + controllers
    observation/   # subsystem 5: page observation
    actions/       # subsystem 6: typed browser actions
    policy/        # subsystem 7: permission and risk policy
    credentials/   # subsystem 8: credential mediation glue
    tasks/         # subsystem 9: task state and recovery
    providers/     # subsystem 10: model-provider adapters
    telemetry/     # subsystem 11: telemetry and privacy controls
    # tests are colocated per subsystem (*_unittest.cc / *_browsertest.cc)
  patches/
    chromium/      # ordered, reversible upstream integration diffs
  gn/              # baseline GN args (macos-arm64-baseline.gn, present)
  scripts/         # fetch/sync/gen/build/run/smoke (present)
  chromium.lock.json  # pinned upstream revision (present)
```

Two integration mechanisms, in strict priority order:

1. **Repository-owned native source (`native/seoul/`).** The default. Seoul code
   is compiled into the browser by mounting this tree onto the Chromium checkout
   at build time and referencing it from GN. Nothing upstream is modified.

2. **Minimal upstream patches (`native/patches/chromium/`).** Used only when a
   subsystem genuinely cannot integrate from the outside (for example, attaching
   Seoul fields to upstream session structures, or extending the vertical-tab and
   split-tab views). Per `native/patches/README.md`, each patch must be minimal,
   documented (what/why/rejected-alternative), and independently reversible
   against the locked revision. The baseline carries zero patches today.

Guiding rule from the baseline: upstream vertical tabs
(`chrome/browser/ui/tabs/features.cc`, verified) and split tabs
(`chrome/browser/ui/views/frame/multi_contents_view.{h,cc}`, verified) must be
EXTENDED before any replacement is considered. Patches are an overlay over a
pinned upstream tree, never a substitute for exhausting the existing upstream
implementations.

Subsystem-to-mechanism summary:

- Native-source dominant (little or no patching expected): workspace, ui,
  observation, actions, policy, credentials, tasks, providers, telemetry,
  testing.
- Patch-dependent (extend upstream): tabs/split layout (tab and frame views),
  session persistence (if workspace/activity metadata cannot ride existing
  session extra-data - **research required**).
