// Project Seoul Adaptive UI (SAUI).
// Held-out generalization tests for the adaptive interface compiler. Every
// fixture below is an UNSEEN schema: server telemetry, league standings,
// housing listings, election tallies, an org chart, a dependency graph, a
// citation set. The production compiler contains no knowledge of any of
// these subjects; if these tests pass, they pass purely through shape and
// role reasoning. Do not add fixture-specific logic to production to make a
// test pass; that defeats the point of the suite.

#include "seoul/browser/saui/interface_compiler.h"

#include <algorithm>
#include <set>

#include "base/test/values_test_util.h"
#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_document.h"
#include "seoul/browser/saui/saui_validator.h"
#include "seoul/browser/saui/semantic_to_saui.h"
#include "seoul/browser/semantic/semantic_validation.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

FieldSpec Field(const std::string& id,
                FieldPrimitive primitive,
                SemanticRole role,
                bool nullable = true) {
  FieldSpec field;
  field.id = id;
  field.label = id;
  field.primitive = primitive;
  field.role = role;
  field.nullable = nullable;
  return field;
}

void SetFixtureProvenance(SemanticResult* result) {
  result->provenance.base.source_name = "fixture-capability";
  result->provenance.base.source_url = "https://source.test/data";
  result->provenance.base.retrieved_at =
      base::Time::UnixEpoch() + base::Days(20000);
  result->provenance.base.effective_at = result->provenance.base.retrieved_at;
}

bool HasReason(const CompiledInterface& compiled, CompilerReason reason) {
  return std::find(compiled.reasons.begin(), compiled.reasons.end(), reason) !=
         compiled.reasons.end();
}

// The primary component under the root stack.
const ComponentNode* Primary(const CompiledInterface& compiled) {
  if (compiled.surface.components.empty() ||
      compiled.surface.components[0].children.empty()) {
    return nullptr;
  }
  return &compiled.surface.components[0].children[0];
}

SemanticResult TelemetryTimeSeries() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kTimeSeries;
  result.schema.fields = {Field("sampled_at", FieldPrimitive::kTimestamp,
                                SemanticRole::kTimestamp, false),
                          Field("queue_depth", FieldPrimitive::kNumber,
                                SemanticRole::kMeasure, false)};
  result.schema.fields[1].unit = "jobs";
  result.data = base::test::ParseJson(R"json([
    {"sampled_at": 1000.0, "queue_depth": 12.0},
    {"sampled_at": 2000.0, "queue_depth": 19.0},
    {"sampled_at": 3000.0, "queue_depth": 7.0}
  ])json");
  SetFixtureProvenance(&result);
  return result;
}

SemanticResult LeagueStandings() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {
      Field("club_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("club", FieldPrimitive::kString, SemanticRole::kName, false),
      Field("points", FieldPrimitive::kInteger, SemanticRole::kCount, false)};
  result.data = base::test::ParseJson(R"json([
    {"club_id": "n", "club": "North", "points": 31},
    {"club_id": "s", "club": "South", "points": 28},
    {"club_id": "e", "club": "East", "points": 22}
  ])json");
  SetFixtureProvenance(&result);
  return result;
}

SemanticResult HousingListings() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kGeoFeatures;
  result.schema.fields = {
      Field("listing_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("address", FieldPrimitive::kString, SemanticRole::kName),
      Field("lat", FieldPrimitive::kNumber, SemanticRole::kLatitude, false),
      Field("lon", FieldPrimitive::kNumber, SemanticRole::kLongitude, false),
      Field("asking", FieldPrimitive::kNumber, SemanticRole::kMoney)};
  result.schema.fields[4].currency_code = "USD";
  result.data = base::test::ParseJson(R"json([
    {"listing_id": "l1", "address": "12 Elm", "lat": 37.1, "lon": -122.0,
     "asking": 400000.0},
    {"listing_id": "l2", "address": "9 Oak", "lat": 37.2, "lon": -122.1,
     "asking": 380000.0}
  ])json");
  SetFixtureProvenance(&result);
  return result;
}

