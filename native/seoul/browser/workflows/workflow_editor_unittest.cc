// Project Seoul workflow system.
// Unit tests for typed workflow edit operations, versioning, duplication,
// and export/import round trips.

#include "seoul/browser/workflows/workflow_editor.h"

#include "base/test/bind.h"
#include "seoul/browser/workflows/workflow_graph.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class WorkflowEditorTest : public testing::Test {
 protected:
  WorkflowEditorTest() {
    workflow_.id = WorkflowId::GenerateNew();
    workflow_.name = "Morning research";
    workflow_.nodes.push_back(ToolNode("search", "info.search.web"));
    workflow_.nodes.push_back(ToolNode("summarize", "info.summarize.page"));
    workflow_.nodes.push_back(ToolNode("email", "connector.mail.send"));
    workflow_.edges.push_back(Edge("search", "summarize"));
    workflow_.edges.push_back(Edge("summarize", "email"));
    workflow_.created_at = clock_;
    workflow_.updated_at = clock_;
  }

  static WorkflowNode ToolNode(const std::string& id, const std::string& tool) {
    WorkflowNode node;
    node.id = id;
    node.kind = WorkflowNodeKind::kToolStep;
    node.label = "Step " + id;
    node.tool = ToolId::FromString(tool);
    node.args.Set("query", id);
    return node;
  }

  static WorkflowEdge Edge(const std::string& from, const std::string& to) {
    WorkflowEdge edge;
    edge.from = from;
    edge.to = to;
    edge.kind = WorkflowEdgeKind::kSequence;
    return edge;
  }

  WorkflowClock Clock() {
    return base::BindLambdaForTesting([this]() { return clock_; });
  }

  base::Time clock_ = base::Time::UnixEpoch() + base::Days(20000);
  WorkflowDefinition workflow_;
};

TEST_F(WorkflowEditorTest, RemoveNodeReconnectsAndBumpsVersion) {
  clock_ += base::Minutes(5);
  // "Remove the email step." resolves to this typed operation.
  ASSERT_TRUE(RemoveWorkflowNode(workflow_, "email", Clock()).has_value());
  EXPECT_EQ(workflow_.version, 2);
  EXPECT_EQ(workflow_.updated_at, clock_);
  EXPECT_EQ(workflow_.nodes.size(), 2u);
  for (const WorkflowEdge& edge : workflow_.edges) {
    EXPECT_NE(edge.from, "email");
    EXPECT_NE(edge.to, "email");
  }

  // Removing a middle node splices its neighbors together.
  WorkflowDefinition three = workflow_;
  ASSERT_TRUE(AddWorkflowNode(three,
                              ToolNode("archive", "browser.tabs.archive"), "",
                              Clock())
                  .has_value());
  ASSERT_TRUE(RemoveWorkflowNode(three, "summarize", Clock()).has_value());
  bool reconnected = false;
  for (const WorkflowEdge& edge : three.edges) {
    if (edge.from == "search" && edge.to == "archive") {
      reconnected = true;
    }
  }
  EXPECT_TRUE(reconnected);
}

TEST_F(WorkflowEditorTest, InvalidEditLeavesWorkflowUntouched) {
  const WorkflowDefinition before = workflow_;
  EXPECT_EQ(RemoveWorkflowNode(workflow_, "never_existed", Clock()).error(),
            WorkflowError::kUnknownNode);
  // An edit that would create an unbounded cycle is rejected atomically.
  WorkflowEdge cycle;
  cycle.from = "email";
  cycle.to = "search";
  cycle.kind = WorkflowEdgeKind::kSequence;
  EXPECT_EQ(AddWorkflowEdge(workflow_, cycle, Clock()).error(),
            WorkflowError::kCycleWithoutLoop);
  EXPECT_EQ(workflow_, before);
}

TEST_F(WorkflowEditorTest, InsertAfterSplicesSequenceEdges) {
  ASSERT_TRUE(AddWorkflowNode(workflow_,
                              ToolNode("filter", "info.filter.results"),
                              "search", Clock())
                  .has_value());
  bool search_to_filter = false;
  bool filter_to_summarize = false;
  for (const WorkflowEdge& edge : workflow_.edges) {
    if (edge.from == "search" && edge.to == "filter") {
      search_to_filter = true;
    }
    if (edge.from == "filter" && edge.to == "summarize") {
      filter_to_summarize = true;
    }
    EXPECT_FALSE(edge.from == "search" && edge.to == "summarize");
  }
  EXPECT_TRUE(search_to_filter);
  EXPECT_TRUE(filter_to_summarize);
}

