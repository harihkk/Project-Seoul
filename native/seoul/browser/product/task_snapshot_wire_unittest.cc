// Project Seoul product runtime.
// Conformance tests for the canonical task-snapshot wire codec over the
// shared protocol fixture corpus (see protocol/README.md).

#include "seoul/browser/product/task_snapshot_wire.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::Value ReadFixture(const std::string& relative) {
  base::FilePath src_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));
  std::string contents;
  const base::FilePath path =
      src_root.AppendASCII("seoul/protocol/fixtures").AppendASCII(relative);
  CHECK(base::ReadFileToString(path, &contents)) << "missing fixture " << path;
  std::optional<base::Value> parsed = base::JSONReader::Read(contents, base::JSON_PARSE_RFC);
  CHECK(parsed.has_value()) << "unparseable fixture " << relative;
  return std::move(parsed.value());
}

TEST(TaskSnapshotWireTest, FixturesParseAndRoundTrip) {
  for (const char* name :
       {"task/completed-fixture.json", "task/awaiting-approval.json",
        "task/awaiting-input.json", "task/failed.json"}) {
    SCOPED_TRACE(name);
    auto snapshot = ParseTaskSnapshot(ReadFixture(name));
    ASSERT_TRUE(snapshot.has_value()) << snapshot.error();

    auto reparsed =
        ParseTaskSnapshot(base::Value(TaskSnapshotToValue(snapshot.value())));
    ASSERT_TRUE(reparsed.has_value()) << reparsed.error();
    EXPECT_EQ(TaskSnapshotToValue(snapshot.value()),
              TaskSnapshotToValue(reparsed.value()));
  }
}

TEST(TaskSnapshotWireTest, FixtureContractVerificationSurvivesTheWire) {
  auto snapshot = ParseTaskSnapshot(ReadFixture("task/completed-fixture.json"));
  ASSERT_TRUE(snapshot.has_value());
  ASSERT_EQ(snapshot->receipts.size(), 1u);
  const VerificationRecord& verification = snapshot->receipts[0].verification;
  EXPECT_TRUE(verification.verified);
  // Synthetic fixture execution must never masquerade as real observation.
  EXPECT_EQ(verification.method, "fixture_contract");
  EXPECT_EQ(verification.detail, "fixture contract validated");
  EXPECT_EQ(snapshot->window.value(), "w-1");
}

TEST(TaskSnapshotWireTest, FutureProtocolVersionIsRejected) {
  auto snapshot =
      ParseTaskSnapshot(ReadFixture("compat/snapshot-future-version.json"));
  ASSERT_FALSE(snapshot.has_value());
  EXPECT_NE(snapshot.error().find("schema_version"), std::string::npos);
}

TEST(TaskSnapshotWireTest, EnumWireNamesRoundTrip) {
  for (auto origin : {PlanOrigin::kDeterministic, PlanOrigin::kLocalModel,
                      PlanOrigin::kCloudModel}) {
    PlanOrigin out;
    EXPECT_TRUE(PlanOriginFromWire(PlanOriginToWire(origin), &out));
    EXPECT_EQ(out, origin);
  }
  TaskState state;
  EXPECT_TRUE(TaskStateFromWire("monitoring", &state));
  EXPECT_EQ(state, TaskState::kMonitoring);
  EXPECT_FALSE(TaskStateFromWire("daydreaming", &state));
  StepStatus status;
  EXPECT_TRUE(StepStatusFromWire("outcome_unknown", &status));
  EXPECT_EQ(status, StepStatus::kOutcomeUnknown);
}

}  // namespace
}  // namespace seoul
