// Project Seoul product runtime: generic end-to-end path (no browser).
//
// Proves the domain-neutral product loop with production services and an
// injected test capability that returns a PREVIOUSLY UNSEEN semantic schema:
//
//   goal -> planner (dynamic registry) -> task -> capability executor ->
//   observed+verified semantic result -> adaptive interface compiler ->
//   validated surface -> generic representation change (patch in place) ->
//   save the task's typed plan as a workflow.
//
// Nothing here is domain-specific and no schema is special-cased; the fixture
// schema is arbitrary on purpose.

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "seoul/browser/product/capability_executor.h"
#include "seoul/browser/product/planner.h"
#include "seoul/browser/product/surface_service.h"
#include "seoul/browser/product/task_service.h"
#include "seoul/browser/product/workflow_service.h"
#include "seoul/browser/saui/interface_compiler.h"
#include "seoul/browser/tools/tool_registry.h"
#include "seoul/browser/tools/tool_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

// A capability whose output is an arbitrary entity collection the compiler has
// never been specialized for.
class UnseenSchemaExecutor : public CapabilityExecutor {
 public:
  ToolId capability_id() const override {
    return ToolId::FromString("info.survey.readings");
  }

  void Execute(CapabilityRequest request,
               CapabilityCallback callback) override {
    SemanticResult result;
    result.schema.shape = SemanticShape::kEntityCollection;
    auto field = [](const std::string& id, FieldPrimitive primitive,
                    SemanticRole role) {
      FieldSpec f;
      f.id = id;
      f.label = id;
      f.primitive = primitive;
      f.role = role;
      f.nullable = false;
      return f;
    };
    result.schema.fields = {
        field("station", FieldPrimitive::kString, SemanticRole::kIdentifier),
        field("substrate", FieldPrimitive::kString, SemanticRole::kCategory),
        field("reading", FieldPrimitive::kNumber, SemanticRole::kMeasure)};
    base::Value::List rows;
    for (int i = 0; i < 3; ++i) {
      base::Value::Dict row;
      row.Set("station", "s" + base::NumberToString(i));
      row.Set("substrate", i % 2 ? "loam" : "silt");
      row.Set("reading", 10.0 + i);
      rows.Append(std::move(row));
    }
    result.data = base::Value(std::move(rows));
    result.provenance.base.source_name = "info.survey.readings";
    result.provenance.base.retrieved_at = base::Time::UnixEpoch();
    result.provenance.base.effective_at = base::Time::UnixEpoch();

    CapabilityOutcome outcome;
    outcome.step.status = StepStatus::kSucceeded;
    outcome.step.observed_summary = "Survey readings collected.";
    outcome.step.verification.verified = true;
    outcome.step.verification.method = "postcondition";
    outcome.semantic = std::move(result);
    std::move(callback).Run(std::move(outcome));
  }
};

ToolDescriptor SurveyDescriptor() {
  ToolDescriptor descriptor;
  descriptor.id = ToolId::FromString("info.survey.readings");
  descriptor.name = "survey readings";
  descriptor.description =
      "Collects substrate survey readings per station from the field log";
  descriptor.provider = "seoul";
  SchemaField query;
  query.name = "query";
  query.kind = SchemaFieldKind::kString;
  query.required = true;
  descriptor.input_schema.fields.push_back(std::move(query));
  descriptor.sensitivity = DataSensitivity::kOrganization;
  return descriptor;
}

class TaskCollector : public TaskServiceObserver {
 public:
  void OnTaskUpdated(const TaskId& id) override {}
  void OnTaskFinished(const TaskId& id) override { finished_ = id; }
  const TaskId& finished() const { return finished_; }

 private:
  TaskId finished_;
};

TEST(ProductEndToEndTest, GoalRunsThroughCapabilityToSurfaceAndWorkflow) {
  base::test::TaskEnvironment environment;

  ToolRegistry registry;
  ASSERT_TRUE(registry.Register(SurveyDescriptor()).has_value());

  CapabilityExecutorRegistry executors;
  ASSERT_TRUE(executors.Register(std::make_unique<UnseenSchemaExecutor>()));

  // Completeness: the descriptor has an executor.
  const CapabilityExecutorRegistry::CompletenessReport completeness =
      executors.CheckCompleteness({SurveyDescriptor()});
  EXPECT_TRUE(completeness.descriptors_without_executor.empty());
  EXPECT_TRUE(completeness.executors_without_descriptor.empty());

  Planner planner(registry, ModelPlanRequester());
  TaskService tasks(&registry, &executors, &planner, base::BindRepeating([] {
    return base::Time::UnixEpoch();
  }));
  SurfaceService surfaces;
  WorkflowService workflows(&tasks);

  TaskCollector collector;
  tasks.AddObserver(&collector);

  ToolPermissionContext context;
  context.max_sensitivity = DataSensitivity::kCredentialAdjacent;

  // 1) A natural-language goal is planned onto the survey capability and run.
  const TaskId task = tasks.StartTask(
      "collect the substrate survey readings from the field log",
      LiveWindowKey::FromSessionId(3), context, /*use_model=*/false,
      /*prefer_local=*/true);
  ASSERT_TRUE(task.is_valid());
  environment.RunUntilIdle();

  const std::optional<TaskSnapshot> snapshot = tasks.Snapshot(task);
  ASSERT_TRUE(snapshot.has_value());
  EXPECT_EQ(snapshot->state, TaskState::kCompleted);
  ASSERT_EQ(snapshot->receipts.size(), 1u);
  EXPECT_TRUE(snapshot->receipts[0].verification.verified);

  // 2) The verified semantic result compiles into a validated surface.
  const SemanticResult* semantic = tasks.FinalSemanticResult(task);
  ASSERT_TRUE(semantic);
  const SurfaceId surface =
      surfaces.CreateFromSemantic(*semantic, InterfaceIntent(), task);
  ASSERT_TRUE(surface.is_valid());
  const AdaptiveSurface* compiled = surfaces.FindSurface(surface);
  ASSERT_TRUE(compiled);
  EXPECT_FALSE(compiled->components.empty());

  // 3) A generic representation change patches the SAME surface in place.
  const size_t before = surfaces.AllSurfaces().size();
  EXPECT_TRUE(surfaces.SetRepresentation(surface, "bar_chart"));
  EXPECT_EQ(surfaces.AllSurfaces().size(), before);  // no duplicate

  // 4) The task's typed plan is saved as a reusable workflow (no positional
  // data - only the registered capability call).
  tasks.RemoveObserver(&collector);
  const std::optional<WorkflowId> workflow =
      workflows.SaveTaskAsWorkflow(task, "Substrate survey");
  ASSERT_TRUE(workflow.has_value());
  const WorkflowDefinition* definition = workflows.Find(workflow.value());
  ASSERT_TRUE(definition);
  ASSERT_EQ(definition->nodes.size(), 1u);
  EXPECT_EQ(definition->nodes[0].tool.value(), "info.survey.readings");
}

}  // namespace
}  // namespace seoul
