// Project Seoul connected tools.
// Unit tests for the generic capability paths: JSON Schema import, OpenAPI
// operation import, MCP tool import, local-file selection, and the browser/
// information capability catalogs. The OpenAPI/MCP fixtures describe a
// deliberately unfamiliar service (an aquarium sensor hub) to prove the
// importers carry no domain assumptions.

#include "base/test/values_test_util.h"
#include "seoul/browser/connectors/generic_capabilities.h"
#include "seoul/browser/connectors/local_file_capability.h"
#include "seoul/browser/connectors/mcp_capability_importer.h"
#include "seoul/browser/connectors/openapi_capability_importer.h"
#include "seoul/browser/tools/tool_registry.h"
#include "seoul/browser/tools/tool_schema_from_json.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(JsonSchemaImportTest, ConvertsSupportedSubset) {
  base::Value schema = base::test::ParseJson(R"json({
    "type": "object",
    "properties": {
      "tank_id": {"type": "string", "description": "Which tank."},
      "depth_m": {"type": "number", "minimum": 0, "maximum": 50},
      "sensor_count": {"type": "integer"},
      "active": {"type": "boolean"},
      "docs_url": {"type": "string", "format": "uri"},
      "mode": {"type": "string", "enum": ["continuous", "sampled"]},
      "labels": {"type": "array", "items": {"type": "string"}},
      "window": {"type": "object", "properties": {
        "start_ms": {"type": "number"},
        "end_ms": {"type": "number"}
      }}
    },
    "required": ["tank_id", "depth_m"]
  })json");
  auto imported = ToolSchemaFromJsonSchema(schema.GetDict());
  ASSERT_TRUE(imported.has_value());
  EXPECT_TRUE(IsWellFormedSchema(imported.value()));
  ASSERT_EQ(imported->fields.size(), 8u);

  bool found_required = false;
  bool found_url = false;
  bool found_enum = false;
  for (const SchemaField& field : imported->fields) {
    if (field.name == "tank_id") {
      found_required = field.required;
    }
    if (field.name == "docs_url") {
      found_url = field.kind == SchemaFieldKind::kUrl;
    }
    if (field.name == "mode") {
      found_enum = field.kind == SchemaFieldKind::kEnum &&
                   field.enum_values.size() == 2;
    }
  }
  EXPECT_TRUE(found_required);
  EXPECT_TRUE(found_url);
  EXPECT_TRUE(found_enum);
}

TEST(JsonSchemaImportTest, RejectsUnsupportedConstructsPrecisely) {
  base::Value not_object = base::test::ParseJson(R"json({
    "type": "string"
  })json");
  EXPECT_EQ(ToolSchemaFromJsonSchema(not_object.GetDict()).error().error,
            JsonSchemaImportError::kNotAnObjectSchema);

  base::Value bad_type = base::test::ParseJson(R"json({
    "type": "object",
    "properties": {"blob": {"type": "null"}}
  })json");
  EXPECT_EQ(ToolSchemaFromJsonSchema(bad_type.GetDict()).error().error,
            JsonSchemaImportError::kUnsupportedType);

  base::Value bad_name = base::test::ParseJson(R"json({
    "type": "object",
    "properties": {"Bad Name": {"type": "string"}}
  })json");
  EXPECT_EQ(ToolSchemaFromJsonSchema(bad_name.GetDict()).error().error,
            JsonSchemaImportError::kInvalidPropertyName);
}

constexpr std::string_view kAquariumOpenApi = R"json({
  "openapi": "3.1.0",
  "info": {"title": "Aquarium Sensor Hub", "version": "2.0.0"},
  "paths": {
    "/tanks/{tank_id}/readings": {
      "get": {
        "operationId": "listReadings",
        "summary": "List sensor readings",
        "parameters": [
          {"name": "tank_id", "in": "path", "required": true,
           "schema": {"type": "string"}},
          {"name": "since_ms", "in": "query",
           "schema": {"type": "number"}}
        ]
      }
    },
    "/tanks/{tank_id}/feed": {
      "post": {
        "operationId": "triggerFeeding",
        "summary": "Trigger a feeding cycle",
        "parameters": [
          {"name": "tank_id", "in": "path", "required": true,
           "schema": {"type": "string"}}
        ],
        "requestBody": {"content": {"application/json": {"schema": {
          "type": "object",
          "properties": {"grams": {"type": "number", "minimum": 1,
                                     "maximum": 50}},
          "required": ["grams"]
        }}}}
      }
    }
  }
})json";

