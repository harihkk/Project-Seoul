// Project Seoul product runtime: the task service.

#include "seoul/browser/product/task_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "seoul/browser/tools/tool_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

SchemaField StringField(const std::string& name, bool required) {
  SchemaField field;
  field.name = name;
  field.kind = SchemaFieldKind::kString;
  field.required = required;
  return field;
}

ToolDescriptor Descriptor(const std::string& id,
                          RiskCategory risk = RiskCategory::kReadOnly) {
  ToolDescriptor descriptor;
  descriptor.id = ToolId::FromString(id);
  descriptor.name = id;
  descriptor.description = "reads the fixture inventory records";
  descriptor.provider = "seoul";
  descriptor.risk = risk;
  descriptor.input_schema.fields.push_back(StringField("query", true));
  descriptor.sensitivity = DataSensitivity::kOrganization;
  descriptor.timeout = base::Seconds(5);
  return descriptor;
}

ToolPermissionContext AllowAll() {
  ToolPermissionContext context;
  context.max_sensitivity = DataSensitivity::kCredentialAdjacent;
  context.allow_network = true;
  return context;
}

// A controllable executor: records requests and completes on demand.
class ManualExecutor : public CapabilityExecutor {
 public:
  ManualExecutor(const std::string& id, StepStatus result_status)
      : id_(ToolId::FromString(id)), result_status_(result_status) {}

  ToolId capability_id() const override { return id_; }

  void Execute(CapabilityRequest request,
               CapabilityCallback callback) override {
    ++executions_;
    last_args_ = request.args.Clone();
    if (complete_synchronously_) {
      CapabilityOutcome outcome;
      outcome.step.status = result_status_;
      outcome.step.observed_summary = "fixture executed";
      outcome.step.verification.verified =
          result_status_ == StepStatus::kSucceeded;
      outcome.step.verification.method = "postcondition";
      SemanticResult semantic;
      semantic.schema.shape = SemanticShape::kScalar;
      outcome.semantic = std::move(semantic);
      std::move(callback).Run(std::move(outcome));
      return;
    }
    pending_ = std::move(callback);
  }

  void Cancel(const TaskId& task_id, const std::string& step_id) override {
    ++cancels_;
  }

  void CompletePending(StepStatus status) {
    CapabilityOutcome outcome;
    outcome.step.status = status;
    std::move(pending_).Run(std::move(outcome));
  }

  int executions() const { return executions_; }
  int cancels() const { return cancels_; }
  const base::DictValue& last_args() const { return last_args_; }
  void set_synchronous(bool synchronous) {
    complete_synchronously_ = synchronous;
  }
  bool has_pending() const { return !pending_.is_null(); }

 private:
  ToolId id_;
  StepStatus result_status_;
  bool complete_synchronously_ = true;
  int executions_ = 0;
  int cancels_ = 0;
  base::DictValue last_args_;
  CapabilityCallback pending_;
};

class RecordingObserver : public TaskServiceObserver {
 public:
  void OnTaskUpdated(const TaskId& task_id) override { ++updates_; }
  void OnTaskNeedsApproval(const TaskId& task_id,
                           const std::string& step_id,
                           const std::string& prompt) override {
    approval_step_ = step_id;
  }
  void OnTaskFinished(const TaskId& task_id) override { ++finished_; }

  int updates() const { return updates_; }
  int finished() const { return finished_; }
  const std::string& approval_step() const { return approval_step_; }

 private:
  int updates_ = 0;
  int finished_ = 0;
  std::string approval_step_;
};

class TaskServiceTest : public testing::Test {
 protected:
  TaskServiceTest()
      : planner_(registry_, ModelPlanRequester()),
        service_(&registry_, &executors_, &planner_, base::BindRepeating([] {
          return base::Time::UnixEpoch();
        })) {
    service_.AddObserver(&observer_);
  }

  ~TaskServiceTest() override { service_.RemoveObserver(&observer_); }

  base::test::TaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ToolRegistry registry_;
  CapabilityExecutorRegistry executors_;
  Planner planner_;
  RecordingObserver observer_;
  TaskService service_;
};

