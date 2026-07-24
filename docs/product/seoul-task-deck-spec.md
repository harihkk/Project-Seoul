# Seoul Task Deck and Operator Specification

Status: Current compile and runtime evidence is maintained in the product
readiness report.

The task layer is Seoul's general-purpose operator: a tool registry, typed plans
with validation, a bounded execution state machine, action receipts, and a Task
Deck that makes every task visible. This spec describes the source under
`native/seoul/browser/tasks/` and `native/seoul/browser/tools/`.

## Tool Registry

`native/seoul/browser/tools/tool_types.h` defines `ToolId` (a dotted namespaced
identifier of two to four `[a-z][a-z0-9_]*` segments) and `ToolDescriptor`
(typed input/output `ToolSchema`, `provider`, `DataSensitivity`, `RiskCategory`,
`ApprovalPolicy`, `IdempotencyClass`, an `observation_contract`, an optional
`verifier_id`, and a `preferred_component`).

`native/seoul/browser/tools/tool_registry.cc` enforces namespace ownership on
`Register`: Seoul-owned roots ("browser", "page", "info", "canvas", "files",
"workflow", "task", "scene") accept only provider "seoul"; connector descriptors
must live under `connector.<provider>.*`. Discovery is permission-scoped:
`ListAvailable(ToolPermissionContext)` returns an id-ordered list filtered by
`max_sensitivity`, `allow_network`, and `connected_providers`. `Find` returns
null for an unknown id, and the header states the caller must treat that as a
rejected plan step, never a soft failure; there is no fallback dispatch.

## Typed Plan and validation

`native/seoul/browser/tasks/plan_types.h` defines `Plan` (goal, ordered
`PlanStep` list, `TaskBudgets`). A step is `kToolCall`, `kApprovalGate`, or
`kUserInput`, with an optional `StepGuard`, a `parallel_group`, and a
`loop_group` with `max_iterations`. There is deliberately no step kind that runs
shell commands or generated code.

`ValidatePlan` in `plan_validator.cc` checks, against the registry and a
permission context: non-empty plan and valid budgets; the step count within
`max_steps`; unique, well-formed step ids; guards that reference an earlier step
(distinguishing `kGuardForwardReference` from `kGuardUnknownStep`); bounded loops
(`kLoopUnbounded`, `kLoopTooLarge` past `kMaxLoopIterations` of 25); tool
resolvability and permission (`kUnknownTool`, `kToolNotPermitted`);
schema-valid args; read-only parallelism (a non-read-only tool in a parallel
group is `kParallelMutation`); and approval gating. `NeedsApproval` is true for
first-use-per-scope or always-required approval, irreversible mutations, and
external side effects; such a step must carry `requires_approval`, else
`kMissingApprovalGate`. A generic earlier approval gate cannot substitute for
the exact tool step because it is not bound to the live execution scope.

Validation proves that a gate exists; execution proves that its scope is exact.
`AgentPermissionService` matches a reusable grant only when capability, live
window, live tab, main frame, committed source origin, optional destination
origin, and connector service scope all match. Grants expire, are revoked with
their tab/window lifecycle, and are never reused for irreversible or external
side effects. Invalid or unresolved page scopes fail closed.

## Execution state machine

`native/seoul/browser/tasks/task_execution.cc` runs one already-validated plan
through observe-verify-decide. `Start` moves draft to executing and stamps the
duration clock. `Advance` returns the next `NextAction` (`kRunStep`,
`kAwaitApproval`, `kAwaitInput`, `kCompleted`, `kStopped`) given guards, loops,
approvals, and prior outcomes. `BeginStep` marks a tool step running and counts
usage and a per-`tool+args` repeat key. `RecordStepOutcome` records observation
and verification and returns an `ExecutionDecision`.

Key rules in the source:

- Verification gate: a reported success whose explicit verification method is
  present but not verified is downgraded to failed. Seoul confirms actual
  results rather than trusting dispatch.
- Unknown-outcome mutation never auto-retried: on `kOutcomeUnknown`, a
  non-read-only step pauses the task and returns `kAskUser`, because retrying
  could duplicate the mutation and only the user can decide. A read-only unknown
  outcome may retry within `max_retries_per_step`.
- Retry, replan, ask-user: a plain failure retries only when the tool is
  read-only or idempotent and retries remain; otherwise a bounded `kReplan`
  preserves receipts and asks the configured planner for a fresh validated
  plan using the original local/cloud preference (up to `max_replans`); it does
  not silently downgrade model-backed reasoning to lexical matching. When
  replans are exhausted the task pauses and asks the user.
- Budgets: `BudgetsExceeded` stops the task when steps, model calls,
  navigations, cloud cost, duration, or identical-action repeats exceed the
  `TaskBudgets` ceilings; execution never silently continues.
- Approval semantics: a rejected `kApprovalGate` is a stop boundary that skips
  everything after it while keeping completed work; a rejected `requires_approval`
  step is skipped and the task continues. This is how "prepare but do not
  submit" completes the rest of a plan.
- User-input semantics: a `kUserInput` step exposes a bounded typed input form
  tied to that exact task and step. The accepted value is recorded as a receipt
  and appended to bounded planning context, then the configured planner builds
  a fresh validated execution. Input is never applied to another step or
  treated as an implicit capability result.
- Checkpoints: `Checkpoint` serializes step statuses, retries, loop iterations,
  and usage. `RestoreFromCheckpoint` continues a task after a browser restart
  with the same plan and converts any step that was mid-run into
  `kOutcomeUnknown` so it does not silently re-run. Because permission grants
  are session-memory only, an approved-but-still-pending tool is restored to an
  approval wait and must resolve/confirm its current scope again.

## ActionReceipt

Each executed step appends an `ActionReceipt` (`task_types.h`) recording the
step id and tool, `StepStatus`, start and finish times, an `observed_summary`, a
`VerificationRecord` (verified flag, method, detail), the `ExecutionRoute`
(deterministic, local, or cloud), cost in microdollars, model-call count, and
navigation count. Receipts are append-only and bounded by `kMaxReceiptsPerTask`
(256).

## Task Deck model

`native/seoul/browser/tasks/task_deck_model.cc` holds a `TaskRecord` per task
(state, failure reason, dominant route, total cost, receipts, optional
`workflow_id` and `scene_id`). `SetState` allows only legal transitions per
`IsLegalTaskTransition`; terminal states (`kCompleted`, `kFailed`, `kCancelled`)
accept none. Filters expose `Active`, `Monitoring`, and `Finished` sets, so the
header's "no hidden automation" holds: background workflows and monitors appear
alongside interactive tasks. When the deck is full (`kMaxTasksInDeck`, 500), it
evicts the oldest finished task and never an active one; if every slot is active,
`Add` fails rather than dropping a running task.
