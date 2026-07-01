# Seoul General-Operator Evaluation Plan

Status: Source complete; not compiled or runtime-verified on the authoring host.

This is the evaluation plan for Seoul's general-purpose operator (the task layer
in `native/seoul/browser/tasks/` and the tool layer in
`native/seoul/browser/tools/`). It defines conditions and metrics. It is a plan:
no measured numbers exist yet, because nothing is compiled. An 8 GiB host cannot
build Chromium, so the runnable form of this suite awaits a capable build host.

## Principle: no held-out goals in the planner

The operator must generalize, so held-out evaluation goals must not be coded into
the planner. The task layer is built to make this checkable: tools are looked up
by namespaced `ToolId` in the `ToolRegistry`, an unknown id returns null and must
be treated as a rejected plan step (`plan_validator.cc`, `PlanError::kUnknownTool`),
and there is no fallback dispatch and no step kind that runs arbitrary code
(`plan_types.h`). Evaluation goals live only in fixtures, never in source.

## Evaluation conditions

Each condition exercises a specific behavior in the source:

- Unfamiliar wording: a goal phrased unlike any fixture still yields a
  schema-valid plan.
- New tool combinations: tools composed in an order no fixture used.
- Missing tool: a needed capability is absent; the plan must not fabricate a tool
  (`kUnknownTool`/`kToolNotPermitted`).
- Changed page: a semantic target no longer resolves; execution reports
  `TaskFailureReason::kAssumptionInvalid` rather than acting blindly.
- Ambiguous goal: the plan collects input through a `kUserInput` step or an
  approval gate rather than guessing.
- Interrupted task: voice barge-in or pause leaves task state intact (the voice
  session never clears the active task).
- Failed step: a plain failure retries only when the tool is read-only or
  idempotent, else replans, else asks the user.
- Unknown outcome: a non-read-only step with an unknown outcome pauses and asks
  the user and is never auto-retried.
- Connector unavailable: a `connector.<provider>.*` tool is not permitted because
  the provider is not connected.
- Local insufficient / cloud disabled: the reasoning router yields the correct
  deterministic/local/cloud route or `kUnavailable` with a specific reason.

## Metrics

The plan will measure, per condition: valid-plan rate (passes `ValidatePlan`),
valid-schema rate (tool args pass `ValidateArgs`), correct tool selection,
execution success, recovery success (correct retry/replan/ask-user decision),
verified completion (steps whose verification actually confirmed the outcome),
unnecessary cloud calls (cloud used where deterministic or local would qualify),
latency, memory, cost in microdollars, and user interruptions required. These are
target metrics only; no values are recorded because the suite is not yet
runnable.

## Units already exercising these pieces

Authored unit tests already cover parts of the operator (they compile and run
only on a capable host, but they are written):

- `native/seoul/browser/tasks/plan_validator_unittest.cc`:
  `AcceptsBoundedSequentialPlan`, `RejectsUnknownAndUnpermittedTools`,
  `RejectsInvalidArgsAndDuplicateIds`, `IrreversibleStepsNeedAnApprovalPath`,
  `ParallelGroupsMustBeReadOnly`, `LoopsMustBeBounded`,
  `GuardsMustReferenceEarlierSteps`, `EmptyPlanBudgetsAndStepCountAreChecked`.
- `native/seoul/browser/tasks/task_execution_unittest.cc`:
  `HappyPathRunsToCompletionWithReceipts`,
  `UnknownOutcomeMutationIsNeverAutoRetried`, `ReadOnlyUnknownOutcomeMayRetry`,
  `FailedVerificationCountsAsFailure`, `RetriesAreBoundedThenReplanThenAskUser`,
  `ApprovalRejectionSkipsTheStepAndCompletes`, `ApprovedStepRunsAfterApproval`,
  `GuardsSkipStepsWhoseDependencyFailed`, `BudgetExhaustionStopsExecution`,
  `DurationBudgetIsEnforced`, `BoundedLoopIterates`,
  `LongLoopIsNotKilledByRepeatActionGuard`, `CancelMarksPendingStepsCancelled`,
  `CheckpointRestoresAndProtectsMidRunSteps`.
- `native/seoul/browser/tools/tool_registry_unittest.cc`: `ValidatesShape`,
  the `ToolSchema` arg-validation tests, `RegisterFindAndDuplicate`,
  `ReservedNamespacesAndConnectorOwnership`,
  `ListAvailableFiltersBySensitivityNetworkProvider`, and
  `InvalidDescriptorsAreRejected`.

These validate plan-shape, execution decisions, and tool ownership at the unit
level. The end-to-end operator metrics above require a browser-test build and
remain unmeasured.
