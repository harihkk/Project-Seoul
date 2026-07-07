// Project Seoul general-purpose operator: task layer.
// Unit tests for bounded task execution: observe-verify-decide, retry rules,
// the unknown-outcome mutation rule, budgets, approval skips, checkpoints.

#include "seoul/browser/tasks/task_execution.h"

#include <map>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class TaskExecutionTest : public testing::Test {
 protected:
  TaskExecutionTest() {
    AddTool("page.observe.text", RiskCategory::kReadOnly,
            IdempotencyClass::kIdempotent);
    AddTool("browser.tabs.open", RiskCategory::kReversibleMutation,
            IdempotencyClass::kNotIdempotent);
    AddTool("page.form.submit", RiskCategory::kIrreversibleMutation,
            IdempotencyClass::kNotIdempotent);
  }

  void AddTool(const std::string& id,
               RiskCategory risk,
               IdempotencyClass idempotency) {
    ToolDescriptor descriptor;
    descriptor.id = ToolId::FromString(id);
    descriptor.name = id;
    descriptor.description = "test tool";
    descriptor.provider = "seoul";
    descriptor.risk = risk;
    descriptor.idempotency = idempotency;
    tools_[descriptor.id.value()] = descriptor;
  }

  ToolResolver Resolver() {
    return base::BindLambdaForTesting(
        [this](const ToolId& id) -> const ToolDescriptor* {
          auto it = tools_.find(id.value());
          return it == tools_.end() ? nullptr : &it->second;
        });
  }

  base::RepeatingCallback<base::Time()> Clock() {
    return base::BindLambdaForTesting([this]() { return clock_; });
  }

  PlanStep ToolStep(const std::string& id, const std::string& tool) {
    PlanStep step;
    step.id = id;
    step.kind = PlanStepKind::kToolCall;
    step.tool = ToolId::FromString(tool);
    step.args.Set("query", id);
    return step;
  }

  StepOutcome Success(const std::string& summary) {
    StepOutcome outcome;
    outcome.status = StepStatus::kSucceeded;
    outcome.observed_summary = summary;
    outcome.verification.verified = true;
    outcome.verification.method = "postcondition";
    return outcome;
  }

  std::unique_ptr<TaskExecution> Execute(Plan plan) {
    return std::make_unique<TaskExecution>(
        TaskId::GenerateNew(), std::move(plan), Resolver(), Clock());
  }

  std::map<std::string, ToolDescriptor> tools_;
  base::Time clock_ = base::Time::UnixEpoch() + base::Days(20000);
};

TEST_F(TaskExecutionTest, HappyPathRunsToCompletionWithReceipts) {
  Plan plan;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  auto execution = Execute(std::move(plan));

  NextAction action = execution->Start();
  ASSERT_EQ(action.kind, NextAction::Kind::kRunStep);
  EXPECT_EQ(action.step_id, "observe");
  ASSERT_TRUE(execution->BeginStep("observe"));
  EXPECT_EQ(execution->RecordStepOutcome("observe", Success("read the page")),
            ExecutionDecision::kContinue);

  action = execution->Advance();
  ASSERT_EQ(action.kind, NextAction::Kind::kRunStep);
  EXPECT_EQ(action.step_id, "open");
  ASSERT_TRUE(execution->BeginStep("open"));
  EXPECT_EQ(execution->RecordStepOutcome("open", Success("tab inserted")),
            ExecutionDecision::kContinue);

  EXPECT_EQ(execution->state(), TaskState::kCompleted);
  ASSERT_EQ(execution->receipts().size(), 2u);
  EXPECT_EQ(execution->receipts()[1].observed_summary, "tab inserted");
  EXPECT_TRUE(execution->receipts()[1].verification.verified);
}

TEST_F(TaskExecutionTest, UnknownOutcomeMutationIsNeverAutoRetried) {
  Plan plan;
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  auto execution = Execute(std::move(plan));
  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("open"));

  StepOutcome unknown;
  unknown.status = StepStatus::kOutcomeUnknown;
  unknown.observed_summary = "no completion observed before timeout";
  EXPECT_EQ(execution->RecordStepOutcome("open", unknown),
            ExecutionDecision::kAskUser);
  EXPECT_EQ(execution->state(), TaskState::kPaused);
  EXPECT_EQ(execution->step_status("open"), StepStatus::kOutcomeUnknown);
}

TEST_F(TaskExecutionTest, ReadOnlyUnknownOutcomeMayRetry) {
  Plan plan;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  auto execution = Execute(std::move(plan));
  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("observe"));

  StepOutcome unknown;
  unknown.status = StepStatus::kOutcomeUnknown;
  EXPECT_EQ(execution->RecordStepOutcome("observe", unknown),
            ExecutionDecision::kRetryStep);
  EXPECT_EQ(execution->step_status("observe"), StepStatus::kPending);
}

