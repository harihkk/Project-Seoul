# Seoul product thesis

Status: This document states what Seoul is for and what would make a user
switch. Current compile, runtime, and release evidence is maintained in
`docs/release/seoul-product-readiness.md`. It does not claim Seoul is first or
best at anything; every measurable claim is marked as a claim to be tested.

## Target users

- People who run their day through the browser and hit the same friction:
  dozens of tabs, repeated multi-step chores (compare, fill, monitor, report),
  and answers that arrive as a wall of text they then have to act on manually.
- People who want to drive the browser by speaking as well as typing, and who
  want to see and confirm what an assistant is about to do before it does it.
- People who care where their data goes: what a cloud model can see, what stays
  on the device, and what a task actually did.

Seoul is a genuine browser first. It must be useful for ordinary browsing with
no model configured. Intelligence, when enabled, makes it substantially more
capable but is never a prerequisite for the browser working.

## The problems Seoul solves

1. Text answers do not persist or act. A spoken or typed question about weather,
   markets, or products yields a paragraph you cannot filter, sort, or pin.
2. Assistants act without showing the plan or confirming the result. You cannot
   see the step, approve the risky action, or read a receipt of what happened.
3. Automation is brittle. Recorded click macros break when a page changes;
   there is no editable, re-resolvable workflow.
4. Organization, appearance, and per-site tweaks are separate one-off settings
   rather than a reusable environment you can switch into.
5. It is unclear which work is local, which is cloud, and what each step cost.

## Table stakes (not differentiators)

Vertical tabs, workspaces, splits, themes, a sidebar, a command palette, "voice"
as dictation, and "AI" as a chat box each exist separately in shipping products
(see `seoul-competitive-review.md`). Seoul implements these because a browser
needs them, not because they are unique.

## What is meaningfully different

The defensible system is the integration, and it shows up in the source:

1. Voice-to-action and typed input share one session state machine
   (`voice/voice_session.*`), with immediate barge-in that preserves the running
   task.
2. Responses render as a validated, Seoul-owned adaptive visual protocol
   (`saui/`) instead of text alone: a model emits data, a trusted renderer
   builds the interface, and a chart cannot be drawn from one unverified number.
3. Actions run as typed browser operations through the existing native command
   layer, then are observed and verified, with a receipt per step
   (`tasks/task_execution.*`). An unknown-outcome mutation is never auto-retried.
4. Any successful task can become a versioned, editable workflow graph that
   compiles onto the same bounded task engine and re-resolves semantic targets
   each run (`workflows/`).
5. Organization, theme, Site Layers, routing, lifecycle, and assistant defaults
   compose into reusable Scenes (`scenes/`).
6. Reasoning routes deterministic-first, then local, then official cloud, with
   sensitive work forbidden from leaving the device and a per-task budget
   ceiling (`intelligence/reasoning_router.*`).
7. Context sent to a task or the cloud is user-approved and minimized; forbidden
   content classes are structurally unrepresentable (`context/context_thread.*`).

## Why a user would switch

- They can speak an outcome, watch the plan, approve the risky step, and get a
  live visual result they can keep and refine - in one browser, over real tabs.
- Their repeated chores become editable workflows instead of manual repetition.
- They can see and control what is local vs cloud, and read what each task did.

## Why a user would not switch

- Their workflow is fully served by an existing browser plus one AI extension.
- They distrust any agentic browsing and prefer a browser that does none of it
  (Vivaldi's stated position is a real market segment).
- They are locked into a specific ecosystem's assistant and accounts.
- Until Seoul is built, signed, and proven on their machine, there is nothing to
  switch to. This is the honest current state.

## Measurable product claims (each requires evidence before it is stated)

None of these may be asserted until the listed evidence exists:

- "Voice plus persistent adaptive visuals beats text answers for browser tasks."
  Evidence: end-to-end fixture task-completion and user-interruption counts
  (`docs/quality/seoul-end-to-end-tests.md`).
- "Typed verified execution is more reliable than free-form agent clicking."
  Evidence: operator-evaluation verified-completion and recovery rates
  (`docs/quality/seoul-operator-evaluation.md`).
- "Deterministic/local/cloud routing lowers cost and data exposure."
  Evidence: unnecessary-cloud-call rate and per-task cost from the router under
  evaluation.
- "Adaptive UI never fabricates data visuals." Evidence: adaptive-UI conformance
  and malicious-payload-rejection results (`docs/quality/seoul-adaptive-ui-tests.md`).

## Evidence required before making any comparative claim

1. A compiled, runnable build with current commands and results recorded in the
   readiness report.
2. The operator-evaluation and adaptive-UI test suites run with recorded
   numbers.
3. End-to-end fixtures executed as browser tests on a capable host.
4. Fresh re-verification of competitor behavior at the moment of comparison, as
   those products change monthly.
