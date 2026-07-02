// Project Seoul product runtime: task-to-surface bridge.

#include "seoul/browser/product/task_surface_bridge.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "seoul/browser/product/capability_executor.h"
#include "seoul/browser/product/planner.h"
#include "seoul/browser/product/surface_service.h"
#include "seoul/browser/product/task_service.h"
#include "seoul/browser/tools/tool_registry.h"
#include "seoul/browser/tools/tool_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

FieldSpec Field(const std::string& id, SemanticRole role) {
  FieldSpec f;
  f.id = id;
  f.label = id;
  f.primitive = FieldPrimitive::kString;
  f.role = role;
  f.nullable = false;
  return f;
}

SemanticResult UnseenCollection(int rows) {
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {Field("orchard", SemanticRole::kIdentifier),
                          Field("cultivar", SemanticRole::kCategory)};
  base::Value::List data;
  for (int i = 0; i < rows; ++i) {
    base::Value::Dict row;
    row.Set("orchard", "o" + base::NumberToString(i));
    row.Set("cultivar", i % 2 ? "opal" : "jonagold");
    data.Append(std::move(row));
  }
  result.data = base::Value(std::move(data));
  result.provenance.base.source_name = "info.survey";
  result.provenance.base.retrieved_at = base::Time::UnixEpoch();
  result.provenance.base.effective_at = base::Time::UnixEpoch();
  return result;
}

// An executor whose output row count is controllable (to simulate streaming
// updates) and which can complete synchronously or on demand.
class SurveyExecutor : public CapabilityExecutor {
 public:
  ToolId capability_id() const override {
    return ToolId::FromString("info.survey.readings");
  }
  void Execute(CapabilityRequest request,
               CapabilityCallback callback) override {
    CapabilityOutcome outcome;
    outcome.step.status = StepStatus::kSucceeded;
    outcome.step.verification.verified = true;
    outcome.step.verification.method = "postcondition";
    outcome.semantic = UnseenCollection(rows_);
    std::move(callback).Run(std::move(outcome));
  }
  void set_rows(int rows) { rows_ = rows; }

 private:
  int rows_ = 2;
};

ToolDescriptor SurveyDescriptor() {
  ToolDescriptor descriptor;
  descriptor.id = ToolId::FromString("info.survey.readings");
  descriptor.name = "survey";
  descriptor.description = "reads the substrate survey readings from the log";
  descriptor.provider = "seoul";
  SchemaField query;
  query.name = "query";
  query.kind = SchemaFieldKind::kString;
  query.required = true;
  descriptor.input_schema.fields.push_back(std::move(query));
  descriptor.sensitivity = DataSensitivity::kOrganization;
  return descriptor;
}

ToolPermissionContext AllowAll() {
  ToolPermissionContext context;
  context.max_sensitivity = DataSensitivity::kCredentialAdjacent;
  context.allow_network = true;
  return context;
}

class TaskSurfaceBridgeTest : public testing::Test {
 protected:
  TaskSurfaceBridgeTest()
      : planner_(registry_, ModelPlanRequester()),
        tasks_(&registry_, &executors_, &planner_, base::BindRepeating([] {
          return base::Time::UnixEpoch();
        })),
        bridge_(&tasks_, &surfaces_) {}

  base::test::TaskEnvironment environment_;
  ToolRegistry registry_;
  CapabilityExecutorRegistry executors_;
  Planner planner_;
  SurfaceService surfaces_;
  TaskService tasks_;
  TaskSurfaceBridge bridge_;
};

// The bridge, not the test, turns a completed task's result into a surface.
TEST_F(TaskSurfaceBridgeTest, CompletedTaskProjectsASurfaceInProduction) {
  ASSERT_TRUE(registry_.Register(SurveyDescriptor()).has_value());
  ASSERT_TRUE(executors_.Register(std::make_unique<SurveyExecutor>()));

  EXPECT_EQ(surfaces_.size(), 0u);
  const TaskId task = tasks_.StartTask(
      "read the substrate survey readings from the log",
      LiveWindowKey::FromSessionId(4), AllowAll(), /*use_model=*/false,
      /*prefer_local=*/true);
  ASSERT_TRUE(task.is_valid());
  environment_.RunUntilIdle();

  // A surface now exists, created by the bridge (no test call to
  // CreateFromSemantic anywhere), and it is associated with the task.
  EXPECT_EQ(surfaces_.size(), 1u);
  const SurfaceId* projected = bridge_.SurfaceForTask(task);
  ASSERT_TRUE(projected);
  EXPECT_TRUE(surfaces_.FindSurface(*projected));
  EXPECT_EQ(surfaces_.SurfacesForTask(task).size(), 1u);
}

// Repeated terminal callbacks and streaming updates patch the SAME surface;
// they never create duplicates.
TEST_F(TaskSurfaceBridgeTest, RepeatedUpdatesReuseOneSurface) {
  ASSERT_TRUE(registry_.Register(SurveyDescriptor()).has_value());
  ASSERT_TRUE(executors_.Register(std::make_unique<SurveyExecutor>()));

  const TaskId task = tasks_.StartTask(
      "read the substrate survey readings from the log",
      LiveWindowKey::FromSessionId(4), AllowAll(), false, true);
  ASSERT_TRUE(task.is_valid());
  environment_.RunUntilIdle();
  ASSERT_EQ(surfaces_.size(), 1u);
  const SurfaceId first = *bridge_.SurfaceForTask(task);

  // Simulate additional observer notifications (as a streaming provider would
  // fire); the bridge must patch in place, not duplicate.
  bridge_.OnTaskUpdated(task);
  bridge_.OnTaskFinished(task);
  EXPECT_EQ(surfaces_.size(), 1u);
  EXPECT_EQ(*bridge_.SurfaceForTask(task), first);
}

// A task that produces no semantic result (e.g. a pure browser mutation)
// projects nothing; state is shown through the task, not a fake artifact.
TEST_F(TaskSurfaceBridgeTest, TaskWithoutDataProjectsNoSurface) {
  // Register a capability whose executor returns no semantic result.
  class MutationExecutor : public CapabilityExecutor {
   public:
    ToolId capability_id() const override {
      return ToolId::FromString("browser.thing.do");
    }
    void Execute(CapabilityRequest request,
                 CapabilityCallback callback) override {
      CapabilityOutcome outcome;
      outcome.step.status = StepStatus::kSucceeded;
      outcome.step.verification.verified = true;
      std::move(callback).Run(std::move(outcome));  // no semantic
    }
  };
  ToolDescriptor descriptor;
  descriptor.id = ToolId::FromString("browser.thing.do");
  descriptor.name = "do the browser thing";
  descriptor.description = "performs the browser mutation on the active window";
  descriptor.provider = "seoul";
  descriptor.risk = RiskCategory::kReversibleMutation;
  SchemaField q;
  q.name = "query";
  q.kind = SchemaFieldKind::kString;
  q.required = true;
  descriptor.input_schema.fields.push_back(std::move(q));
  ASSERT_TRUE(registry_.Register(std::move(descriptor)).has_value());
  ASSERT_TRUE(executors_.Register(std::make_unique<MutationExecutor>()));

  const TaskId task = tasks_.StartTask(
      "perform the browser mutation on the active window",
      LiveWindowKey::FromSessionId(4), AllowAll(), false, true);
  ASSERT_TRUE(task.is_valid());
  environment_.RunUntilIdle();
  EXPECT_EQ(surfaces_.size(), 0u);
  EXPECT_FALSE(bridge_.SurfaceForTask(task));
}

}  // namespace
}  // namespace seoul