SemanticResult ElectionTallies() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {
      Field("option_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("option", FieldPrimitive::kString, SemanticRole::kCategory, false),
      Field("votes", FieldPrimitive::kInteger, SemanticRole::kCount, false)};
  result.data = base::test::ParseJson(R"json([
    {"option_id": "a", "option": "Alpha", "votes": 5210},
    {"option_id": "b", "option": "Beta", "votes": 4790},
    {"option_id": "c", "option": "Gamma", "votes": 1013},
    {"option_id": "d", "option": "Delta", "votes": 512},
    {"option_id": "e", "option": "Epsilon", "votes": 77},
    {"option_id": "f", "option": "Zeta", "votes": 41},
    {"option_id": "g", "option": "Eta", "votes": 12},
    {"option_id": "h", "option": "Theta", "votes": 3}
  ])json");
  SetFixtureProvenance(&result);
  return result;
}

TEST(InterfaceCompilerTest, TemporalMeasureBecomesALineChart) {
  auto compiled = CompileInterface(TelemetryTimeSeries(), InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->type, ComponentType::kLineChart);
  EXPECT_TRUE(
      HasReason(compiled.value(), CompilerReason::kTemporalMeasureLineChart));
  // Chart honesty props were derived from field semantics, not a domain.
  EXPECT_EQ(*primary->props.FindString("units"), "jobs");
  EXPECT_TRUE(ValidateSurface(compiled->surface).has_value());
}

TEST(InterfaceCompilerTest, FewComparableEntitiesBecomeAMatrix) {
  auto compiled = CompileInterface(LeagueStandings(), InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->type, ComponentType::kComparisonMatrix);
  EXPECT_TRUE(
      HasReason(compiled.value(), CompilerReason::kComparableEntitiesMatrix));
}

TEST(InterfaceCompilerTest, GeospatialSemanticsBecomeAMap) {
  auto compiled = CompileInterface(HousingListings(), InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->type, ComponentType::kMap);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kGeoMap));
  // A bounded marker list accompanies small feature sets.
  EXPECT_EQ(compiled->surface.components[0].children.size(), 2u);
}

TEST(InterfaceCompilerTest, LargerCollectionsBecomeSortableTables) {
  auto compiled = CompileInterface(ElectionTallies(), InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->type, ComponentType::kSortableTable);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kCollectionTable));
}

TEST(InterfaceCompilerTest, UserRepresentationRequestIsHonoredWhenHonest) {
  InterfaceIntent intent;
  intent.requested_representation = ComponentType::kBarChart;
  auto compiled = CompileInterface(ElectionTallies(), intent);
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->type, ComponentType::kBarChart);
  EXPECT_TRUE(HasReason(compiled.value(),
                        CompilerReason::kUserRequestedRepresentation));
  // Axis honesty is preserved on user-requested bars.
  EXPECT_TRUE(primary->props.FindBool("baseline_zero").value_or(false));
}

TEST(InterfaceCompilerTest, MisleadingChartRequestFallsBack) {
  SemanticResult single_point = TelemetryTimeSeries();
  base::ListValue* rows = single_point.data.GetIfList();
  while (rows->size() > 1) {
    rows->erase(rows->begin());
  }
  InterfaceIntent intent;
  intent.requested_representation = ComponentType::kLineChart;
  auto compiled = CompileInterface(single_point, intent);
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_NE(primary->type, ComponentType::kLineChart);
  EXPECT_TRUE(HasReason(compiled.value(),
                        CompilerReason::kUserRequestRejectedMisleading));
}

TEST(InterfaceCompilerTest, UnattributedSeriesFallsBackToTable) {
  SemanticResult unattributed = TelemetryTimeSeries();
  unattributed.provenance = SemanticProvenance();
  auto compiled = CompileInterface(unattributed, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  ASSERT_NE(primary, nullptr);
  EXPECT_EQ(primary->type, ComponentType::kSortableTable);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kChartWouldMislead));
}

