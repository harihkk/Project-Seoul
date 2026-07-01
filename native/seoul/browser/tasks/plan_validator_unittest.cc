// Project Seoul general-purpose operator: task layer.
// Unit tests for plan validation against the registry and permissions.

#include "seoul/browser/tasks/plan_validator.h"

#include "seoul/browser/tools/tool_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

ToolDescriptor MakeTool(const std::string& id,
                        RiskCategory risk,
                        DataSensitivity sensitivity) {
  ToolDescriptor descriptor;
  descriptor.id = ToolId::FromString(id);
  descriptor.name = "Tool " + id;
  descriptor.description = "Deterministic test tool.";
  descriptor.provider = "seoul";
  SchemaField query;
  query.name = "query";
  query.kind = SchemaFieldKind::kString;
  query.required = true;
  descriptor.input_schema.fields.push_back(query);
  descriptor.risk = risk;
  descriptor.sensitivity = sensitivity;
  descriptor.observation_contract = "observable result";
  return descriptor;
}

class PlanValidatorTest : public testing::Test {
 protected:
  PlanValidatorTest() {
    CHECK(registry_
              .Register(MakeTool("page.observe.text", RiskCategory::kReadOnly,
                                 DataSensitivity::kPageContent))
              .has_value());
    CHECK(registry_
              .Register(MakeTool("browser.tabs.open",
                                 RiskCategory::kReversibleMutation,
                                 DataSensitivity::kOrganization))
              .has_value());
    CHECK(registry_
              .Register(MakeTool("page.form.submit",
                                 RiskCategory::kIrreversibleMutation,
                                 DataSensitivity::kCredentialAdjacent))
              .has_value());
    context_.max_sensitivity = DataSensitivity::kCredentialAdjacent;
    context_.allow_network = false;
  }

  PlanStep ToolStep(const std::string& id, const std::string& tool) {
    PlanStep step;
    step.id = id;
    step.kind = PlanStepKind::kToolCall;
    step.tool = ToolId::FromString(tool);
    step.args.Set("query", "x");
    return step;
  }

  ToolRegistry registry_;
  ToolPermissionContext context_;
};

TEST_F(PlanValidatorTest, AcceptsBoundedSequentialPlan) {
  Plan plan;
  plan.goal = "observe then open";
  plan.steps.push_back(ToolStep("observe", "page.observe.text"));
  plan.steps.push_back(ToolStep("open", "browser.tabs.open"));
  EXPECT_TRUE(ValidatePlan(plan, registry_, context_).has_value());
}

TEST_F(PlanValidatorTest, RejectsUnknownAndUnpermittedTools) {
  Plan plan;
  plan.steps.push_back(ToolStep("bad", "info.invented.tool"));
  auto result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kUnknownTool);
  EXPECT_EQ(result.error().step_id, "bad");

  ToolPermissionContext restricted;
  restricted.max_sensitivity = DataSensitivity::kOrganization;
  Plan sensitive;
  sensitive.steps.push_back(ToolStep("observe", "page.observe.text"));
  result = ValidatePlan(sensitive, registry_, restricted);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kToolNotPermitted);
}

TEST_F(PlanValidatorTest, RejectsInvalidArgsAndDuplicateIds) {
  Plan plan;
  PlanStep step = ToolStep("observe", "page.observe.text");
  step.args.clear();
  step.args.Set("not_declared", true);
  plan.steps.push_back(step);
  auto result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kArgsInvalid);

  Plan duplicate;
  duplicate.steps.push_back(ToolStep("same", "page.observe.text"));
  duplicate.steps.push_back(ToolStep("same", "page.observe.text"));
  result = ValidatePlan(duplicate, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kDuplicateStepId);
}

TEST_F(PlanValidatorTest, IrreversibleStepsNeedAnApprovalPath) {
  Plan plan;
  plan.steps.push_back(ToolStep("fill", "page.observe.text"));
  plan.steps.push_back(ToolStep("submit", "page.form.submit"));
  auto result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kMissingApprovalGate);
  EXPECT_EQ(result.error().step_id, "submit");

  // Either a per-step approval flag...
  plan.steps[1].requires_approval = true;
  EXPECT_TRUE(ValidatePlan(plan, registry_, context_).has_value());

  // ...or a preceding approval gate satisfies the requirement.
  plan.steps[1].requires_approval = false;
  PlanStep gate;
  gate.id = "confirm";
  gate.kind = PlanStepKind::kApprovalGate;
  gate.prompt = "Submit the application?";
  plan.steps.insert(plan.steps.begin() + 1, gate);
  EXPECT_TRUE(ValidatePlan(plan, registry_, context_).has_value());
}

TEST_F(PlanValidatorTest, ParallelGroupsMustBeReadOnly) {
  Plan plan;
  PlanStep a = ToolStep("read-a", "page.observe.text");
  a.parallel_group = 1;
  PlanStep b = ToolStep("open-b", "browser.tabs.open");
  b.parallel_group = 1;
  plan.steps = {a, b};
  auto result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kParallelMutation);
  EXPECT_EQ(result.error().step_id, "open-b");
}

TEST_F(PlanValidatorTest, LoopsMustBeBounded) {
  Plan plan;
  PlanStep step = ToolStep("poll", "page.observe.text");
  step.loop_group = 1;
  step.max_iterations = 0;
  plan.steps.push_back(step);
  auto result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kLoopUnbounded);

  plan.steps[0].max_iterations = kMaxLoopIterations + 1;
  result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kLoopTooLarge);

  plan.steps[0].max_iterations = 5;
  EXPECT_TRUE(ValidatePlan(plan, registry_, context_).has_value());
}

TEST_F(PlanValidatorTest, GuardsMustReferenceEarlierSteps) {
  Plan plan;
  PlanStep first = ToolStep("first", "page.observe.text");
  StepGuard guard;
  guard.depends_on_step = "second";
  first.guard = guard;
  PlanStep second = ToolStep("second", "page.observe.text");
  plan.steps = {first, second};
  auto result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kGuardForwardReference);

  plan.steps[0].guard->depends_on_step = "never_existed";
  result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kGuardUnknownStep);
}

TEST_F(PlanValidatorTest, EmptyPlanBudgetsAndStepCountAreChecked) {
  Plan empty;
  auto result = ValidatePlan(empty, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kEmptyPlan);

  Plan plan;
  plan.steps.push_back(ToolStep("only", "page.observe.text"));
  plan.budgets.max_duration = base::TimeDelta();
  result = ValidatePlan(plan, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kInvalidBudgets);

  Plan too_long;
  too_long.budgets.max_steps = 2;
  for (int i = 0; i < 3; ++i) {
    too_long.steps.push_back(
        ToolStep("step-" + std::to_string(i), "page.observe.text"));
  }
  result = ValidatePlan(too_long, registry_, context_);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, PlanError::kTooManySteps);
}

}  // namespace
}  // namespace seoul
