// Project Seoul Adaptive UI (SAUI).
// Unit tests for the component catalog and document parsing/serialization.

#include "seoul/browser/saui/saui_document.h"

#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_limits.h"
#include "seoul/browser/saui/saui_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::Value MinimalSurfaceJson() {
  return base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "t1", "type": "text", "props": {"text": "hello"}}
    ]
  })json");
}

TEST(SauiCatalogTest, EveryTypeHasARowInDeclarationOrder) {
  for (size_t i = 0; i < ComponentTypeCount(); ++i) {
    const ComponentType type = static_cast<ComponentType>(i);
    const ComponentTypeInfo& info = GetComponentTypeInfo(type);
    EXPECT_EQ(info.type, type) << "catalog row " << i << " out of order";
    EXPECT_NE(info.name, nullptr);
    EXPECT_NE(std::string(info.name), "");
  }
}

TEST(SauiCatalogTest, NamesAreUniqueAndRoundTrip) {
  for (size_t i = 0; i < ComponentTypeCount(); ++i) {
    const ComponentType type = static_cast<ComponentType>(i);
    const ComponentTypeInfo* found =
        FindComponentTypeByName(ComponentTypeName(type));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->type, type);
  }
  EXPECT_EQ(FindComponentTypeByName("not_a_component"), nullptr);
  EXPECT_EQ(FindComponentTypeByName(""), nullptr);
}

TEST(SauiCatalogTest, ChartTypesRequireBindingAndAccessibleName) {
  for (size_t i = 0; i < ComponentTypeCount(); ++i) {
    const ComponentTypeInfo& info =
        GetComponentTypeInfo(static_cast<ComponentType>(i));
    if (info.chart) {
      EXPECT_TRUE(info.binding_required) << info.name;
      EXPECT_TRUE(info.requires_accessible_name) << info.name;
      EXPECT_NE(info.accepted_binding_kinds, kBindNone) << info.name;
    }
  }
}

TEST(SauiIdentifierTest, ValidatesCharsetAndBounds) {
  EXPECT_TRUE(IsValidSauiIdentifier("a"));
  EXPECT_TRUE(IsValidSauiIdentifier("chart-1_main"));
  EXPECT_FALSE(IsValidSauiIdentifier(""));
  EXPECT_FALSE(IsValidSauiIdentifier("1abc"));  // must start with a letter
  EXPECT_FALSE(IsValidSauiIdentifier("-abc"));
  EXPECT_FALSE(IsValidSauiIdentifier("a b"));
  EXPECT_FALSE(IsValidSauiIdentifier("a<script>"));
  EXPECT_FALSE(IsValidSauiIdentifier(std::string(65, 'a')));
}

TEST(SauiPropKeyTest, RejectsHandlerAndMarkupKeys) {
  EXPECT_TRUE(IsValidPropKey("text"));
  EXPECT_TRUE(IsValidPropKey("baseline_zero"));
  EXPECT_FALSE(IsValidPropKey("onclick"));
  EXPECT_FALSE(IsValidPropKey("onload"));
  EXPECT_FALSE(IsValidPropKey("on"));
  EXPECT_FALSE(IsValidPropKey("html"));
  EXPECT_FALSE(IsValidPropKey("script"));
  EXPECT_FALSE(IsValidPropKey("srcdoc"));
  EXPECT_FALSE(IsValidPropKey("innerhtml"));
  EXPECT_FALSE(IsValidPropKey("style"));
  EXPECT_FALSE(IsValidPropKey("Text"));  // lowercase only
  EXPECT_FALSE(IsValidPropKey(""));
}

TEST(SauiDocumentTest, ParsesMinimalSurface) {
  auto surface = ParseSurface(MinimalSurfaceJson());
  ASSERT_TRUE(surface.has_value());
  EXPECT_EQ(surface->kind, SurfaceKind::kResponse);
  ASSERT_EQ(surface->components.size(), 1u);
  EXPECT_EQ(surface->components[0].type, ComponentType::kText);
  EXPECT_TRUE(surface->id.is_valid());  // generated when absent
}

TEST(SauiDocumentTest, RejectsWrongSchemaVersionAndUnknownKind) {
  base::Value doc = MinimalSurfaceJson();
  doc.GetDict().Set("schema_version", 99);
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kUnsupportedSchemaVersion);

  doc = MinimalSurfaceJson();
  doc.GetDict().Set("kind", "hologram");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kUnknownSurfaceKind);
}

TEST(SauiDocumentTest, RejectsUnknownComponentType) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [{"id": "x", "type": "hologram_projector"}]
  })json");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kUnknownComponentType);
}

TEST(SauiDocumentTest, RejectsEventHandlerProps) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "x", "type": "text",
       "props": {"text": "hi", "onclick": "steal()"}}
    ]
  })json");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kForbiddenPropertyKey);
}

TEST(SauiDocumentTest, RejectsNonHttpUrls) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "x", "type": "link",
       "props": {"href": "javascript:alert(1)", "text": "click"}}
    ]
  })json");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kInvalidUrlProperty);

  doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "x", "type": "image",
       "props": {"src": "data:text/html,<script>1</script>", "alt": "a"}}
    ]
  })json");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kInvalidUrlProperty);
}

