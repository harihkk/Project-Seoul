// Project Seoul general-purpose operator: tool layer.
// Unit tests for typed schemas, argument validation, and the tool registry.

#include "seoul/browser/tools/tool_registry.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "seoul/browser/tools/tool_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

SchemaField StringField(const std::string& name, bool required) {
  SchemaField field;
  field.name = name;
  field.kind = SchemaFieldKind::kString;
  field.required = required;
  return field;
}

ToolSchema SearchSchema() {
  ToolSchema schema;
  schema.fields.push_back(StringField("query", true));
  SchemaField max_results;
  max_results.name = "max_results";
  max_results.kind = SchemaFieldKind::kInteger;
  max_results.has_range = true;
  max_results.min_value = 1;
  max_results.max_value = 50;
  schema.fields.push_back(max_results);
  SchemaField source;
  source.name = "source";
  source.kind = SchemaFieldKind::kEnum;
  source.enum_values = {"web", "history", "tabs"};
  schema.fields.push_back(source);
  SchemaField url;
  url.name = "site";
  url.kind = SchemaFieldKind::kUrl;
  schema.fields.push_back(url);
  SchemaField filters;
  filters.name = "filters";
  filters.kind = SchemaFieldKind::kObject;
  SchemaField max_price;
  max_price.name = "max_price";
  max_price.kind = SchemaFieldKind::kNumber;
  max_price.has_range = true;
  max_price.min_value = 0;
  max_price.max_value = 1e9;
  filters.children.push_back(max_price);
  schema.fields.push_back(filters);
  SchemaField tags;
  tags.name = "tags";
  tags.kind = SchemaFieldKind::kList;
  tags.children.push_back(StringField("item", false));
  schema.fields.push_back(tags);
  return schema;
}

ToolDescriptor SearchTool() {
  ToolDescriptor descriptor;
  descriptor.id = ToolId::FromString("info.search.web");
  descriptor.name = "Web search";
  descriptor.description = "Searches the configured engine.";
  descriptor.provider = "seoul";
  descriptor.input_schema = SearchSchema();
  descriptor.requires_network = true;
  descriptor.sensitivity = DataSensitivity::kNone;
  descriptor.risk = RiskCategory::kReadOnly;
  descriptor.observation_contract = "returns ranked result list";
  return descriptor;
}

TEST(ToolIdTest, ValidatesShape) {
  EXPECT_TRUE(ToolId::IsValidString("browser.tabs.open"));
  EXPECT_TRUE(ToolId::IsValidString("connector.calendar.list_events"));
  EXPECT_TRUE(ToolId::IsValidString("info.search"));
  EXPECT_FALSE(ToolId::IsValidString("single"));
  EXPECT_FALSE(ToolId::IsValidString(".leading.dot"));
  EXPECT_FALSE(ToolId::IsValidString("trailing.dot."));
  EXPECT_FALSE(ToolId::IsValidString("Upper.Case"));
  EXPECT_FALSE(ToolId::IsValidString("a.b.c.d.e"));  // too many segments
  EXPECT_FALSE(ToolId::IsValidString("has.1leading.digit"));
  EXPECT_FALSE(ToolId::IsValidString(""));
  EXPECT_EQ(ToolId::FromString("browser.tabs.open").root_namespace(),
            "browser");
}

TEST(ToolSchemaTest, ValidatesArgsAgainstDeclaredFields) {
  const ToolSchema schema = SearchSchema();
  base::Value::Dict args;
  args.Set("query", "cheap monitors");
  args.Set("max_results", 10);
  args.Set("source", "web");
  args.Set("site", "https://example.test");
  base::Value::Dict filters;
  filters.Set("max_price", 500.0);
  args.Set("filters", std::move(filters));
  base::Value::List tags;
  tags.Append("displays");
  args.Set("tags", std::move(tags));
  EXPECT_TRUE(ValidateArgs(schema, args).has_value());
}

TEST(ToolSchemaTest, RejectsUnknownAndMissingAndWrongKind) {
  const ToolSchema schema = SearchSchema();
  base::Value::Dict args;
  args.Set("query", "x");
  args.Set("invented_field", 1);
  auto result = ValidateArgs(schema, args);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kUnknownField);
  EXPECT_EQ(result.error().field_path, "invented_field");

  base::Value::Dict missing;
  result = ValidateArgs(schema, missing);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kMissingRequiredField);
  EXPECT_EQ(result.error().field_path, "query");

  base::Value::Dict wrong;
  wrong.Set("query", 42);
  result = ValidateArgs(schema, wrong);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kWrongKind);
}