TEST(OpenApiImportTest, ImportsOperationsWithHonestRisk) {
  base::Value document = base::test::ParseJson(kAquariumOpenApi);
  auto imported =
      ImportOpenApiCapabilities(document.GetDict(), "aquarium");
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->size(), 2u);

  const OpenApiOperationBinding* get_binding = nullptr;
  const OpenApiOperationBinding* post_binding = nullptr;
  for (const OpenApiOperationBinding& binding : imported.value()) {
    if (binding.http_method == "get") {
      get_binding = &binding;
    } else {
      post_binding = &binding;
    }
  }
  ASSERT_NE(get_binding, nullptr);
  ASSERT_NE(post_binding, nullptr);

  EXPECT_EQ(get_binding->descriptor.id.value(),
            "connector.aquarium.listreadings");
  EXPECT_EQ(get_binding->descriptor.risk, RiskCategory::kReadOnly);
  EXPECT_EQ(get_binding->descriptor.approval,
            ApprovalPolicy::kNeverRequired);
  EXPECT_EQ(get_binding->path_template, "/tanks/{tank_id}/readings");

  // A remote mutation imports as an approval-gated external side effect.
  EXPECT_EQ(post_binding->descriptor.risk,
            RiskCategory::kExternalSideEffect);
  EXPECT_EQ(post_binding->descriptor.approval,
            ApprovalPolicy::kAlwaysRequired);
  // Path parameter and body property merged into one typed input schema.
  ASSERT_EQ(post_binding->descriptor.input_schema.fields.size(), 2u);
  EXPECT_TRUE(IsWellFormedSchema(post_binding->descriptor.input_schema));
}

TEST(OpenApiImportTest, ImportedCapabilitiesRegisterUnderTheProvider) {
  base::Value document = base::test::ParseJson(kAquariumOpenApi);
  auto imported =
      ImportOpenApiCapabilities(document.GetDict(), "aquarium");
  ASSERT_TRUE(imported.has_value());
  ToolRegistry registry;
  for (const OpenApiOperationBinding& binding : imported.value()) {
    ASSERT_TRUE(registry.Register(binding.descriptor).has_value())
        << binding.descriptor.id.value();
  }
  EXPECT_EQ(registry.size(), 2u);
}

TEST(OpenApiImportTest, RejectsDefectsPrecisely) {
  base::Value not_openapi = base::test::ParseJson(R"json({"paths": {}})json");
  EXPECT_EQ(
      ImportOpenApiCapabilities(not_openapi.GetDict(), "x").error().error,
      OpenApiImportError::kNotAnOpenApiDocument);

  base::Value no_id = base::test::ParseJson(R"json({
    "openapi": "3.1.0",
    "paths": {"/a": {"get": {"summary": "no operation id"}}}
  })json");
  EXPECT_EQ(ImportOpenApiCapabilities(no_id.GetDict(), "x").error().error,
            OpenApiImportError::kMissingOperationId);

  base::Value duplicate = base::test::ParseJson(R"json({
    "openapi": "3.1.0",
    "paths": {
      "/a": {"get": {"operationId": "same"}},
      "/b": {"get": {"operationId": "same"}}
    }
  })json");
  EXPECT_EQ(
      ImportOpenApiCapabilities(duplicate.GetDict(), "x").error().error,
      OpenApiImportError::kDuplicateOperationId);

  base::Value bad_location = base::test::ParseJson(R"json({
    "openapi": "3.1.0",
    "paths": {"/a": {"get": {"operationId": "op",
      "parameters": [{"name": "auth", "in": "header",
                       "schema": {"type": "string"}}]}}}
  })json");
  EXPECT_EQ(
      ImportOpenApiCapabilities(bad_location.GetDict(), "x").error().error,
      OpenApiImportError::kUnsupportedParameterLocation);
}

