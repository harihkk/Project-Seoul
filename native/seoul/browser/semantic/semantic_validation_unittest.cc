// Project Seoul semantic data fabric.
// Unit tests for shape-and-role validation over arbitrary schemas. The
// fixtures deliberately use subject matter the production code has never
// heard of (server telemetry, league standings, housing listings): only
// shapes and roles matter, so they must validate through the same generic
// rules as anything else.

#include "seoul/browser/semantic/semantic_validation.h"

#include "base/test/values_test_util.h"
#include "seoul/browser/semantic/semantic_inspection.h"
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

// Held-out fixture: server telemetry as a time series.
SemanticResult TelemetryResult() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kTimeSeries;
  result.schema.fields = {
      Field("sampled_at", FieldPrimitive::kTimestamp,
            SemanticRole::kTimestamp, false),
      Field("cpu_load", FieldPrimitive::kNumber, SemanticRole::kMeasure,
            false),
      Field("region", FieldPrimitive::kString, SemanticRole::kDimension),
  };
  result.schema.fields[1].unit = "percent";
  result.data = base::test::ParseJson(R"json([
    {"sampled_at": 1000.0, "cpu_load": 40.5, "region": "east"},
    {"sampled_at": 2000.0, "cpu_load": 55.25, "region": "east"},
    {"sampled_at": 3000.0, "cpu_load": 47.0, "region": "east"}
  ])json");
  result.provenance.base.source_name = "fixture";
  result.provenance.base.retrieved_at =
      base::Time::UnixEpoch() + base::Days(20000);
  result.provenance.base.effective_at = result.provenance.base.retrieved_at;
  return result;
}

// Held-out fixture: league standings as an entity collection.
SemanticResult StandingsResult() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {
      Field("team_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("team_name", FieldPrimitive::kString, SemanticRole::kName,
            false),
      Field("wins", FieldPrimitive::kInteger, SemanticRole::kCount, false),
      Field("losses", FieldPrimitive::kInteger, SemanticRole::kCount, false),
  };
  result.data = base::test::ParseJson(R"json([
    {"team_id": "a", "team_name": "Alpha", "wins": 10, "losses": 2},
    {"team_id": "b", "team_name": "Beta", "wins": 8, "losses": 4},
    {"team_id": "c", "team_name": "Gamma", "wins": 5, "losses": 7}
  ])json");
  return result;
}

TEST(SemanticSchemaTest, AcceptsArbitraryWellFormedSchemas) {
  EXPECT_TRUE(ValidateSemanticSchema(TelemetryResult().schema).has_value());
  EXPECT_TRUE(ValidateSemanticSchema(StandingsResult().schema).has_value());
}

TEST(SemanticSchemaTest, EnforcesRoleCoherencePerShape) {
  SemanticSchema no_timestamp;
  no_timestamp.shape = SemanticShape::kTimeSeries;
  no_timestamp.fields = {
      Field("value", FieldPrimitive::kNumber, SemanticRole::kMeasure)};
  auto result = ValidateSemanticSchema(no_timestamp);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            SemanticFabricError::kMissingRoleForShape);

  SemanticSchema two_timestamps = no_timestamp;
  two_timestamps.fields.push_back(
      Field("t1", FieldPrimitive::kTimestamp, SemanticRole::kTimestamp));
  two_timestamps.fields.push_back(
      Field("t2", FieldPrimitive::kTimestamp, SemanticRole::kTimestamp));
  result = ValidateSemanticSchema(two_timestamps);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, SemanticFabricError::kConflictingRoles);

  SemanticSchema hierarchy;
  hierarchy.shape = SemanticShape::kHierarchy;
  hierarchy.fields = {
      Field("node_id", FieldPrimitive::kString, SemanticRole::kIdentifier)};
  result = ValidateSemanticSchema(hierarchy);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            SemanticFabricError::kMissingRoleForShape);

  SemanticSchema graph;
  graph.shape = SemanticShape::kGraph;
  graph.fields = {
      Field("node_id", FieldPrimitive::kString, SemanticRole::kIdentifier)};
  graph.edge_fields = {
      Field("from_node", FieldPrimitive::kString, SemanticRole::kSourceNode)};
  result = ValidateSemanticSchema(graph);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            SemanticFabricError::kMissingRoleForShape);

  SemanticSchema geo;
  geo.shape = SemanticShape::kGeoFeatures;
  geo.fields = {
      Field("lat", FieldPrimitive::kNumber, SemanticRole::kLatitude)};
  result = ValidateSemanticSchema(geo);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error,
            SemanticFabricError::kMissingRoleForShape);
}