TEST(ToolSchemaTest, RejectsRangeEnumUrlAndNestedViolations) {
  const ToolSchema schema = SearchSchema();
  base::Value::Dict args;
  args.Set("query", "x");
  args.Set("max_results", 500);
  auto result = ValidateArgs(schema, args);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kOutOfRange);

  args = base::Value::Dict();
  args.Set("query", "x");
  args.Set("source", "carrier_pigeon");
  result = ValidateArgs(schema, args);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kNotInEnum);

  args = base::Value::Dict();
  args.Set("query", "x");
  args.Set("site", "javascript:alert(1)");
  result = ValidateArgs(schema, args);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kInvalidUrl);

  args = base::Value::Dict();
  args.Set("query", "x");
  base::Value::Dict filters;
  filters.Set("max_price", -5.0);
  args.Set("filters", std::move(filters));
  result = ValidateArgs(schema, args);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().kind, SchemaViolationKind::kOutOfRange);
  EXPECT_EQ(result.error().field_path, "filters.max_price");
}

TEST(ToolSchemaTest, MalformedSchemasAreRejectedUpFront) {
  ToolSchema duplicate;
  duplicate.fields.push_back(StringField("query", true));
  duplicate.fields.push_back(StringField("query", false));
  EXPECT_FALSE(IsWellFormedSchema(duplicate));

  ToolSchema bad_enum;
  SchemaField field;
  field.name = "mode";
  field.kind = SchemaFieldKind::kEnum;
  bad_enum.fields.push_back(field);  // no enum values
  EXPECT_FALSE(IsWellFormedSchema(bad_enum));

  ToolSchema bad_name;
  bad_name.fields.push_back(StringField("BadName", false));
  EXPECT_FALSE(IsWellFormedSchema(bad_name));
}

TEST(ToolRegistryTest, RegisterFindAndDuplicate) {
  ToolRegistry registry;
  ASSERT_TRUE(registry.Register(SearchTool()).has_value());
  EXPECT_NE(registry.Find(ToolId::FromString("info.search.web")), nullptr);
  EXPECT_EQ(registry.Find(ToolId::FromString("info.search.images")), nullptr);
  EXPECT_EQ(registry.Register(SearchTool()).error(), ToolError::kDuplicateId);
}

TEST(ToolRegistryTest, ReservedNamespacesAndConnectorOwnership) {
  ToolRegistry registry;
  ToolDescriptor imposter = SearchTool();
  imposter.id = ToolId::FromString("browser.tabs.close_all");
  imposter.provider = "some_connector";
  EXPECT_EQ(registry.Register(imposter).error(), ToolError::kReservedNamespace);

  ToolDescriptor mismatched = SearchTool();
  mismatched.id = ToolId::FromString("connector.calendar.list_events");
  mismatched.provider = "mail";  // does not own connector.calendar.*
  EXPECT_EQ(registry.Register(mismatched).error(),
            ToolError::kProviderMismatch);

  ToolDescriptor owned = SearchTool();
  owned.id = ToolId::FromString("connector.calendar.list_events");
  owned.provider = "calendar";
  owned.sensitivity = DataSensitivity::kPersonal;
  EXPECT_TRUE(registry.Register(owned).has_value());

  ToolDescriptor rogue_root = SearchTool();
  rogue_root.id = ToolId::FromString("shell.exec.command");
  EXPECT_EQ(registry.Register(rogue_root).error(),
            ToolError::kReservedNamespace);
}

TEST(ToolRegistryTest, ListAvailableFiltersBySensitivityNetworkProvider) {
  ToolRegistry registry;
  ASSERT_TRUE(registry.Register(SearchTool()).has_value());  // network

  ToolDescriptor local_tool = SearchTool();
  local_tool.id = ToolId::FromString("browser.tabs.enumerate");
  local_tool.requires_network = false;
  local_tool.sensitivity = DataSensitivity::kOrganization;
  ASSERT_TRUE(registry.Register(local_tool).has_value());

  ToolDescriptor calendar_tool = SearchTool();
  calendar_tool.id = ToolId::FromString("connector.calendar.list_events");
  calendar_tool.provider = "calendar";
  calendar_tool.sensitivity = DataSensitivity::kPersonal;
  ASSERT_TRUE(registry.Register(calendar_tool).has_value());

  ToolPermissionContext offline;
  offline.max_sensitivity = DataSensitivity::kOrganization;
  offline.allow_network = false;
  auto available = registry.ListAvailable(offline);
  ASSERT_EQ(available.size(), 1u);
  EXPECT_EQ(available[0]->id.value(), "browser.tabs.enumerate");

  ToolPermissionContext connected;
  connected.max_sensitivity = DataSensitivity::kPersonal;
  connected.allow_network = true;
  connected.connected_providers = {"calendar"};
  available = registry.ListAvailable(connected);
  EXPECT_EQ(available.size(), 3u);

  // Disconnecting the provider removes its tools from discovery and from the
  // registry when unregistered.
  connected.connected_providers.clear();
  EXPECT_EQ(registry.ListAvailable(connected).size(), 2u);
  EXPECT_EQ(registry.UnregisterProvider("calendar"), 1u);
  EXPECT_EQ(registry.Find(ToolId::FromString("connector.calendar.list_events")),
            nullptr);
}

