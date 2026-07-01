// Project Seoul Adaptive UI (SAUI).
// Unit tests for typed incremental surface patches: parsing, atomic apply,
// revision bumping, change summaries.

#include "seoul/browser/saui/saui_patch.h"

#include "base/test/values_test_util.h"
#include "seoul/browser/saui/saui_document.h"
#include "seoul/browser/saui/saui_limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

AdaptiveSurface LiveChartSurface() {
  auto surface = ParseSurface(base::test::ParseJson(R"json({
    "schema_version": 1,
    "kind": "dashboard",
    "title": "Prices",
    "components": [
      {"id": "root", "type": "stack", "children": [
        {"id": "chart", "type": "line_chart",
         "props": {"title": "Price", "x_label": "Time", "y_label": "USD",
                    "units": "USD"},
         "bindings": {"data": "prices"},
         "accessible_name": "Price over time", "update_policy": "live"}
      ]}
    ],
    "data": {
      "prices": {"kind": "series", "y_unit": "USD",
                 "points": [{"t_ms": 1000.0, "y": 1.0},
                            {"t_ms": 2000.0, "y": 2.0}],
                 "provenance": {"source_name": "fixture",
                                 "retrieved_at_ms": 5000.0,
                                 "effective_at_ms": 4000.0}}
    }
  })json"));
  CHECK(surface.has_value());
  return std::move(surface.value());
}

SurfacePatch PatchFor(const AdaptiveSurface& surface, SurfacePatchOp op) {
  SurfacePatch patch;
  patch.surface_id = surface.id;
  patch.ops.push_back(std::move(op));
  return patch;
}

TEST(SauiPatchTest, AppendSeriesPointsUpdatesInPlace) {
  AdaptiveSurface surface = LiveChartSurface();
  const uint64_t initial_revision = surface.revision;

  SurfacePatchOp op;
  op.kind = PatchOpKind::kAppendSeriesPoints;
  op.entry_name = "prices";
  SeriesPoint point;
  point.has_time = true;
  point.time = base::Time::UnixEpoch() + base::Seconds(3);
  point.y = 3.0;
  op.points.push_back(point);

  auto applied = ApplySurfacePatch(surface, PatchFor(surface, op));
  ASSERT_TRUE(applied.has_value());
  EXPECT_EQ(surface.data["prices"].series.points.size(), 3u);
  EXPECT_EQ(surface.revision, initial_revision + 1);
  ASSERT_EQ(applied->changed_entry_names.size(), 1u);
  EXPECT_EQ(applied->changed_entry_names[0], "prices");
  EXPECT_TRUE(applied->changed_component_ids.empty());
}

TEST(SauiPatchTest, SetPropsMergesAndReportsComponent) {
  AdaptiveSurface surface = LiveChartSurface();
  SurfacePatchOp op;
  op.kind = PatchOpKind::kSetProps;
  op.target_component = "chart";
  op.props.Set("title", "Updated Price");

  auto applied = ApplySurfacePatch(surface, PatchFor(surface, op));
  ASSERT_TRUE(applied.has_value());
  ASSERT_EQ(applied->changed_component_ids.size(), 1u);
  EXPECT_EQ(applied->changed_component_ids[0], "chart");
  const ComponentNode& chart = surface.components[0].children[0];
  EXPECT_EQ(*chart.props.FindString("title"), "Updated Price");
  // Untouched props survive the merge.
  EXPECT_EQ(*chart.props.FindString("units"), "USD");
}

TEST(SauiPatchTest, FailedOpLeavesSurfaceUntouched) {
  AdaptiveSurface surface = LiveChartSurface();
  const AdaptiveSurface before = surface;

  SurfacePatch patch;
  patch.surface_id = surface.id;
  SurfacePatchOp good;
  good.kind = PatchOpKind::kSetTitle;
  good.title = "New Title";
  patch.ops.push_back(good);
  SurfacePatchOp bad;
  bad.kind = PatchOpKind::kSetProps;
  bad.target_component = "does_not_exist";
  bad.props.Set("title", "x");
  patch.ops.push_back(bad);

  auto applied = ApplySurfacePatch(surface, patch);
  EXPECT_EQ(applied.error(), SauiError::kPatchTargetMissing);
  EXPECT_EQ(surface, before);  // atomic: nothing from the good op remains
}

