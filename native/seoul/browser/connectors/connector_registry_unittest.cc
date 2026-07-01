// Project Seoul connected tools.
// Unit tests for connector connect/disconnect, tool mirroring, ownership
// enforcement, and connect rollback.

#include "seoul/browser/connectors/connector_registry.h"

#include <memory>

#include "seoul/browser/connectors/fake_connector.h"
#include "seoul/browser/tools/tool_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

std::unique_ptr<FakeConnector> CalendarConnector() {
  auto connector =
      std::make_unique<FakeConnector>("calendar", "user@example.com");
  connector->AddTool("list_events");
  connector->AddTool("create_event");
  return connector;
}

TEST(ConnectorRegistryTest, ConnectMirrorsToolsIntoRegistry) {
  ToolRegistry tools;
  ConnectorRegistry connectors(&tools);
  ASSERT_TRUE(connectors.Connect(CalendarConnector()).has_value());
  EXPECT_EQ(connectors.size(), 1u);
  EXPECT_NE(tools.Find(ToolId::FromString("connector.calendar.list_events")),
            nullptr);
  EXPECT_NE(tools.Find(ToolId::FromString("connector.calendar.create_event")),
            nullptr);
}

TEST(ConnectorRegistryTest, DisconnectRemovesExactlyThatProvidersTools) {
  ToolRegistry tools;
  ConnectorRegistry connectors(&tools);
  ASSERT_TRUE(connectors.Connect(CalendarConnector()).has_value());

  auto mail = std::make_unique<FakeConnector>("mail", "user@example.com");
  mail->AddTool("send");
  ASSERT_TRUE(connectors.Connect(std::move(mail)).has_value());
  EXPECT_EQ(tools.size(), 3u);

  ASSERT_TRUE(connectors.Disconnect("calendar").has_value());
  EXPECT_EQ(connectors.size(), 1u);
  EXPECT_EQ(tools.Find(ToolId::FromString("connector.calendar.list_events")),
            nullptr);
  EXPECT_NE(tools.Find(ToolId::FromString("connector.mail.send")), nullptr);
}

TEST(ConnectorRegistryTest, DuplicateAndReservedProvidersRejected) {
  ToolRegistry tools;
  ConnectorRegistry connectors(&tools);
  ASSERT_TRUE(connectors.Connect(CalendarConnector()).has_value());
  EXPECT_EQ(connectors.Connect(CalendarConnector()).error(),
            ConnectorError::kDuplicateProvider);

  auto reserved = std::make_unique<FakeConnector>("seoul", "x");
  reserved->AddTool("evil");
  EXPECT_EQ(connectors.Connect(std::move(reserved)).error(),
            ConnectorError::kReservedProvider);

  EXPECT_EQ(connectors.Disconnect("never_connected").error(),
            ConnectorError::kUnknownProvider);
}

TEST(ConnectorRegistryTest, MalformedToolRollsBackTheWholeConnect) {
  ToolRegistry tools;
  ConnectorRegistry connectors(&tools);
  auto connector = std::make_unique<FakeConnector>("notes", "user@example.com");
  connector->AddTool("search");                         // valid
  connector->AddTool("broken", /*well_formed=*/false);  // invalid schema
  EXPECT_EQ(connectors.Connect(std::move(connector)).error(),
            ConnectorError::kToolRegistrationFailed);
  // Neither tool survives; connect is atomic.
  EXPECT_EQ(connectors.size(), 0u);
  EXPECT_EQ(tools.size(), 0u);
}

TEST(ConnectorRegistryTest, RefreshReRegistersTools) {
  ToolRegistry tools;
  ConnectorRegistry connectors(&tools);
  ASSERT_TRUE(connectors.Connect(CalendarConnector()).has_value());
  ASSERT_TRUE(connectors.Refresh("calendar").has_value());
  EXPECT_EQ(tools.size(), 2u);  // no duplication after re-register
  EXPECT_EQ(connectors.Refresh("unknown").error(),
            ConnectorError::kUnknownProvider);
}

TEST(ConnectorRegistryTest, ConnectorToolsBecomeAvailableUnderPermission) {
  ToolRegistry tools;
  ConnectorRegistry connectors(&tools);
  ASSERT_TRUE(connectors.Connect(CalendarConnector()).has_value());

  ToolPermissionContext without;
  without.max_sensitivity = DataSensitivity::kPersonal;
  without.allow_network = true;
  // No connected providers listed: connector tools are hidden.
  EXPECT_TRUE(tools.ListAvailable(without).empty());

  ToolPermissionContext with;
  with.max_sensitivity = DataSensitivity::kPersonal;
  with.allow_network = true;
  with.connected_providers = {"calendar"};
  EXPECT_EQ(tools.ListAvailable(with).size(), 2u);
}

}  // namespace
}  // namespace seoul