TEST(CapabilityGraphTest, UnavailableCapabilitiesAreHiddenFromPlanning) {
  ToolRegistry registry;
  ASSERT_TRUE(registry.Register(SearchTool()).has_value());
  const ToolId id = ToolId::FromString("info.search.web");

  ToolPermissionContext context;
  context.max_sensitivity = DataSensitivity::kPersonal;
  context.allow_network = true;
  ASSERT_EQ(registry.ListAvailable(context).size(), 1u);

  // Provider outage: the capability stays registered but is never offered.
  ASSERT_TRUE(registry.SetAvailability(
      id, AvailabilityState::kUnavailable, "provider outage"));
  EXPECT_TRUE(registry.ListAvailable(context).empty());
  EXPECT_NE(registry.Find(id), nullptr);  // still resolvable for display
  EXPECT_EQ(registry.GetAvailability(id), AvailabilityState::kUnavailable);

  // Degraded capabilities remain plannable.
  ASSERT_TRUE(registry.SetAvailability(id, AvailabilityState::kDegraded,
                                       "slow backend"));
  EXPECT_EQ(registry.ListAvailable(context).size(), 1u);

  EXPECT_FALSE(registry.SetAvailability(ToolId::FromString("info.none.x"),
                                        AvailabilityState::kAvailable, ""));
}

TEST(CapabilityGraphTest, HealthStateIsTracked) {
  ToolRegistry registry;
  ASSERT_TRUE(registry.Register(SearchTool()).has_value());
  const ToolId id = ToolId::FromString("info.search.web");
  EXPECT_EQ(registry.GetHealth(id), HealthState::kUnknown);
  ASSERT_TRUE(registry.SetHealth(id, HealthState::kUnhealthy));
  EXPECT_EQ(registry.GetHealth(id), HealthState::kUnhealthy);
  EXPECT_EQ(registry.GetHealth(ToolId::FromString("info.none.x")),
            HealthState::kUnknown);
}

TEST(CapabilityGraphTest, VersionNegotiationRejectsTooOldCapabilities) {
  ToolRegistry registry;
  ToolDescriptor tool = SearchTool();
  tool.version = 2;
  ASSERT_TRUE(registry.Register(tool).has_value());
  const ToolId id = ToolId::FromString("info.search.web");
  EXPECT_NE(registry.FindCompatible(id, 1), nullptr);
  EXPECT_NE(registry.FindCompatible(id, 2), nullptr);
  EXPECT_EQ(registry.FindCompatible(id, 3), nullptr);
}

TEST(CapabilityGraphTest, DynamicUpdateKeepsOwnershipAndValidates) {
  ToolRegistry registry;
  ASSERT_TRUE(registry.Register(SearchTool()).has_value());

  // The owning provider may update the descriptor in place.
  ToolDescriptor updated = SearchTool();
  updated.version = 2;
  updated.description = "Searches with pagination support.";
  ASSERT_TRUE(registry.UpdateDescriptor(updated).has_value());
  EXPECT_EQ(registry.Find(ToolId::FromString("info.search.web"))->version,
            2);
  EXPECT_EQ(registry.size(), 1u);

  // Another provider can never take over the id.
  ToolDescriptor takeover = SearchTool();
  takeover.provider = "rogue";
  EXPECT_EQ(registry.UpdateDescriptor(takeover).error(),
            ToolError::kReservedNamespace);  // seoul namespace, rogue owner

  ToolDescriptor unknown = SearchTool();
  unknown.id = ToolId::FromString("info.search.images");
  EXPECT_EQ(registry.UpdateDescriptor(unknown).error(),
            ToolError::kUnknownTool);
}

TEST(ToolRegistryTest, InvalidDescriptorsAreRejected) {
  ToolRegistry registry;
  ToolDescriptor no_description = SearchTool();
  no_description.description.clear();
  EXPECT_EQ(registry.Register(no_description).error(),
            ToolError::kInvalidDescriptor);

  ToolDescriptor bad_schema = SearchTool();
  bad_schema.input_schema.fields.push_back(StringField("query", false));
  EXPECT_EQ(registry.Register(bad_schema).error(),
            ToolError::kInvalidDescriptor);

  ToolDescriptor bad_id = SearchTool();
  bad_id.id = ToolId::FromString("not a tool id");
  EXPECT_EQ(registry.Register(bad_id).error(), ToolError::kInvalidId);
}

}  // namespace
}  // namespace seoul
