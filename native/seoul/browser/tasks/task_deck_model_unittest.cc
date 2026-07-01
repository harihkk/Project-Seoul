// Project Seoul general-purpose operator: task layer.
// Unit tests for the Task Deck model: state transitions, filters, receipts,
// bounded history with active-task protection.

#include "seoul/browser/tasks/task_deck_model.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class RecordingDeckObserver : public TaskDeckObserver {
 public:
  void OnTaskDeckChanged(const TaskId& task) override { ++changes; }
  int changes = 0;
};

class TaskDeckModelTest : public testing::Test {
 protected:
  TaskDeckModelTest()
      : deck_(base::BindLambdaForTesting([this]() { return clock_; })) {
    deck_.AddObserver(&observer_);
  }

  ~TaskDeckModelTest() override { deck_.RemoveObserver(&observer_); }

  TaskRecord MakeTask(const std::string& title) {
    TaskRecord record;
    record.id = TaskId::GenerateNew();
    record.title = title;
    record.goal = "goal: " + title;
    record.state = TaskState::kDraft;
    return record;
  }

  base::Time clock_ = base::Time::UnixEpoch() + base::Days(20000);
  TaskDeckModel deck_;
  RecordingDeckObserver observer_;
};

TEST_F(TaskDeckModelTest, AddFindAndNotify) {
  TaskRecord record = MakeTask("Compare prices");
  const TaskId id = record.id;
  ASSERT_TRUE(deck_.Add(std::move(record)));
  EXPECT_EQ(observer_.changes, 1);
  const TaskRecord* found = deck_.Find(id);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->title, "Compare prices");
  EXPECT_FALSE(deck_.Add(MakeTask("")));  // empty title rejected
}

TEST_F(TaskDeckModelTest, LegalAndIllegalTransitions) {
  TaskRecord record = MakeTask("Research");
  const TaskId id = record.id;
  ASSERT_TRUE(deck_.Add(std::move(record)));

  EXPECT_TRUE(deck_.SetState(id, TaskState::kPlanning));
  EXPECT_TRUE(deck_.SetState(id, TaskState::kExecuting));
  EXPECT_TRUE(deck_.SetState(id, TaskState::kPaused));
  EXPECT_TRUE(deck_.SetState(id, TaskState::kExecuting));
  EXPECT_TRUE(deck_.SetState(id, TaskState::kMonitoring));
  EXPECT_TRUE(deck_.SetState(id, TaskState::kCompleted));

  // Terminal states never transition.
  EXPECT_FALSE(deck_.SetState(id, TaskState::kExecuting));
  EXPECT_FALSE(deck_.SetState(id, TaskState::kFailed));

  TaskRecord skipper = MakeTask("Skipper");
  const TaskId skipper_id = skipper.id;
  ASSERT_TRUE(deck_.Add(std::move(skipper)));
  EXPECT_FALSE(deck_.SetState(skipper_id, TaskState::kExecuting));
  EXPECT_FALSE(deck_.SetState(skipper_id, TaskState::kMonitoring));
}

TEST_F(TaskDeckModelTest, FiltersSeparateActiveMonitoringFinished) {
  TaskRecord active = MakeTask("Active");
  const TaskId active_id = active.id;
  ASSERT_TRUE(deck_.Add(std::move(active)));
  ASSERT_TRUE(deck_.SetState(active_id, TaskState::kPlanning));
  ASSERT_TRUE(deck_.SetState(active_id, TaskState::kExecuting));

  TaskRecord monitor = MakeTask("Monitor");
  const TaskId monitor_id = monitor.id;
  ASSERT_TRUE(deck_.Add(std::move(monitor)));
  ASSERT_TRUE(deck_.SetState(monitor_id, TaskState::kPlanning));
  ASSERT_TRUE(deck_.SetState(monitor_id, TaskState::kExecuting));
  ASSERT_TRUE(deck_.SetState(monitor_id, TaskState::kMonitoring));

  TaskRecord failed = MakeTask("Failed");
  const TaskId failed_id = failed.id;
  ASSERT_TRUE(deck_.Add(std::move(failed)));
  ASSERT_TRUE(deck_.SetState(failed_id, TaskState::kPlanning));
  ASSERT_TRUE(deck_.SetState(failed_id, TaskState::kFailed,
                             TaskFailureReason::kStepFailed));

  EXPECT_EQ(deck_.Active().size(), 1u);
  EXPECT_EQ(deck_.Monitoring().size(), 1u);
  EXPECT_EQ(deck_.Finished().size(), 1u);
  EXPECT_EQ(deck_.Finished()[0]->failure_reason,
            TaskFailureReason::kStepFailed);
}

TEST_F(TaskDeckModelTest, ReceiptsAccumulateCostAndRoute) {
  TaskRecord record = MakeTask("Costed");
  const TaskId id = record.id;
  ASSERT_TRUE(deck_.Add(std::move(record)));

  ActionReceipt deterministic;
  deterministic.step_id = "s1";
  deterministic.route = ExecutionRoute::kDeterministic;
  ASSERT_TRUE(deck_.AppendReceipt(id, deterministic));
  EXPECT_EQ(deck_.Find(id)->dominant_route, ExecutionRoute::kDeterministic);

  ActionReceipt local;
  local.step_id = "s2";
  local.route = ExecutionRoute::kLocalModel;
  ASSERT_TRUE(deck_.AppendReceipt(id, local));
  EXPECT_EQ(deck_.Find(id)->dominant_route, ExecutionRoute::kLocalModel);

  ActionReceipt cloud;
  cloud.step_id = "s3";
  cloud.route = ExecutionRoute::kCloudModel;
  cloud.cost_microdollars = 1500;
  ASSERT_TRUE(deck_.AppendReceipt(id, cloud));
  EXPECT_EQ(deck_.Find(id)->dominant_route, ExecutionRoute::kCloudModel);
  EXPECT_EQ(deck_.Find(id)->total_cost_microdollars, 1500);
  EXPECT_EQ(deck_.Find(id)->receipts.size(), 3u);
}

TEST_F(TaskDeckModelTest, EvictionProtectsActiveTasks) {
  // Fill the deck with finished tasks plus one active task.
  TaskId first_finished_id;
  for (size_t i = 0; i < kMaxTasksInDeck; ++i) {
    TaskRecord record = MakeTask("Task " + std::to_string(i));
    const TaskId id = record.id;
    ASSERT_TRUE(deck_.Add(std::move(record)));
    clock_ += base::Seconds(1);
    if (i == 0) {
      ASSERT_TRUE(deck_.SetState(id, TaskState::kPlanning));
      ASSERT_TRUE(deck_.SetState(id, TaskState::kExecuting));
    } else {
      ASSERT_TRUE(deck_.SetState(id, TaskState::kCancelled));
      if (i == 1) {
        first_finished_id = id;
      }
    }
  }
  ASSERT_EQ(deck_.size(), kMaxTasksInDeck);

  // Adding one more evicts the oldest finished task, not the active one.
  TaskRecord overflow = MakeTask("Overflow");
  ASSERT_TRUE(deck_.Add(std::move(overflow)));
  EXPECT_EQ(deck_.size(), kMaxTasksInDeck);
  EXPECT_EQ(deck_.Find(first_finished_id), nullptr);
  EXPECT_EQ(deck_.Active().size(), 2u);  // original active + overflow draft
}

}  // namespace
}  // namespace seoul