TEST(McpImportTest, ImportsToolListWithAnnotations) {
  base::Value tools = base::test::ParseJson(R"json({
    "tools": [
      {"name": "query_readings",
       "description": "Query recent readings from the hub.",
       "annotations": {"readOnlyHint": true},
       "inputSchema": {"type": "object", "properties": {
         "limit": {"type": "integer", "minimum": 1, "maximum": 100}
       }}},
      {"name": "calibrate-sensor",
       "description": "Recalibrates one sensor.",
       "inputSchema": {"type": "object", "properties": {
         "sensor": {"type": "string"}
       }, "required": ["sensor"]}}
    ]
  })json");
  auto imported = ImportMcpCapabilities(tools.GetDict(), "hub");
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->size(), 2u);

  EXPECT_EQ((*imported)[0].id.value(), "connector.hub.query_readings");
  EXPECT_EQ((*imported)[0].risk, RiskCategory::kReadOnly);
  // The dash sanitizes to an underscore; risk defaults to gated side effect.
  EXPECT_EQ((*imported)[1].id.value(), "connector.hub.calibrate_sensor");
  EXPECT_EQ((*imported)[1].risk, RiskCategory::kExternalSideEffect);
  EXPECT_EQ((*imported)[1].approval, ApprovalPolicy::kAlwaysRequired);

  ToolRegistry registry;
  for (const ToolDescriptor& descriptor : imported.value()) {
    ASSERT_TRUE(registry.Register(descriptor).has_value());
  }
  EXPECT_EQ(registry.size(), 2u);
}

TEST(McpImportTest, RejectsDefects) {
  base::Value empty = base::test::ParseJson(R"json({"tools": []})json");
  EXPECT_EQ(ImportMcpCapabilities(empty.GetDict(), "hub").error().error,
            McpImportError::kNoTools);

  base::Value nameless = base::test::ParseJson(R"json({
    "tools": [{"description": "no name"}]
  })json");
  EXPECT_EQ(ImportMcpCapabilities(nameless.GetDict(), "hub").error().error,
            McpImportError::kMissingName);

  base::Value duplicate = base::test::ParseJson(R"json({
    "tools": [
      {"name": "a_b", "description": "one"},
      {"name": "a-b", "description": "two"}
    ]
  })json");
  // Sanitization collides a_b and a-b: rejected, never silently merged.
  EXPECT_EQ(ImportMcpCapabilities(duplicate.GetDict(), "hub").error().error,
            McpImportError::kDuplicateName);
}

TEST(LocalFileCapabilityTest, SelectionTokensGateAllReads) {
  SelectedFileRegistry registry;
  auto token = registry.Select("notes.txt", "text/plain", 1204);
  ASSERT_TRUE(token.has_value());
  EXPECT_EQ(registry.size(), 1u);

  // Reads resolve only registered tokens.
  EXPECT_TRUE(ResolveReadRequest(registry, token.value()).has_value());
  EXPECT_EQ(ResolveReadRequest(registry, "invented-token").error(),
            FileSelectionError::kUnknownToken);

  // Revocation is immediate.
  ASSERT_TRUE(registry.Revoke(token.value()));
  EXPECT_EQ(ResolveReadRequest(registry, token.value()).error(),
            FileSelectionError::kUnknownToken);

  // The registry never stores or exposes a path.
  auto second = registry.Select("report.pdf", "application/pdf", 5000);
  ASSERT_TRUE(second.has_value());
  const SelectedFile* file = registry.Find(second.value());
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->display_name.find('/'), std::string::npos);
}

TEST(GenericCapabilityCatalogTest, CatalogsRegisterAndCarryHonestRisk) {
  ToolRegistry registry;
  for (const ToolDescriptor& descriptor : BuildBrowserCapabilities()) {
    ASSERT_TRUE(registry.Register(descriptor).has_value())
        << descriptor.id.value();
  }
  for (const ToolDescriptor& descriptor : BuildInformationCapabilities()) {
    ASSERT_TRUE(registry.Register(descriptor).has_value())
        << descriptor.id.value();
  }
  for (const ToolDescriptor& descriptor : BuildLocalFileCapabilities()) {
    ASSERT_TRUE(registry.Register(descriptor).has_value())
        << descriptor.id.value();
  }

  // Form submission always requires approval; observation never does.
  const ToolDescriptor* submit =
      registry.Find(ToolId::FromString("page.act.submit"));
  ASSERT_NE(submit, nullptr);
  EXPECT_EQ(submit->approval, ApprovalPolicy::kAlwaysRequired);
  EXPECT_EQ(submit->risk, RiskCategory::kExternalSideEffect);

  const ToolDescriptor* observe =
      registry.Find(ToolId::FromString("page.observe.text"));
  ASSERT_NE(observe, nullptr);
  EXPECT_EQ(observe->risk, RiskCategory::kReadOnly);

  // Offline permission context: network capabilities are filtered out.
  ToolPermissionContext offline;
  offline.max_sensitivity = DataSensitivity::kPersonal;
  offline.allow_network = false;
  for (const ToolDescriptor* descriptor : registry.ListAvailable(offline)) {
    EXPECT_FALSE(descriptor->requires_network) << descriptor->id.value();
  }
}

}  // namespace
}  // namespace seoul