TEST(InterfaceCompilerTest, RepresentationChangesWithoutRefetch) {
  const SemanticResult result = TelemetryTimeSeries();
  auto first = CompileInterface(result, InterfaceIntent());
  ASSERT_TRUE(first.has_value());

  InterfaceIntent as_table;
  as_table.requested_representation = ComponentType::kSortableTable;
  auto second = CompileInterface(result, as_table, first->surface.id);
  ASSERT_TRUE(second.has_value());
  // Same surface identity (in-place update), same underlying data entries,
  // different representation: no refetch, no re-reasoning.
  EXPECT_TRUE(second->surface.id == first->surface.id);
  ASSERT_NE(Primary(second.value()), nullptr);
  EXPECT_EQ(Primary(second.value())->type, ComponentType::kSortableTable);
  EXPECT_EQ(first->surface.data.at("rows"), second->surface.data.at("rows"));
}

TEST(InterfaceCompilerTest, OhlcRolesSelectCandlestick) {
  SemanticResult buckets;
  buckets.schema.shape = SemanticShape::kTimeSeries;
  buckets.schema.fields = {
      Field("bucket", FieldPrimitive::kTimestamp, SemanticRole::kTimestamp,
            false),
      Field("open_v", FieldPrimitive::kNumber, SemanticRole::kOpen, false),
      Field("high_v", FieldPrimitive::kNumber, SemanticRole::kHigh, false),
      Field("low_v", FieldPrimitive::kNumber, SemanticRole::kLow, false),
      Field("close_v", FieldPrimitive::kNumber, SemanticRole::kClose, false)};
  buckets.data = base::test::ParseJson(R"json([
    {"bucket": 1000.0, "open_v": 4.0, "high_v": 6.0, "low_v": 3.0,
     "close_v": 5.0},
    {"bucket": 2000.0, "open_v": 5.0, "high_v": 8.0, "low_v": 4.0,
     "close_v": 7.0}
  ])json");
  SetFixtureProvenance(&buckets);
  auto compiled = CompileInterface(buckets, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  ASSERT_NE(Primary(compiled.value()), nullptr);
  EXPECT_EQ(Primary(compiled.value())->type, ComponentType::kCandlestickChart);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kOhlcCandlestick));
}

TEST(InterfaceCompilerTest, HierarchyAndGraphShapes) {
  SemanticResult org_chart;
  org_chart.schema.shape = SemanticShape::kHierarchy;
  org_chart.schema.fields = {
      Field("node_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("reports_to", FieldPrimitive::kString,
            SemanticRole::kParentReference),
      Field("title_text", FieldPrimitive::kString, SemanticRole::kName)};
  org_chart.data = base::test::ParseJson(R"json([
    {"node_id": "n1", "title_text": "Root"},
    {"node_id": "n2", "reports_to": "n1", "title_text": "Child"}
  ])json");
  SetFixtureProvenance(&org_chart);
  auto compiled = CompileInterface(org_chart, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  EXPECT_EQ(Primary(compiled.value())->type, ComponentType::kTree);

  SemanticResult dependencies;
  dependencies.schema.shape = SemanticShape::kGraph;
  dependencies.schema.fields = {Field("module", FieldPrimitive::kString,
                                      SemanticRole::kIdentifier, false)};
  dependencies.schema.edge_fields = {
      Field("from_module", FieldPrimitive::kString, SemanticRole::kSourceNode,
            false),
      Field("to_module", FieldPrimitive::kString, SemanticRole::kTargetNode,
            false)};
  dependencies.data = base::test::ParseJson(R"json({
    "nodes": [{"module": "core"}, {"module": "ui"}],
    "edges": [{"from_module": "ui", "to_module": "core"}]
  })json");
  SetFixtureProvenance(&dependencies);
  compiled = CompileInterface(dependencies, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  const ComponentNode* primary = Primary(compiled.value());
  EXPECT_EQ(primary->type, ComponentType::kNetworkGraph);
  EXPECT_EQ(primary->bindings.at("edges"), "edges");
}

