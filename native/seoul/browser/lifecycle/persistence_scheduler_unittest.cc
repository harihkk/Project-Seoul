// Project Seoul native lifecycle bridge.
// Authored for later compilation on a capable host. Not run on the dev machine.

#include "seoul/browser/lifecycle/persistence_scheduler.h"

#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class PersistenceSchedulerTest : public testing::Test {
 protected:
  PersistenceSchedulerTest()
      : scheduler_(base::BindLambdaForTesting([this]() {
                     ++writer_calls_;
                     return write_ok_;
                   }),
                   base::SequencedTaskRunner::GetCurrentDefault()) {}

  base::test::TaskEnvironment env_;
  int writer_calls_ = 0;
  bool write_ok_ = true;
  PersistenceScheduler scheduler_;
};

TEST_F(PersistenceSchedulerTest, OneRequestOneWrite) {
  scheduler_.ScheduleWrite();
  env_.RunUntilIdle();
  EXPECT_EQ(1, writer_calls_);
  EXPECT_EQ(1, scheduler_.writes_committed());
  EXPECT_FALSE(scheduler_.has_pending());
}

TEST_F(PersistenceSchedulerTest, BurstCoalescesIntoOneWrite) {
  for (int i = 0; i < 8; ++i) {
    scheduler_.ScheduleWrite();
  }
  env_.RunUntilIdle();
  EXPECT_EQ(1,
            writer_calls_);  // a burst within one turn collapses to one write
  EXPECT_EQ(1, scheduler_.writes_committed());
}

TEST_F(PersistenceSchedulerTest, FlushWritesPendingOnce) {
  scheduler_.ScheduleWrite();
  EXPECT_TRUE(scheduler_.Flush());  // synchronous write (e.g. shutdown)
  EXPECT_EQ(1, writer_calls_);
  env_.RunUntilIdle();  // the queued task sees nothing dirty
  EXPECT_EQ(1, writer_calls_);
}

TEST_F(PersistenceSchedulerTest, FlushWithNothingPendingIsNoop) {
  EXPECT_FALSE(scheduler_.Flush());
  EXPECT_EQ(0, writer_calls_);
}

TEST_F(PersistenceSchedulerTest, FailureIsObservableAndKeepsPending) {
  write_ok_ = false;
  scheduler_.ScheduleWrite();
  env_.RunUntilIdle();
  EXPECT_EQ(1, scheduler_.writes_failed());
  EXPECT_EQ(0, scheduler_.writes_committed());
  EXPECT_TRUE(scheduler_.has_pending());  // not lost, not corrupting memory
}

TEST_F(PersistenceSchedulerTest, BoundedRetryOnNextRequest) {
  write_ok_ = false;
  scheduler_.ScheduleWrite();
  env_.RunUntilIdle();
  EXPECT_EQ(1, writer_calls_);  // one attempt, no spin
  // A later request retries; this time it succeeds.
  write_ok_ = true;
  scheduler_.ScheduleWrite();
  env_.RunUntilIdle();
  EXPECT_EQ(2, writer_calls_);
  EXPECT_EQ(1, scheduler_.writes_committed());
  EXPECT_FALSE(scheduler_.has_pending());
}

TEST_F(PersistenceSchedulerTest, ScheduleAfterShutdownIsIgnored) {
  scheduler_.Shutdown();
  scheduler_.ScheduleWrite();
  env_.RunUntilIdle();
  EXPECT_EQ(0, writer_calls_);
  EXPECT_EQ(1, scheduler_.writes_skipped_after_shutdown());
}

}  // namespace
}  // namespace seoul