TEST_F(TaskExecutionTest, FailedVerificationCountsAsFailure) {
  Plan plan;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  auto execution = Execute(std::move(plan));
  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("observe"));

  StepOutcome claimed_success;
  claimed_success.status = StepStatus::kSucceeded;
  claimed_success.verification.method = "postcondition";
  claimed_success.verification.verified = false;  // dispatch lied
  EXPECT_EQ(execution->RecordStepOutcome("observe", claimed_success),
            ExecutionDecision::kRetryStep);
  EXPECT_EQ(execution->step_status("observe"), StepStatus::kPending);
}

TEST_F(TaskExecutionTest, RetriesAreBoundedThenReplanThenAskUser) {
  Plan plan;
  plan.budgets.max_retries_per_step = 1;
  plan.budgets.max_replans = 1;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  auto execution = Execute(std::move(plan));
  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);

  StepOutcome failed;
  failed.status = StepStatus::kFailed;

  ASSERT_TRUE(execution->BeginStep("observe"));
  EXPECT_EQ(execution->RecordStepOutcome("observe", failed),
            ExecutionDecision::kRetryStep);
  ASSERT_TRUE(execution->BeginStep("observe"));
  EXPECT_EQ(execution->RecordStepOutcome("observe", failed),
            ExecutionDecision::kReplan);
  EXPECT_EQ(execution->usage().replans_used, 1);

  // Retries and replans exhausted: the user decides.
  ASSERT_EQ(execution->ResumeFromPause().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("observe"));
  EXPECT_EQ(execution->RecordStepOutcome("observe", failed),
            ExecutionDecision::kAskUser);
  EXPECT_EQ(execution->state(), TaskState::kPaused);
}

TEST_F(TaskExecutionTest, ApprovalRejectionSkipsTheStepAndCompletes) {
  Plan plan;
  plan.steps.push_back(ToolStep("fill", "page.observe.text"));
  PlanStep submit = ToolStep("submit", "page.form.submit");
  submit.requires_approval = true;
  plan.steps.push_back(submit);
  auto execution = Execute(std::move(plan));

  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("fill"));
  ASSERT_EQ(execution->RecordStepOutcome("fill", Success("form filled")),
            ExecutionDecision::kContinue);

  NextAction action = execution->Advance();
  ASSERT_EQ(action.kind, NextAction::Kind::kAwaitApproval);
  EXPECT_EQ(action.step_id, "submit");

  // "Fill the form but stop before submitting": rejection completes the
  // task with the submit step skipped, not failed.
  action = execution->RecordApproval("submit", /*approved=*/false);
  EXPECT_EQ(action.kind, NextAction::Kind::kCompleted);
  EXPECT_EQ(execution->state(), TaskState::kCompleted);
  EXPECT_EQ(execution->step_status("submit"), StepStatus::kSkipped);
}

TEST_F(TaskExecutionTest, ApprovedStepRunsAfterApproval) {
  Plan plan;
  PlanStep submit = ToolStep("submit", "page.form.submit");
  submit.requires_approval = true;
  plan.steps.push_back(submit);
  auto execution = Execute(std::move(plan));

  NextAction action = execution->Start();
  ASSERT_EQ(action.kind, NextAction::Kind::kAwaitApproval);
  EXPECT_FALSE(execution->BeginStep("submit"));  // not before approval
  action = execution->RecordApproval("submit", /*approved=*/true);
  ASSERT_EQ(action.kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("submit"));
  EXPECT_EQ(execution->RecordStepOutcome("submit", Success("submitted")),
            ExecutionDecision::kContinue);
  EXPECT_EQ(execution->state(), TaskState::kCompleted);
}

TEST_F(TaskExecutionTest, GuardsSkipStepsWhoseDependencyFailed) {
  Plan plan;
  plan.budgets.max_retries_per_step = 0;
  plan.budgets.max_replans = 0;
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  PlanStep dependent = ToolStep("observe", "page.observe.text");
  StepGuard guard;
  guard.depends_on_step = "open";
  guard.require_success = true;
  dependent.guard = guard;
  plan.steps.push_back(dependent);
  PlanStep fallback = ToolStep("fallback", "page.observe.text");
  StepGuard failure_guard;
  failure_guard.depends_on_step = "open";
  failure_guard.require_success = false;
  fallback.guard = failure_guard;
  plan.steps.push_back(fallback);
  auto execution = Execute(std::move(plan));

  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("open"));
  StepOutcome failed;
  failed.status = StepStatus::kFailed;
  EXPECT_EQ(execution->RecordStepOutcome("open", failed),
            ExecutionDecision::kAskUser);
  ASSERT_EQ(execution->ResumeFromPause().kind, NextAction::Kind::kRunStep);

  // The success-guarded step is skipped; the failure-guarded step runs.
  EXPECT_EQ(execution->step_status("observe"), StepStatus::kSkipped);
  ASSERT_TRUE(execution->BeginStep("fallback"));
  EXPECT_EQ(execution->RecordStepOutcome("fallback", Success("recovered")),
            ExecutionDecision::kContinue);
  EXPECT_EQ(execution->state(), TaskState::kCompleted);
}

