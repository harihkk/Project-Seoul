// Project Seoul workflow system.
// Unit tests for graph validation, deterministic ordering, and compilation
// onto typed plans.

#include "seoul/browser/workflows/workflow_graph.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

WorkflowNode ToolNode(const std::string& id, const std::string& tool) {
  WorkflowNode node;
  node.id = id;
  node.kind = WorkflowNodeKind::kToolStep;
  node.label = "Step " + id;
  node.tool = ToolId::FromString(tool);
  node.args.Set("query", id);
  return node;
}

WorkflowEdge Edge(const std::string& from,
                  const std::string& to,
                  WorkflowEdgeKind kind = WorkflowEdgeKind::kSequence) {
  WorkflowEdge edge;
  edge.from = from;
  edge.to = to;
  edge.kind = kind;
  return edge;
}

WorkflowDefinition LinearWorkflow() {
  WorkflowDefinition definition;
  definition.id = WorkflowId::GenerateNew();
  definition.name = "Compare prices";
  definition.nodes.push_back(ToolNode("search", "info.search.web"));
  definition.nodes.push_back(ToolNode("extract", "page.observe.text"));
  definition.nodes.push_back(ToolNode("compare", "info.compare.items"));
  definition.edges.push_back(Edge("search", "extract"));
  definition.edges.push_back(Edge("extract", "compare"));
  return definition;
}

TEST(WorkflowGraphTest, ValidLinearWorkflowPasses) {
  EXPECT_TRUE(ValidateWorkflowStructure(LinearWorkflow()).has_value());
}

TEST(WorkflowGraphTest, RejectsStructuralDefects) {
  WorkflowDefinition unnamed = LinearWorkflow();
  unnamed.name.clear();
  EXPECT_EQ(ValidateWorkflowStructure(unnamed).error(),
            WorkflowError::kInvalidName);

  WorkflowDefinition empty;
  empty.name = "Empty";
  EXPECT_EQ(ValidateWorkflowStructure(empty).error(),
            WorkflowError::kEmptyWorkflow);

  WorkflowDefinition duplicate = LinearWorkflow();
  duplicate.nodes.push_back(ToolNode("search", "info.search.web"));
  EXPECT_EQ(ValidateWorkflowStructure(duplicate).error(),
            WorkflowError::kDuplicateNodeId);

  WorkflowDefinition dangling = LinearWorkflow();
  dangling.edges.push_back(Edge("compare", "never_declared"));
  EXPECT_EQ(ValidateWorkflowStructure(dangling).error(),
            WorkflowError::kEdgeUnknownNode);

  WorkflowDefinition self_edge = LinearWorkflow();
  self_edge.edges.push_back(Edge("search", "search"));
  EXPECT_EQ(ValidateWorkflowStructure(self_edge).error(),
            WorkflowError::kSelfEdge);

  WorkflowDefinition missing_prompt = LinearWorkflow();
  WorkflowNode approval;
  approval.id = "confirm";
  approval.kind = WorkflowNodeKind::kApproval;
  missing_prompt.nodes.push_back(approval);
  missing_prompt.edges.push_back(Edge("compare", "confirm"));
  EXPECT_EQ(ValidateWorkflowStructure(missing_prompt).error(),
            WorkflowError::kMissingPrompt);
}

TEST(WorkflowGraphTest, RejectsCyclesWithoutExplicitLoop) {
  WorkflowDefinition cyclic = LinearWorkflow();
  cyclic.edges.push_back(Edge("compare", "search"));  // kSequence cycle
  EXPECT_EQ(ValidateWorkflowStructure(cyclic).error(),
            WorkflowError::kCycleWithoutLoop);
}

TEST(WorkflowGraphTest, BoundedLoopIsAcceptedUnboundedIsNot) {
  WorkflowDefinition looped = LinearWorkflow();
  looped.nodes[1].max_iterations = 5;  // "extract" is the loop header
  looped.edges.push_back(
      Edge("compare", "extract", WorkflowEdgeKind::kLoopBack));
  EXPECT_TRUE(ValidateWorkflowStructure(looped).has_value());

  WorkflowDefinition unbounded = LinearWorkflow();
  unbounded.edges.push_back(
      Edge("compare", "extract", WorkflowEdgeKind::kLoopBack));
  EXPECT_EQ(ValidateWorkflowStructure(unbounded).error(),
            WorkflowError::kLoopUnbounded);

  WorkflowDefinition dead_header = LinearWorkflow();
  dead_header.nodes[1].max_iterations = 5;  // bound without a loop-back edge
  EXPECT_EQ(ValidateWorkflowStructure(dead_header).error(),
            WorkflowError::kLoopHeaderUnreachable);
}

