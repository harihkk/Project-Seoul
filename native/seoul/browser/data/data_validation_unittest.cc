// Project Seoul grounded data layer.
// Unit tests for generic provenance validation. Fixture values are
// deterministic test data only.

#include "seoul/browser/data/data_validation.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::Time TestTime(int minutes) {
  return base::Time::UnixEpoch() + base::Days(20000) + base::Minutes(minutes);
}

DataProvenance ValidProvenance() {
  DataProvenance p;
  p.source_name = "fixture-provider";
  p.source_url = "https://provider.test/data";
  p.retrieved_at = TestTime(10);
  p.effective_at = TestTime(9);
  p.timezone = "UTC";
  p.freshness = FreshnessState::kDelayed;
  return p;
}

TEST(DataProvenanceTest, RequiresSourceAndTimestamps) {
  DataProvenance p = ValidProvenance();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kEligible);

  p.source_name.clear();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kMissingSource);

  p = ValidProvenance();
  p.effective_at = base::Time();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kMissingTimestamps);

  p = ValidProvenance();
  p.retrieved_at = base::Time();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kMissingTimestamps);

  p = ValidProvenance();
  p.completeness = 1.5;
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kOutOfRangeValue);

  p = ValidProvenance();
  p.completeness = -0.1;
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kOutOfRangeValue);
}

TEST(DataProvenanceTest, FreshnessAndErrorNamesAreStable) {
  EXPECT_STREQ(FreshnessStateToString(FreshnessState::kRealTime),
               "real_time");
  EXPECT_STREQ(FreshnessStateToString(FreshnessState::kStale), "stale");
  EXPECT_STREQ(DataErrorToString(DataError::kNoProvider), "no_provider");
  EXPECT_STREQ(DataErrorToString(DataError::kUnresolvedEntity),
               "unresolved_entity");
}

}  // namespace
}  // namespace seoul