TEST(SemanticSchemaTest, RejectsIdentityDefects) {
  SemanticSchema schema;
  schema.shape = SemanticShape::kRecord;
  schema.fields = {
      Field("Bad Id", FieldPrimitive::kString, SemanticRole::kNone)};
  auto result = ValidateSemanticSchema(schema);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, SemanticFabricError::kInvalidFieldId);

  schema.fields = {
      Field("dup", FieldPrimitive::kString, SemanticRole::kNone),
      Field("dup", FieldPrimitive::kString, SemanticRole::kNone)};
  result = ValidateSemanticSchema(schema);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().error, SemanticFabricError::kDuplicateFieldId);
}

TEST(SemanticResultTest, ValidatesUnseenSchemasEndToEnd) {
  EXPECT_TRUE(ValidateSemanticResult(TelemetryResult()).has_value());
  EXPECT_TRUE(ValidateSemanticResult(StandingsResult()).has_value());
}

TEST(SemanticResultTest, RejectsUndeclaredColumnsAndTypeMismatches) {
  SemanticResult result = StandingsResult();
  base::ListValue* rows = result.data.GetIfList();
  rows->front().GetDict().Set("invented_column", 1);
  auto validation = ValidateSemanticResult(result);
  ASSERT_FALSE(validation.has_value());
  EXPECT_EQ(validation.error().error, SemanticFabricError::kUnknownField);
  EXPECT_EQ(validation.error().detail, "invented_column");

  result = StandingsResult();
  result.data.GetIfList()->front().GetDict().Set("wins", "ten");
  validation = ValidateSemanticResult(result);
  ASSERT_FALSE(validation.has_value());
  EXPECT_EQ(validation.error().error,
            SemanticFabricError::kFieldTypeMismatch);
}

TEST(SemanticResultTest, UnavailableFieldsMustStayAbsent) {
  SemanticResult result = TelemetryResult();
  result.state = ResultState::kPartial;
  result.unavailable_field_ids = {"region"};
  // Region values are still present: that is fabrication and is rejected.
  auto validation = ValidateSemanticResult(result);
  ASSERT_FALSE(validation.has_value());
  EXPECT_EQ(validation.error().error,
            SemanticFabricError::kUnavailableFieldPresent);

  for (base::Value& row : *result.data.GetIfList()) {
    row.GetDict().Remove("region");
  }
  EXPECT_TRUE(ValidateSemanticResult(result).has_value());
}

TEST(SemanticResultTest, NonNullableFieldsAreRequired) {
  SemanticResult result = TelemetryResult();
  result.data.GetIfList()->front().GetDict().Remove("cpu_load");
  auto validation = ValidateSemanticResult(result);
  ASSERT_FALSE(validation.has_value());
  EXPECT_EQ(validation.error().error,
            SemanticFabricError::kMissingRequiredField);
}