TEST(WorkflowGraphTest, TriggersAreValidated) {
  WorkflowDefinition workflow = LinearWorkflow();
  workflow.trigger.kind = WorkflowTriggerKind::kSchedule;
  workflow.trigger.interval_minutes = 0;
  EXPECT_EQ(ValidateWorkflowStructure(workflow).error(),
            WorkflowError::kInvalidTrigger);
  workflow.trigger.interval_minutes = 60;
  EXPECT_TRUE(ValidateWorkflowStructure(workflow).has_value());

  workflow.trigger.kind = WorkflowTriggerKind::kNavigation;
  workflow.trigger.origin_pattern.clear();
  EXPECT_EQ(ValidateWorkflowStructure(workflow).error(),
            WorkflowError::kInvalidTrigger);
  workflow.trigger.origin_pattern = "https://docs.example.test";
  EXPECT_TRUE(ValidateWorkflowStructure(workflow).has_value());
}

TEST(WorkflowGraphTest, TopologicalOrderIsDeterministic) {
  WorkflowDefinition branched = LinearWorkflow();
  branched.nodes.push_back(ToolNode("archive", "browser.tabs.archive"));
  branched.edges.push_back(Edge("search", "archive"));
  auto order = TopologicalOrder(branched);
  ASSERT_TRUE(order.has_value());
  ASSERT_EQ(order->size(), 4u);
  EXPECT_EQ(order->front(), "search");
  // "archive" and "extract" are both ready after "search"; lexicographic
  // tie-break makes the order stable.
  EXPECT_EQ((*order)[1], "archive");
  EXPECT_EQ((*order)[2], "extract");
}

TEST(WorkflowGraphTest, CompilesToPlanWithGuardsAndLoops) {
  WorkflowDefinition workflow = LinearWorkflow();
  // Branch: notify only when compare succeeds; cleanup when it fails.
  WorkflowNode notify = ToolNode("notify", "canvas.surface.update");
  WorkflowNode cleanup = ToolNode("cleanup", "browser.tabs.archive");
  workflow.nodes.push_back(notify);
  workflow.nodes.push_back(cleanup);
  workflow.edges.push_back(
      Edge("compare", "notify", WorkflowEdgeKind::kOnSuccess));
  workflow.edges.push_back(
      Edge("compare", "cleanup", WorkflowEdgeKind::kOnFailure));
  // Loop the extract step.
  workflow.nodes[1].max_iterations = 3;
  workflow.edges.push_back(
      Edge("extract", "extract2", WorkflowEdgeKind::kSequence));
  workflow.nodes.push_back(ToolNode("extract2", "page.observe.text"));
  workflow.edges.push_back(
      Edge("extract2", "extract", WorkflowEdgeKind::kLoopBack));

  auto plan = CompileWorkflow(workflow, base::Value::Dict(), TaskBudgets());
  ASSERT_TRUE(plan.has_value());
  ASSERT_EQ(plan->steps.size(), workflow.nodes.size());

  const PlanStep* notify_step = nullptr;
  const PlanStep* cleanup_step = nullptr;
  const PlanStep* extract_step = nullptr;
  for (const PlanStep& step : plan->steps) {
    if (step.id == "notify") {
      notify_step = &step;
    }
    if (step.id == "cleanup") {
      cleanup_step = &step;
    }
    if (step.id == "extract") {
      extract_step = &step;
    }
  }
  ASSERT_NE(notify_step, nullptr);
  ASSERT_TRUE(notify_step->guard.has_value());
  EXPECT_EQ(notify_step->guard->depends_on_step, "compare");
  EXPECT_TRUE(notify_step->guard->require_success);
  ASSERT_NE(cleanup_step, nullptr);
  ASSERT_TRUE(cleanup_step->guard.has_value());
  EXPECT_FALSE(cleanup_step->guard->require_success);
  ASSERT_NE(extract_step, nullptr);
  EXPECT_GT(extract_step->loop_group, 0);
  EXPECT_EQ(extract_step->max_iterations, 3);
}

TEST(WorkflowGraphTest, CompileSubstitutesTypedParams) {
  WorkflowDefinition workflow = LinearWorkflow();
  WorkflowParam param;
  param.field.name = "target_price";
  param.field.kind = SchemaFieldKind::kNumber;
  param.default_value = base::Value(500.0);
  workflow.params.push_back(param);
  workflow.nodes[2].args.Set("max_price", "{{param:target_price}}");

  // Run value wins over the default and keeps its type.
  base::Value::Dict run_values;
  run_values.Set("target_price", 350.0);
  auto plan = CompileWorkflow(workflow, run_values, TaskBudgets());
  ASSERT_TRUE(plan.has_value());
  const PlanStep& compare = plan->steps.back();
  EXPECT_EQ(compare.args.FindDouble("max_price").value_or(0.0), 350.0);

  // Default applies when no run value is given.
  plan = CompileWorkflow(workflow, base::Value::Dict(), TaskBudgets());
  ASSERT_TRUE(plan.has_value());
  EXPECT_EQ(plan->steps.back().args.FindDouble("max_price").value_or(0.0),
            500.0);

  // Unknown references are rejected at validation time.
  workflow.nodes[2].args.Set("max_price", "{{param:never_declared}}");
  EXPECT_EQ(ValidateWorkflowStructure(workflow).error(),
            WorkflowError::kUnknownParamReference);
}

}  // namespace
}  // namespace seoul