TEST(SauiPatchTest, ResultingInvalidSurfaceIsRejected) {
  AdaptiveSurface surface = LiveChartSurface();
  const AdaptiveSurface before = surface;

  // Removing the series entry would orphan the chart binding; apply must
  // reject and roll back.
  SurfacePatchOp op;
  op.kind = PatchOpKind::kUpsertDataEntry;
  op.entry_name = "prices";
  op.entry.kind = DataEntryKind::kScalar;
  op.entry.scalar = base::Value(1.0);

  auto applied = ApplySurfacePatch(surface, PatchFor(surface, op));
  EXPECT_EQ(applied.error(), SauiError::kBindingKindMismatch);
  EXPECT_EQ(surface, before);
}

TEST(SauiPatchTest, AppendChildRespectsContainerRule) {
  AdaptiveSurface surface = LiveChartSurface();

  SurfacePatchOp op;
  op.kind = PatchOpKind::kAppendChild;
  op.target_component = "chart";  // charts are not containers
  op.component.id = "extra";
  op.component.type = ComponentType::kText;
  op.component.props.Set("text", "no");

  auto applied = ApplySurfacePatch(surface, PatchFor(surface, op));
  EXPECT_EQ(applied.error(), SauiError::kChildrenNotAllowed);

  op.target_component = "root";
  applied = ApplySurfacePatch(surface, PatchFor(surface, op));
  ASSERT_TRUE(applied.has_value());
  EXPECT_EQ(surface.components[0].children.size(), 2u);
}

TEST(SauiPatchTest, RemoveComponentAndStateChange) {
  AdaptiveSurface surface = LiveChartSurface();

  SurfacePatchOp state_op;
  state_op.kind = PatchOpKind::kSetState;
  state_op.target_component = "chart";
  state_op.state = ComponentState::kLoading;
  state_op.state_message = "Refreshing";
  ASSERT_TRUE(
      ApplySurfacePatch(surface, PatchFor(surface, state_op)).has_value());
  EXPECT_EQ(surface.components[0].children[0].state, ComponentState::kLoading);

  SurfacePatchOp remove_op;
  remove_op.kind = PatchOpKind::kRemoveComponent;
  remove_op.target_component = "chart";
  ASSERT_TRUE(
      ApplySurfacePatch(surface, PatchFor(surface, remove_op)).has_value());
  EXPECT_TRUE(surface.components[0].children.empty());
}

TEST(SauiPatchTest, WrongSurfaceIdIsRejected) {
  AdaptiveSurface surface = LiveChartSurface();
  SurfacePatchOp op;
  op.kind = PatchOpKind::kSetTitle;
  op.title = "x";
  SurfacePatch patch = PatchFor(surface, op);
  patch.surface_id = SurfaceId::GenerateNew();
  EXPECT_EQ(ApplySurfacePatch(surface, patch).error(),
            SauiError::kInvalidPatch);
}

TEST(SauiPatchTest, ParsesUntrustedPatchDocument) {
  AdaptiveSurface surface = LiveChartSurface();
  base::Value::Dict doc;
  doc.Set("surface_id", surface.id.value());
  base::Value::List ops;
  ops.Append(base::test::ParseJson(R"json({
    "op": "append_series_points", "entry": "prices",
    "points": [{"t_ms": 3000.0, "y": 3.0}]
  })json"));
  ops.Append(base::test::ParseJson(R"json({
    "op": "set_state", "target": "chart", "state": "ready"
  })json"));
  doc.Set("ops", std::move(ops));

  auto patch = ParseSurfacePatch(base::Value(std::move(doc)));
  ASSERT_TRUE(patch.has_value());
  ASSERT_EQ(patch->ops.size(), 2u);
  auto applied = ApplySurfacePatch(surface, patch.value());
  ASSERT_TRUE(applied.has_value());
  EXPECT_EQ(surface.data["prices"].series.points.size(), 3u);
}

TEST(SauiPatchTest, ParseRejectsUnknownOpAndBadIds) {
  base::Value doc = base::test::ParseJson(R"json({
    "surface_id": "not-a-uuid",
    "ops": [{"op": "set_title", "title": "x"}]
  })json");
  EXPECT_EQ(ParseSurfacePatch(doc).error(), SauiError::kInvalidPatch);

  doc = base::test::ParseJson(R"json({
    "surface_id": "8b6b02f2-6a0f-4d35-9c62-9b6f4de1a001",
    "ops": [{"op": "explode_surface"}]
  })json");
  EXPECT_EQ(ParseSurfacePatch(doc).error(), SauiError::kInvalidPatch);
}

}  // namespace
}  // namespace seoul
