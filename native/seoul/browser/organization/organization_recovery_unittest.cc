// Project Seoul native organization engine.

#include "base/values.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/organization_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(OrganizationRecoveryTest, CorruptSnapshotRejectedWithoutMutation) {
  base::DictValue corrupt;
  corrupt.Set("schema_version", 999999);
  auto parsed = DeserializeSnapshot(corrupt);
  EXPECT_FALSE(parsed.has_value());
}

TEST(OrganizationRecoveryTest, ValidEmptySnapshotRoundTrips) {
  OrganizationModel model;
  model.EnsureDefaultWorkspace();
  base::DictValue dict = SerializeSnapshot(model.ToSnapshot());
  auto parsed = DeserializeSnapshot(dict);
  ASSERT_TRUE(parsed.has_value());
  OrganizationModel loaded;
  EXPECT_TRUE(loaded.LoadSnapshot(parsed.value()).has_value());
}

}  // namespace
}  // namespace seoul