TEST(InterfaceCompilerTest, MissingInputsBecomeASchemaForm) {
  SemanticResult missing;
  missing.schema.shape = SemanticShape::kFormSchema;
  missing.schema.fields = {
      Field("param_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("prompt_text", FieldPrimitive::kString, SemanticRole::kName, false),
      Field("required", FieldPrimitive::kBoolean, SemanticRole::kNone)};
  missing.data = base::test::ParseJson(R"json([
    {"param_id": "destination", "prompt_text": "Where to?",
     "required": true},
    {"param_id": "date_range", "prompt_text": "Which dates?",
     "required": true}
  ])json");
  auto compiled = CompileInterface(missing, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  EXPECT_EQ(Primary(compiled.value())->type, ComponentType::kSchemaForm);
  EXPECT_TRUE(
      HasReason(compiled.value(), CompilerReason::kFormFromMissingInputs));
}

TEST(InterfaceCompilerTest, UnknownRolelessRecordFallsBackGenerically) {
  SemanticResult odd;
  odd.schema.shape = SemanticShape::kRecord;
  odd.schema.fields = {
      Field("alpha", FieldPrimitive::kString, SemanticRole::kNone),
      Field("beta", FieldPrimitive::kNumber, SemanticRole::kNone)};
  odd.data = base::test::ParseJson(R"json({"alpha": "x", "beta": 2.5})json");
  auto compiled = CompileInterface(odd, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  EXPECT_EQ(Primary(compiled.value())->type, ComponentType::kKeyValueCard);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kRecordKeyValue));
}

TEST(InterfaceCompilerTest, CompositeStacksItsParts) {
  SemanticResult composite;
  composite.schema.shape = SemanticShape::kComposite;
  composite.schema.part_names = {"headline", "trend"};
  SemanticSchema headline;
  headline.shape = SemanticShape::kScalar;
  headline.fields = {
      Field("current", FieldPrimitive::kNumber, SemanticRole::kMeasure)};
  composite.schema.parts.push_back(headline);
  composite.schema.parts.push_back(TelemetryTimeSeries().schema);
  base::DictValue data;
  data.Set("headline", 7.0);
  data.Set("trend", TelemetryTimeSeries().data.Clone());
  composite.data = base::Value(std::move(data));
  SetFixtureProvenance(&composite);

  auto compiled = CompileInterface(composite, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kCompositeSections));
  // Both parts materialized under the root.
  EXPECT_EQ(compiled->surface.components[0].children.size(), 2u);
  EXPECT_TRUE(ValidateSurface(compiled->surface).has_value());
}

TEST(InterfaceCompilerTest, CitationsBecomeASourceList) {
  SemanticResult citations;
  citations.schema.shape = SemanticShape::kCitations;
  citations.schema.fields = {
      Field("source_url", FieldPrimitive::kString, SemanticRole::kUrl, false),
      Field("source_title", FieldPrimitive::kString, SemanticRole::kName)};
  citations.data = base::test::ParseJson(R"json([
    {"source_url": "https://a.test/one", "source_title": "One"},
    {"source_url": "https://b.test/two", "source_title": "Two"}
  ])json");
  auto compiled = CompileInterface(citations, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  EXPECT_EQ(Primary(compiled.value())->type, ComponentType::kSourceList);
}

TEST(InterfaceCompilerTest, FieldDirectivesFilterTheData) {
  InterfaceIntent intent;
  intent.hidden_field_ids = {"points"};
  intent.compare_entity_ids = {"n", "s"};
  auto compiled = CompileInterface(LeagueStandings(), intent);
  ASSERT_TRUE(compiled.has_value());
  const DataEntry& rows = compiled->surface.data.at("rows");
  // Hidden column dropped; rows filtered to the compared entities.
  EXPECT_EQ(rows.table.columns.size(), 2u);
  EXPECT_EQ(rows.table.rows.size(), 2u);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kFieldsHidden));
  EXPECT_TRUE(
      HasReason(compiled.value(), CompilerReason::kFilteredToComparedEntities));
}