TEST(SauiDocumentTest, RejectsChildrenOnNonContainer) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "x", "type": "text", "props": {"text": "hi"},
       "children": [{"id": "y", "type": "text", "props": {"text": "no"}}]}
    ]
  })json");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kChildrenNotAllowed);
}

TEST(SauiDocumentTest, RejectsExcessiveNestingDepth) {
  // Build a stack nested beyond kMaxComponentDepth.
  base::DictValue leaf;
  leaf.Set("id", "leaf");
  leaf.Set("type", "text");
  base::DictValue leaf_props;
  leaf_props.Set("text", "deep");
  leaf.Set("props", std::move(leaf_props));
  base::DictValue current = std::move(leaf);
  for (size_t i = 0; i <= kMaxComponentDepth; ++i) {
    base::DictValue wrapper;
    wrapper.Set("id", "s" + std::to_string(i));
    wrapper.Set("type", "stack");
    base::ListValue children;
    children.Append(std::move(current));
    wrapper.Set("children", std::move(children));
    current = std::move(wrapper);
  }
  base::DictValue doc;
  doc.Set("schema_version", 1);
  doc.Set("kind", "response");
  base::ListValue components;
  components.Append(std::move(current));
  doc.Set("components", std::move(components));
  EXPECT_EQ(ParseSurface(base::Value(std::move(doc))).error(),
            SauiError::kDepthExceeded);
}

TEST(SauiDocumentTest, ParsesDataEntriesOfEveryKind) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "dashboard",
    "title": "Fixture",
    "components": [{"id": "t", "type": "text", "props": {"text": "x"}}],
    "data": {
      "temp": {"kind": "scalar", "value": 21.5},
      "quote": {"kind": "record", "fields": {"symbol": "TSTA", "px": 12.5}},
      "prices": {"kind": "series", "y_unit": "USD",
                 "points": [{"t_ms": 1000.0, "y": 1.0},
                            {"t_ms": 2000.0, "y": 2.0}],
                 "provenance": {"source_name": "fixture",
                                 "retrieved_at_ms": 5000.0,
                                 "effective_at_ms": 4000.0,
                                 "freshness": "delayed"}},
      "rows": {"kind": "table",
               "columns": [{"key": "name", "label": "Name"}],
               "rows": [["a"], ["b"]]}
    }
  })json");
  auto surface = ParseSurface(doc);
  ASSERT_TRUE(surface.has_value());
  EXPECT_EQ(surface->data.size(), 4u);
  EXPECT_EQ(surface->data["temp"].kind, DataEntryKind::kScalar);
  EXPECT_EQ(surface->data["quote"].kind, DataEntryKind::kRecord);
  ASSERT_EQ(surface->data["prices"].series.points.size(), 2u);
  EXPECT_TRUE(surface->data["prices"].has_provenance);
  EXPECT_EQ(surface->data["prices"].provenance.freshness,
            FreshnessState::kDelayed);
  ASSERT_EQ(surface->data["rows"].table.rows.size(), 2u);
}

TEST(SauiDocumentTest, RejectsMalformedSeriesPoints) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [{"id": "t", "type": "text", "props": {"text": "x"}}],
    "data": {
      "bad": {"kind": "series",
              "points": [{"t_ms": 1000.0, "x": 1.0, "y": 2.0}]}
    }
  })json");
  // A point may carry t_ms or x, never both.
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kInvalidDataEntry);
}

TEST(SauiDocumentTest, RejectsRaggedTableRows) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [{"id": "t", "type": "text", "props": {"text": "x"}}],
    "data": {
      "rows": {"kind": "table",
               "columns": [{"key": "a", "label": "A"},
                            {"key": "b", "label": "B"}],
               "rows": [["only-one-cell"]]}
    }
  })json");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kInvalidDataEntry);
}

TEST(SauiDocumentTest, ParsesActionsAndRejectsBadNavigationTarget) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [{"id": "t", "type": "text", "props": {"text": "x"}}],
    "actions": [
      {"id": "open", "label": "Open", "kind": "navigate",
       "target": "https://example.test/page"}
    ]
  })json");
  auto surface = ParseSurface(doc);
  ASSERT_TRUE(surface.has_value());
  ASSERT_EQ(surface->actions.size(), 1u);
  EXPECT_EQ(surface->actions[0].kind, SurfaceActionKind::kNavigate);

  doc.GetDict().FindList("actions")->front().GetDict().Set(
      "target", "javascript:alert(1)");
  EXPECT_EQ(ParseSurface(doc).error(), SauiError::kInvalidUrlProperty);
}

TEST(SauiDocumentTest, SerializationRoundTrips) {
  base::Value doc = base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "dashboard",
    "title": "Round Trip",
    "components": [
      {"id": "root", "type": "stack", "children": [
        {"id": "m", "type": "metric", "props": {"label": "Now"},
         "bindings": {"data": "temp"}, "accessible_name": "Current value",
         "update_policy": "live"}
      ]}
    ],
    "data": {"temp": {"kind": "scalar", "value": 3.5}},
    "actions": [{"id": "a", "label": "Do", "kind": "local_state",
                  "target": "refresh"}]
  })json");
  auto first = ParseSurface(doc);
  ASSERT_TRUE(first.has_value());
  base::DictValue serialized = SurfaceToValue(first.value());
  auto second = ParseSurface(base::Value(serialized.Clone()));
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first.value(), second.value());
}

}  // namespace
}  // namespace seoul