TEST_F(TaskExecutionTest, BudgetExhaustionStopsExecution) {
  Plan plan;
  plan.budgets.max_model_calls = 1;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  auto execution = Execute(std::move(plan));

  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("observe"));
  StepOutcome expensive = Success("used the model twice");
  expensive.model_calls = 2;
  EXPECT_EQ(execution->RecordStepOutcome("observe", expensive),
            ExecutionDecision::kStop);
  EXPECT_EQ(execution->state(), TaskState::kFailed);
  EXPECT_EQ(execution->failure_reason(), TaskFailureReason::kBudgetExhausted);
}

TEST_F(TaskExecutionTest, DurationBudgetIsEnforced) {
  Plan plan;
  plan.budgets.max_duration = base::Minutes(1);
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  auto execution = Execute(std::move(plan));
  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution->BeginStep("observe"));
  clock_ += base::Minutes(5);
  EXPECT_EQ(execution->RecordStepOutcome("observe", Success("slow")),
            ExecutionDecision::kStop);
  EXPECT_EQ(execution->failure_reason(), TaskFailureReason::kBudgetExhausted);
}

TEST_F(TaskExecutionTest, BoundedLoopIterates) {
  Plan plan;
  PlanStep poll = ToolStep("poll", "page.observe.text");
  poll.loop_group = 1;
  poll.max_iterations = 3;
  plan.steps.push_back(poll);
  auto execution = Execute(std::move(plan));

  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(execution->BeginStep("poll")) << "iteration " << i;
    ASSERT_EQ(execution->RecordStepOutcome("poll", Success("observed")),
              ExecutionDecision::kContinue);
    execution->Advance();
  }
  EXPECT_EQ(execution->state(), TaskState::kCompleted);
  EXPECT_EQ(execution->usage().steps_executed, 3);
}

TEST_F(TaskExecutionTest, LongLoopIsNotKilledByRepeatActionGuard) {
  Plan plan;
  // The loop runs the same tool+args far more than max_repeat_actions (5); a
  // bounded loop is capped by its own max_iterations, not the repeat guard.
  plan.budgets.max_repeat_actions = 5;
  PlanStep poll = ToolStep("poll", "page.observe.text");
  poll.loop_group = 1;
  poll.max_iterations = 10;
  plan.steps.push_back(poll);
  auto execution = Execute(std::move(plan));

  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(execution->BeginStep("poll")) << "iteration " << i;
    ASSERT_EQ(execution->RecordStepOutcome("poll", Success("observed")),
              ExecutionDecision::kContinue)
        << "iteration " << i;
    execution->Advance();
  }
  EXPECT_EQ(execution->state(), TaskState::kCompleted);
  EXPECT_EQ(execution->usage().steps_executed, 10);
}

TEST_F(TaskExecutionTest, CancelMarksPendingStepsCancelled) {
  Plan plan;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  auto execution = Execute(std::move(plan));
  ASSERT_EQ(execution->Start().kind, NextAction::Kind::kRunStep);
  NextAction action = execution->Cancel();
  EXPECT_EQ(action.kind, NextAction::Kind::kStopped);
  EXPECT_EQ(execution->state(), TaskState::kCancelled);
  EXPECT_EQ(execution->step_status("open"), StepStatus::kCancelled);
}

TEST_F(TaskExecutionTest, CheckpointRestoresAndProtectsMidRunSteps) {
  Plan plan;
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  const TaskId task_id = TaskId::GenerateNew();
  TaskExecution execution(task_id, plan, Resolver(), Clock());
  ASSERT_EQ(execution.Start().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution.BeginStep("observe"));
  ASSERT_EQ(execution.RecordStepOutcome("observe", Success("done")),
            ExecutionDecision::kContinue);
  ASSERT_EQ(execution.Advance().kind, NextAction::Kind::kRunStep);
  ASSERT_TRUE(execution.BeginStep("open"));  // browser dies mid-mutation

  base::DictValue checkpoint = execution.Checkpoint();
  auto restored = TaskExecution::RestoreFromCheckpoint(
      task_id, plan, Resolver(), Clock(), checkpoint);
  ASSERT_NE(restored, nullptr);
  EXPECT_EQ(restored->step_status("observe"), StepStatus::kSucceeded);
  // The step that was running is restored as outcome-unknown, so a restart
  // can never silently repeat a mutation.
  EXPECT_EQ(restored->step_status("open"), StepStatus::kOutcomeUnknown);
  EXPECT_EQ(restored->usage().steps_executed, 2);

  auto mismatched = TaskExecution::RestoreFromCheckpoint(
      TaskId::GenerateNew(), plan, Resolver(), Clock(), checkpoint);
  EXPECT_EQ(mismatched, nullptr);
}

}  // namespace
}  // namespace seoul
