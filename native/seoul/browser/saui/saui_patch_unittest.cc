// Project Seoul Adaptive UI (SAUI).
// Unit tests for typed incremental surface patches: parsing, atomic apply,
// revision bumping, change summaries.

#include "seoul/browser/saui/saui_patch.h"

#include "base/strings/string_number_conversions.h"
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

TEST(SauiPatchTest, SetBindingsRebindsToExistingEntry) {
  AdaptiveSurface surface = LiveChartSurface();

  // Add a second series entry, then rebind the chart to it.
  SurfacePatchOp add_entry;
  add_entry.kind = PatchOpKind::kUpsertDataEntry;
  add_entry.entry_name = "prices_backfill";
  add_entry.entry.kind = DataEntryKind::kSeries;
  SeriesPoint point;
  point.has_time = true;
  point.time = base::Time::UnixEpoch() + base::Seconds(1);
  point.y = 9.0;
  add_entry.entry.series.points.push_back(point);
  add_entry.entry.has_provenance = true;
  add_entry.entry.provenance.source_name = "fixture";
  add_entry.entry.provenance.retrieved_at =
      base::Time::UnixEpoch() + base::Seconds(5);
  add_entry.entry.provenance.effective_at =
      base::Time::UnixEpoch() + base::Seconds(4);
  ASSERT_TRUE(
      ApplySurfacePatch(surface, PatchFor(surface, add_entry)).has_value());

  SurfacePatchOp rebind;
  rebind.kind = PatchOpKind::kSetBindings;
  rebind.target_component = "chart";
  rebind.bindings["data"] = "prices_backfill";
  auto applied = ApplySurfacePatch(surface, PatchFor(surface, rebind));
  ASSERT_TRUE(applied.has_value());
  ASSERT_EQ(applied->changed_component_ids.size(), 1u);
  EXPECT_EQ(applied->changed_component_ids[0], "chart");
  EXPECT_EQ(surface.components[0].children[0].bindings.at("data"),
            "prices_backfill");
}

TEST(SauiPatchTest, SetBindingsToMissingEntryFailsAtomically) {
  AdaptiveSurface surface = LiveChartSurface();
  const AdaptiveSurface before = surface;

  SurfacePatchOp rebind;
  rebind.kind = PatchOpKind::kSetBindings;
  rebind.target_component = "chart";
  rebind.bindings["data"] = "does_not_exist";
  auto applied = ApplySurfacePatch(surface, PatchFor(surface, rebind));
  EXPECT_FALSE(applied.has_value());
  EXPECT_EQ(surface, before);  // atomic: the dangling rebind never lands
}

TEST(SauiPatchTest, ParseSetBindingsValidatesSlotAndEntryNames) {
  base::Value doc = base::test::ParseJson(R"json({
    "surface_id": "8b6b02f2-6a0f-4d35-9c62-9b6f4de1a001",
    "ops": [{"op": "set_bindings", "target": "chart",
             "bindings": {"data": "prices_backfill"}}]
  })json");
  auto patch = ParseSurfacePatch(doc);
  ASSERT_TRUE(patch.has_value());
  ASSERT_EQ(patch->ops.size(), 1u);
  EXPECT_EQ(patch->ops[0].kind, PatchOpKind::kSetBindings);
  EXPECT_EQ(patch->ops[0].bindings.at("data"), "prices_backfill");

  doc = base::test::ParseJson(R"json({
    "surface_id": "8b6b02f2-6a0f-4d35-9c62-9b6f4de1a001",
    "ops": [{"op": "set_bindings", "target": "chart",
             "bindings": {"onload": "prices"}}]
  })json");
  EXPECT_EQ(ParseSurfacePatch(doc).error(), SauiError::kInvalidDataEntry);

  doc = base::test::ParseJson(R"json({
    "surface_id": "8b6b02f2-6a0f-4d35-9c62-9b6f4de1a001",
    "ops": [{"op": "set_bindings", "target": "chart", "bindings": {}}]
  })json");
  EXPECT_EQ(ParseSurfacePatch(doc).error(), SauiError::kInvalidPatch);
}

TEST(SauiPatchTest, ReplaceUnderNewIdReportsOldIdToo) {
  AdaptiveSurface surface = LiveChartSurface();
  SurfacePatchOp op;
  op.kind = PatchOpKind::kReplaceComponent;
  op.target_component = "chart";
  op.component.id = "chart-v2";
  op.component.type = ComponentType::kText;
  op.component.props.Set("text", "replaced");
  auto applied = ApplySurfacePatch(surface, PatchFor(surface, op));
  ASSERT_TRUE(applied.has_value());
  // The renderer needs both: remove the stale "chart" element and render
  // "chart-v2".
  ASSERT_EQ(applied->changed_component_ids.size(), 2u);
  EXPECT_EQ(applied->changed_component_ids[0], "chart");
  EXPECT_EQ(applied->changed_component_ids[1], "chart-v2");
}

TEST(SauiPatchTest, AppendChildDepthIsBounded) {
  AdaptiveSurface surface = LiveChartSurface();
  // Build a chain of nested stacks up to the depth limit, then one more
  // append must fail while the surface stays untouched.
  std::string parent = "root";
  for (size_t depth = 2; depth <= kMaxComponentDepth; ++depth) {
    SurfacePatchOp op;
    op.kind = PatchOpKind::kAppendChild;
    op.target_component = parent;
    op.component.id = "s" + base::NumberToString(depth);
    op.component.type = ComponentType::kStack;
    ASSERT_TRUE(ApplySurfacePatch(surface, PatchFor(surface, op)).has_value())
        << "depth " << depth;
    parent = op.component.id;
  }
  const AdaptiveSurface before = surface;
  SurfacePatchOp too_deep;
  too_deep.kind = PatchOpKind::kAppendChild;
  too_deep.target_component = parent;
  too_deep.component.id = "beyond";
  too_deep.component.type = ComponentType::kText;
  too_deep.component.props.Set("text", "x");
  auto applied = ApplySurfacePatch(surface, PatchFor(surface, too_deep));
  ASSERT_FALSE(applied.has_value());
  EXPECT_EQ(applied.error(), SauiError::kLimitExceeded);
  EXPECT_EQ(surface, before);
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
  base::DictValue doc;
  doc.Set("surface_id", surface.id.value());
  base::ListValue ops;
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
