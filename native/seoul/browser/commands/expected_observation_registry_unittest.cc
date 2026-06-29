// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/expected_observation_registry.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

ExpectedObservation MakeObservation(CommandId id, int window, int tab) {
  ExpectedObservation obs;
  obs.command_id = id;
  obs.window = LiveWindowKey::FromSessionId(window);
  obs.tab = LiveTabKey::FromSessionId(tab);
  obs.created_at = base::TimeTicks::Now();
  return obs;
}

TEST(ExpectedObservationRegistryTest, RejectsDuplicateId) {
  ExpectedObservationRegistry registry;
  CommandId id = CommandId::Next();
  ASSERT_TRUE(registry.Register(MakeObservation(id, 1, 10)).has_value());
  EXPECT_EQ(registry.Register(MakeObservation(id, 1, 11)).error(),
            CommandError::kDuplicateCommandId);
}

TEST(ExpectedObservationRegistryTest, RejectsTargetConflict) {
  ExpectedObservationRegistry registry;
  ASSERT_TRUE(
      registry.Register(MakeObservation(CommandId::Next(), 1, 10)).has_value());
  EXPECT_EQ(
      registry.Register(MakeObservation(CommandId::Next(), 1, 10)).error(),
      CommandError::kConflictingCommand);
}

TEST(ExpectedObservationRegistryTest, ConsumeOnce) {
  ExpectedObservationRegistry registry;
  CommandId id = CommandId::Next();
  ASSERT_TRUE(registry.Register(MakeObservation(id, 1, 10)).has_value());
  EXPECT_TRUE(registry.Consume(id).has_value());
  EXPECT_FALSE(registry.Consume(id).has_value());
}

TEST(ExpectedObservationRegistryTest, ClearsOnShutdown) {
  ExpectedObservationRegistry registry;
  ASSERT_TRUE(
      registry.Register(MakeObservation(CommandId::Next(), 1, 10)).has_value());
  registry.Clear();
  EXPECT_EQ(registry.in_flight_count(), 0u);
}

}  // namespace
}  // namespace seoul