TEST(SemanticResultTest, GraphAndCompositeShapesValidate) {
  SemanticResult graph;
  graph.schema.shape = SemanticShape::kGraph;
  graph.schema.fields = {
      Field("node_id", FieldPrimitive::kString, SemanticRole::kIdentifier,
            false),
      Field("node_label", FieldPrimitive::kString, SemanticRole::kName)};
  graph.schema.edge_fields = {
      Field("from_node", FieldPrimitive::kString, SemanticRole::kSourceNode,
            false),
      Field("to_node", FieldPrimitive::kString, SemanticRole::kTargetNode,
            false)};
  graph.data = base::test::ParseJson(R"json({
    "nodes": [{"node_id": "a", "node_label": "A"},
               {"node_id": "b", "node_label": "B"}],
    "edges": [{"from_node": "a", "to_node": "b"}]
  })json");
  EXPECT_TRUE(ValidateSemanticResult(graph).has_value());

  SemanticResult composite;
  composite.schema.shape = SemanticShape::kComposite;
  composite.schema.part_names = {"summary", "points"};
  SemanticSchema summary;
  summary.shape = SemanticShape::kScalar;
  summary.fields = {
      Field("total", FieldPrimitive::kNumber, SemanticRole::kMeasure)};
  composite.schema.parts.push_back(summary);
  composite.schema.parts.push_back(TelemetryResult().schema);
  base::DictValue data;
  data.Set("summary", 47.6);
  data.Set("points", TelemetryResult().data.Clone());
  composite.data = base::Value(std::move(data));
  EXPECT_TRUE(ValidateSemanticResult(composite).has_value());

  // A missing part is a mismatch, not a silent gap.
  composite.data.GetIfDict()->Remove("points");
  auto validation = ValidateSemanticResult(composite);
  ASSERT_FALSE(validation.has_value());
  EXPECT_EQ(validation.error().error,
            SemanticFabricError::kCompositePartMismatch);
}

TEST(SemanticResultTest, StreamingMergeIsAtomicAndBounded) {
  SemanticResult result = TelemetryResult();
  result.state = ResultState::kStreaming;

  base::Value good_rows = base::test::ParseJson(R"json([
    {"sampled_at": 4000.0, "cpu_load": 60.0, "region": "east"}
  ])json");
  ASSERT_TRUE(MergeStreamingRows(result, good_rows.GetList()).has_value());
  EXPECT_EQ(RowCount(result), 4u);

  base::Value bad_rows = base::test::ParseJson(R"json([
    {"sampled_at": 5000.0, "cpu_load": "not-a-number", "region": "east"}
  ])json");
  auto merge = MergeStreamingRows(result, bad_rows.GetList());
  ASSERT_FALSE(merge.has_value());
  EXPECT_EQ(merge.error().error, SemanticFabricError::kFieldTypeMismatch);
  EXPECT_EQ(RowCount(result), 4u);  // atomic: nothing appended

  result.state = ResultState::kComplete;
  merge = MergeStreamingRows(result, good_rows.GetList());
  ASSERT_FALSE(merge.has_value());
  EXPECT_EQ(merge.error().error, SemanticFabricError::kNotStreaming);
}

TEST(SemanticInspectionTest, RoleQueriesDriveGenericDecisions) {
  const SemanticResult telemetry = TelemetryResult();
  EXPECT_TRUE(HasTemporalAxis(telemetry.schema));
  EXPECT_FALSE(OhlcEligible(telemetry.schema));
  EXPECT_FALSE(GeoEligible(telemetry.schema));
  EXPECT_EQ(MeasureFields(telemetry.schema).size(), 1u);
  EXPECT_FALSE(ChartWouldMislead(telemetry));

  const SemanticResult standings = StandingsResult();
  EXPECT_TRUE(ComparableEntities(standings));
  EXPECT_FALSE(HasTemporalAxis(standings.schema));

  // One point is never a chart.
  SemanticResult single = TelemetryResult();
  base::ListValue* rows = single.data.GetIfList();
  while (rows->size() > 1) {
    rows->erase(rows->begin());
  }
  EXPECT_TRUE(ChartWouldMislead(single));

  // OHLC roles are generic interval summaries.
  SemanticSchema ohlc;
  ohlc.shape = SemanticShape::kTimeSeries;
  ohlc.fields = {
      Field("bucket", FieldPrimitive::kTimestamp, SemanticRole::kTimestamp),
      Field("open_value", FieldPrimitive::kNumber, SemanticRole::kOpen),
      Field("high_value", FieldPrimitive::kNumber, SemanticRole::kHigh),
      Field("low_value", FieldPrimitive::kNumber, SemanticRole::kLow),
      Field("close_value", FieldPrimitive::kNumber, SemanticRole::kClose)};
  EXPECT_TRUE(OhlcEligible(ohlc));
}

}  // namespace
}  // namespace seoul