TEST_F(WorkflowEditorTest, ApprovalTriggerAndLoopEdits) {
  // "Ask me before sending the email."
  ASSERT_TRUE(
      SetWorkflowNodeApproval(workflow_, "email", true, Clock()).has_value());
  EXPECT_TRUE(workflow_.nodes[2].requires_approval);

  // "Run this every weekday morning" (interval form).
  WorkflowTrigger trigger;
  trigger.kind = WorkflowTriggerKind::kSchedule;
  trigger.interval_minutes = 24 * 60;
  ASSERT_TRUE(SetWorkflowTrigger(workflow_, trigger, Clock()).has_value());
  EXPECT_EQ(workflow_.trigger.kind, WorkflowTriggerKind::kSchedule);

  trigger.interval_minutes = 0;
  EXPECT_EQ(SetWorkflowTrigger(workflow_, trigger, Clock()).error(),
            WorkflowError::kInvalidTrigger);

  // Bounded loop from email back to summarize, added atomically.
  ASSERT_TRUE(
      AddWorkflowLoop(workflow_, "email", "summarize", 4, Clock()).has_value());
  EXPECT_EQ(workflow_.nodes[1].max_iterations, 4);

  // A self-loop is rejected and leaves everything unchanged.
  EXPECT_EQ(
      AddWorkflowLoop(workflow_, "summarize", "summarize", 4, Clock()).error(),
      WorkflowError::kSelfEdge);

  // Removing the loop clears the now-dead header bound.
  ASSERT_TRUE(
      RemoveWorkflowLoop(workflow_, "email", "summarize", Clock()).has_value());
  EXPECT_EQ(workflow_.nodes[1].max_iterations, 0);
}

TEST_F(WorkflowEditorTest, SetArgsValidatesParamReferences) {
  base::DictValue args;
  args.Set("query", "{{param:topic}}");
  EXPECT_EQ(SetWorkflowNodeArgs(workflow_, "search", std::move(args), Clock())
                .error(),
            WorkflowError::kUnknownParamReference);

  WorkflowParam topic;
  topic.field.name = "topic";
  topic.field.kind = SchemaFieldKind::kString;
  workflow_.params.push_back(topic);
  base::DictValue valid_args;
  valid_args.Set("query", "{{param:topic}}");
  EXPECT_TRUE(
      SetWorkflowNodeArgs(workflow_, "search", std::move(valid_args), Clock())
          .has_value());
}

TEST_F(WorkflowEditorTest, DuplicateGetsFreshIdentityAndVersion) {
  workflow_.version = 7;
  WorkflowRunSummary run;
  run.ran_at = clock_;
  run.succeeded = true;
  workflow_.last_run = run;
  clock_ += base::Hours(1);

  WorkflowDefinition copy = DuplicateWorkflow(workflow_, Clock());
  EXPECT_FALSE(copy.id == workflow_.id);
  EXPECT_EQ(copy.name, "Morning research (copy)");
  EXPECT_EQ(copy.version, 1);
  EXPECT_FALSE(copy.last_run.has_value());
  EXPECT_EQ(copy.nodes.size(), workflow_.nodes.size());
}

TEST_F(WorkflowEditorTest, ExportImportRoundTrips) {
  WorkflowParam topic;
  topic.field.name = "topic";
  topic.field.kind = SchemaFieldKind::kEnum;
  topic.field.enum_values = {"markets", "weather"};
  topic.default_value = base::Value("markets");
  workflow_.params.push_back(topic);
  workflow_.trigger.kind = WorkflowTriggerKind::kSchedule;
  workflow_.trigger.interval_minutes = 120;
  workflow_.nodes[2].requires_approval = true;

  base::DictValue exported = ExportWorkflow(workflow_);
  auto imported = ImportWorkflow(base::Value(exported.Clone()));
  ASSERT_TRUE(imported.has_value());
  EXPECT_EQ(imported.value(), workflow_);
}

TEST_F(WorkflowEditorTest, ImportRejectsUnknownSchemaAndBadGraphs) {
  base::DictValue exported = ExportWorkflow(workflow_);
  exported.Set("schema_version", 99);
  EXPECT_EQ(ImportWorkflow(base::Value(exported.Clone())).error(),
            WorkflowError::kUnsupportedSchema);

  base::DictValue bad_graph = ExportWorkflow(workflow_);
  bad_graph.Set("schema_version", kWorkflowSchemaVersion);
  base::ListValue* edges = bad_graph.FindList("edges");
  ASSERT_NE(edges, nullptr);
  base::DictValue cycle;
  cycle.Set("from", "email");
  cycle.Set("to", "search");
  cycle.Set("kind", "sequence");
  edges->Append(std::move(cycle));
  EXPECT_EQ(ImportWorkflow(base::Value(std::move(bad_graph))).error(),
            WorkflowError::kCycleWithoutLoop);
}

}  // namespace
}  // namespace seoul