TEST_F(TaskServiceTest, RunsPlannedTaskThroughExecutorToCompletion) {
  ASSERT_TRUE(
      registry_.Register(Descriptor("info.read.inventory")).has_value());
  auto executor = std::make_unique<ManualExecutor>("info.read.inventory",
                                                   StepStatus::kSucceeded);
  ManualExecutor* raw = executor.get();
  ASSERT_TRUE(executors_.Register(std::move(executor)));

  const TaskId id = service_.StartTask("read the fixture inventory records",
                                       LiveWindowKey::FromSessionId(7),
                                       AllowAll(), /*use_model=*/false,
                                       /*prefer_local=*/true);
  ASSERT_TRUE(id.is_valid());
  environment_.RunUntilIdle();

  EXPECT_EQ(raw->executions(), 1);
  const std::optional<TaskSnapshot> snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kCompleted);
  ASSERT_EQ(snapshot->receipts.size(), 1u);
  EXPECT_EQ(snapshot->receipts[0].status, StepStatus::kSucceeded);
  EXPECT_TRUE(snapshot->receipts[0].verification.verified);
  EXPECT_TRUE(service_.FinalSemanticResult(id));
  // Exactly one terminal notification even though the executor completed
  // synchronously inside the drive loop (re-entrancy guard + idempotent
  // finish). A regression here fires OnTaskFinished N+1 times.
  EXPECT_EQ(observer_.finished(), 1);
}

TEST_F(TaskServiceTest, SynchronousMultiStepPlanDrivesOnceAndFinishesOnce) {
  ASSERT_TRUE(
      registry_.Register(Descriptor("info.read.inventory")).has_value());
  auto executor = std::make_unique<ManualExecutor>("info.read.inventory",
                                                   StepStatus::kSucceeded);
  ManualExecutor* raw = executor.get();
  ASSERT_TRUE(executors_.Register(std::move(executor)));

  // A three-step sequential plan whose executor completes synchronously. The
  // re-entrancy guard must run all three without unbounded recursion, and the
  // terminal notification fires exactly once.
  Plan plan;
  plan.goal = "read the inventory three times";
  for (int i = 1; i <= 3; ++i) {
    PlanStep step;
    step.id = "step_" + base::NumberToString(i);
    step.kind = PlanStepKind::kToolCall;
    step.tool = ToolId::FromString("info.read.inventory");
    step.args.Set("query", "all");
    if (i > 1) {
      StepGuard guard;
      guard.depends_on_step = "step_" + base::NumberToString(i - 1);
      step.guard = guard;
    }
    plan.steps.push_back(std::move(step));
  }

  const TaskId id = service_.StartTaskWithPlan(
      "read the inventory three times", std::move(plan),
      PlanOrigin::kDeterministic, LiveWindowKey::FromSessionId(7), AllowAll());
  ASSERT_TRUE(id.is_valid());
  environment_.RunUntilIdle();

  EXPECT_EQ(raw->executions(), 3);
  const std::optional<TaskSnapshot> snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kCompleted);
  EXPECT_EQ(snapshot->receipts.size(), 3u);
  EXPECT_EQ(observer_.finished(), 1);
}

TEST_F(TaskServiceTest, RiskyStepWaitsForApprovalAndRejectionSkipsIt) {
  ASSERT_TRUE(registry_
                  .Register(Descriptor("info.submit.record",
                                       RiskCategory::kExternalSideEffect))
                  .has_value());
  auto executor = std::make_unique<ManualExecutor>("info.submit.record",
                                                   StepStatus::kSucceeded);
  ManualExecutor* raw = executor.get();
  ASSERT_TRUE(executors_.Register(std::move(executor)));

  const TaskId id = service_.StartTask("submit the fixture record",
                                       LiveWindowKey::FromSessionId(7),
                                       AllowAll(), false, true);
  environment_.RunUntilIdle();

  // Nothing executed yet; the approval gate is pending.
  EXPECT_EQ(raw->executions(), 0);
  ASSERT_FALSE(observer_.approval_step().empty());
  std::optional<TaskSnapshot> snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kAwaitingApproval);

  // Rejection skips the gated step and completes the rest of the plan.
  ASSERT_TRUE(service_.Approve(id, observer_.approval_step(), false));
  environment_.RunUntilIdle();
  EXPECT_EQ(raw->executions(), 0);
  snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kCompleted);
}

