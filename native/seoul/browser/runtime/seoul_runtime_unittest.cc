// Project Seoul runtime composition.
// Unit tests: the runtime registers built-in capabilities, mirrors connector
// capabilities into the graph and removes them on shutdown, and produces a
// coherent routing policy. Uses a fake connector; no browser needed.

#include "seoul/browser/runtime/seoul_runtime.h"

#include <memory>

#include "base/test/bind.h"
#include "seoul/browser/connectors/fake_connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

SceneResolvers PermissiveResolvers() {
  SceneResolvers resolvers;
  resolvers.workspace_exists =
      base::BindLambdaForTesting([](const std::string&) { return true; });
  resolvers.theme_exists =
      base::BindLambdaForTesting([](const std::string&) { return true; });
  resolvers.site_layer_exists =
      base::BindLambdaForTesting([](const std::string&) { return true; });
  return resolvers;
}

TEST(SeoulRuntimeTest, RegistersBuiltinCapabilitiesWithProductionOwner) {
  SeoulRuntime runtime(PermissiveResolvers());
  // The browser command catalog and information seams are present.
  EXPECT_NE(runtime.capabilities().Find(ToolId::FromString("browser.tabs.open")),
            nullptr);
  EXPECT_NE(runtime.capabilities().Find(ToolId::FromString("info.search.web")),
            nullptr);
  EXPECT_NE(
      runtime.capabilities().Find(ToolId::FromString("files.selection.read")),
      nullptr);
}

TEST(SeoulRuntimeTest, ConnectorCapabilitiesJoinAndLeaveTheGraph) {
  SeoulRuntime runtime(PermissiveResolvers());
  const size_t builtin_count = runtime.capabilities().size();

  auto connector =
      std::make_unique<FakeConnector>("calendar", "user@example.test");
  connector->AddTool("list_events");
  ASSERT_TRUE(runtime.connectors().Connect(std::move(connector)).has_value());
  EXPECT_EQ(runtime.capabilities().size(), builtin_count + 1);
  EXPECT_NE(runtime.capabilities().Find(
                ToolId::FromString("connector.calendar.list_events")),
            nullptr);

  // Shutdown disconnects the connector and removes its capabilities.
  runtime.Shutdown();
  EXPECT_EQ(runtime.capabilities().Find(
                ToolId::FromString("connector.calendar.list_events")),
            nullptr);
}

TEST(SeoulRuntimeTest, RoutingPolicyReflectsInputs) {
  SeoulRuntime runtime(PermissiveResolvers());
  const RoutingPolicy policy = runtime.MakeRoutingPolicy(
      /*cloud_enabled=*/true, /*local_available=*/true,
      /*remaining_budget_microdollars=*/500000);
  EXPECT_TRUE(policy.cloud_enabled);
  EXPECT_TRUE(policy.local_available);
  // Capability-first default: the best qualifying route wins unless the user
  // opts into preferring local.
  EXPECT_FALSE(policy.prefer_local);
  EXPECT_EQ(policy.remaining_budget_microdollars, 500000);
}

TEST(SeoulRuntimeTest, ScenesAreReachableThroughTheRuntime) {
  SeoulRuntime runtime(PermissiveResolvers());
  SceneDefinition scene;
  scene.id = "research";
  scene.name = "Research";
  scene.workspace_id = "ws-1";
  EXPECT_TRUE(runtime.scenes().Upsert(scene).has_value());
  EXPECT_NE(runtime.scenes().Find("research"), nullptr);
}

}  // namespace
}  // namespace seoul