TEST(InterfaceCompilerTest, EmptyCollectionRendersAnEmptyState) {
  SemanticResult empty = LeagueStandings();
  empty.data = base::Value(base::ListValue());
  auto compiled = CompileInterface(empty, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  ASSERT_NE(Primary(compiled.value()), nullptr);
  EXPECT_EQ(Primary(compiled.value())->type, ComponentType::kEmptyState);
  EXPECT_TRUE(HasReason(compiled.value(), CompilerReason::kEmptyResult));
}

TEST(InterfaceCompilerTest, PinnedSurfacesBecomeTitledDashboards) {
  InterfaceIntent intent;
  intent.pin = true;
  intent.title = "Queue depth";
  auto compiled = CompileInterface(TelemetryTimeSeries(), intent);
  ASSERT_TRUE(compiled.has_value());
  EXPECT_EQ(compiled->surface.kind, SurfaceKind::kDashboard);
  EXPECT_TRUE(compiled->surface.pinned);
  EXPECT_EQ(compiled->surface.title, "Queue depth");
}

TEST(InterfaceCompilerTest, InvalidSemanticResultIsRejectedUpFront) {
  SemanticResult broken = LeagueStandings();
  broken.data.GetIfList()->front().GetDict().Set("uninvited", 3);
  auto compiled = CompileInterface(broken, InterfaceIntent());
  EXPECT_FALSE(compiled.has_value());
}

TEST(InterfaceCompilerTest, SemanticIdsMapCollisionFreeIntoSauiKeys) {
  const std::string long_id(50, 'a');
  const auto keys = BuildSauiKeyMap({"field_0", "onload", long_id});
  ASSERT_EQ(keys.size(), 3u);
  EXPECT_EQ(keys.at("field_0"), "field_0");
  std::set<std::string> unique;
  for (const auto& [semantic_id, wire_key] : keys) {
    EXPECT_TRUE(IsValidPropKey(wire_key));
    unique.insert(wire_key);
  }
  EXPECT_EQ(unique.size(), keys.size());

  SemanticResult result;
  result.schema.shape = SemanticShape::kTable;
  result.schema.fields = {
      Field(long_id, FieldPrimitive::kString, SemanticRole::kDimension, false),
      Field("n", FieldPrimitive::kNumber, SemanticRole::kMeasure, false)};
  base::ListValue rows;
  for (int i = 0; i < 2; ++i) {
    base::DictValue row;
    row.Set(long_id, i == 0 ? "a" : "b");
    row.Set("n", i + 1.0);
    rows.Append(std::move(row));
  }
  result.data = base::Value(std::move(rows));
  SetFixtureProvenance(&result);

  auto compiled = CompileInterface(result, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  ASSERT_TRUE(ValidateSurface(compiled->surface).has_value());
  const DataEntry& table = compiled->surface.data.at("rows");
  ASSERT_EQ(table.table.columns.size(), 2u);
  const auto compiled_keys = BuildSauiKeyMap({long_id, "n"});
  EXPECT_EQ(table.table.columns[0].key, compiled_keys.at(long_id));
}

TEST(InterfaceCompilerTest, LongCompositePartNamesUseBoundedOrdinalPaths) {
  SemanticResult composite;
  composite.schema.shape = SemanticShape::kComposite;
  SemanticSchema scalar;
  scalar.shape = SemanticShape::kScalar;
  scalar.fields = {
      Field("value", FieldPrimitive::kNumber, SemanticRole::kMeasure)};
  const std::string first_name(64, 'a');
  const std::string second_name(64, 'b');
  composite.schema.part_names = {first_name, second_name};
  composite.schema.parts = {scalar, scalar};
  base::DictValue data;
  data.Set(first_name, 1.0);
  data.Set(second_name, 2.0);
  composite.data = base::Value(std::move(data));
  SetFixtureProvenance(&composite);

  auto compiled = CompileInterface(composite, InterfaceIntent());
  ASSERT_TRUE(compiled.has_value());
  EXPECT_TRUE(compiled->surface.data.contains("p0_value"));
  EXPECT_TRUE(compiled->surface.data.contains("p1_value"));
  EXPECT_TRUE(ValidateSurface(compiled->surface).has_value());
}

}  // namespace
}  // namespace seoul