TEST_F(TaskServiceTest, TimedOutMutationIsUnknownOutcomeAndNeverReplayed) {
  ASSERT_TRUE(registry_
                  .Register(Descriptor("info.change.record",
                                       RiskCategory::kReversibleMutation))
                  .has_value());
  auto executor = std::make_unique<ManualExecutor>("info.change.record",
                                                   StepStatus::kSucceeded);
  ManualExecutor* raw = executor.get();
  raw->set_synchronous(false);
  ASSERT_TRUE(executors_.Register(std::move(executor)));

  const TaskId id = service_.StartTask("change the fixture record",
                                       LiveWindowKey::FromSessionId(7),
                                       AllowAll(), false, true);
  environment_.RunUntilIdle();
  EXPECT_EQ(raw->executions(), 1);
  ASSERT_TRUE(raw->has_pending());

  // Let the per-step timeout fire: the mutation's outcome is unknown.
  environment_.FastForwardBy(base::Seconds(6));
  environment_.RunUntilIdle();

  EXPECT_EQ(raw->cancels(), 1);
  const std::optional<TaskSnapshot> snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  bool saw_unknown = false;
  for (const ActionReceipt& receipt : snapshot->receipts) {
    if (receipt.status == StepStatus::kOutcomeUnknown) {
      saw_unknown = true;
    }
  }
  EXPECT_TRUE(saw_unknown);
  // Never replayed: exactly one execution ever happened.
  EXPECT_EQ(raw->executions(), 1);
  EXPECT_EQ(snapshot->replans_used, 0);
}

TEST_F(TaskServiceTest, CancelCancelsInFlightExecutorAndFinishes) {
  ASSERT_TRUE(
      registry_.Register(Descriptor("info.read.inventory")).has_value());
  auto executor = std::make_unique<ManualExecutor>("info.read.inventory",
                                                   StepStatus::kSucceeded);
  ManualExecutor* raw = executor.get();
  raw->set_synchronous(false);
  ASSERT_TRUE(executors_.Register(std::move(executor)));

  const TaskId id = service_.StartTask("read the fixture inventory records",
                                       LiveWindowKey::FromSessionId(7),
                                       AllowAll(), false, true);
  environment_.RunUntilIdle();
  ASSERT_TRUE(raw->has_pending());

  ASSERT_TRUE(service_.Cancel(id));
  EXPECT_EQ(raw->cancels(), 1);
  const std::optional<TaskSnapshot> snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kCancelled);
}

TEST_F(TaskServiceTest, UnplannableGoalFailsHonestlyWithReason) {
  const TaskId id =
      service_.StartTask("zzqx vvwq ppl", LiveWindowKey::FromSessionId(7),
                         AllowAll(), false, true);
  environment_.RunUntilIdle();
  const std::optional<TaskSnapshot> snapshot = service_.Snapshot(id);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kCancelled);
  EXPECT_FALSE(snapshot->pending_approval_prompt.empty());
}

TEST_F(TaskServiceTest, ShutdownCancelsWithoutCallbacks) {
  ASSERT_TRUE(
      registry_.Register(Descriptor("info.read.inventory")).has_value());
  auto executor = std::make_unique<ManualExecutor>("info.read.inventory",
                                                   StepStatus::kSucceeded);
  ManualExecutor* raw = executor.get();
  raw->set_synchronous(false);
  ASSERT_TRUE(executors_.Register(std::move(executor)));
  service_.StartTask("read the fixture inventory records",
                     LiveWindowKey::FromSessionId(7), AllowAll(), false, true);
  environment_.RunUntilIdle();
  const int updates_before = observer_.updates();
  service_.Shutdown();
  // Late executor completion after shutdown must not notify anyone.
  if (raw->has_pending()) {
    raw->CompletePending(StepStatus::kSucceeded);
  }
  environment_.RunUntilIdle();
  EXPECT_EQ(observer_.updates(), updates_before);
}

}  // namespace
}  // namespace seoul
