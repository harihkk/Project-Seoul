// Project Seoul Adaptive UI (SAUI).
// Unit tests for semantic surface validation: identity, bindings, chart
// honesty, action references, accessibility.

#include "seoul/browser/saui/saui_validator.h"

#include "base/test/values_test_util.h"
#include "seoul/browser/saui/saui_document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

AdaptiveSurface ParseOk(std::string_view json) {
  auto surface = ParseSurface(base::test::ParseJson(json));
  EXPECT_TRUE(surface.has_value());
  return std::move(surface.value());
}

constexpr std::string_view kChartSurfaceJson = R"json({
  "schema_version": 1,
  "kind": "dashboard",
  "title": "Prices",
  "components": [
    {"id": "chart", "type": "line_chart",
     "props": {"title": "Price", "x_label": "Time", "y_label": "USD",
                "units": "USD"},
     "bindings": {"data": "prices"},
     "accessible_name": "Price over time", "update_policy": "live"}
  ],
  "data": {
    "prices": {"kind": "series", "y_unit": "USD",
               "points": [{"t_ms": 1000.0, "y": 1.0},
                          {"t_ms": 2000.0, "y": 2.0}],
               "provenance": {"source_name": "fixture",
                               "retrieved_at_ms": 5000.0,
                               "effective_at_ms": 4000.0}}
  }
})json";

TEST(SauiValidatorTest, AcceptsWellFormedChartSurface) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  EXPECT_TRUE(ValidateSurface(surface).has_value());
}

TEST(SauiValidatorTest, RejectsDuplicateComponentIds) {
  AdaptiveSurface surface = ParseOk(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "same", "type": "text", "props": {"text": "a"}},
      {"id": "same", "type": "text", "props": {"text": "b"}}
    ]
  })json");
  EXPECT_EQ(ValidateSurface(surface).error(), SauiError::kDuplicateComponentId);
}

TEST(SauiValidatorTest, RejectsUnresolvedBinding) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.components[0].bindings["data"] = "missing_entry";
  EXPECT_EQ(ValidateSurface(surface).error(), SauiError::kUnknownDataEntry);
}

TEST(SauiValidatorTest, RejectsBindingKindMismatch) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  // A sparkline accepts only series; rebind it to a record.
  DataEntry record;
  record.kind = DataEntryKind::kRecord;
  surface.data["record_entry"] = record;
  surface.components[0].bindings["data"] = "record_entry";
  EXPECT_EQ(ValidateSurface(surface).error(), SauiError::kBindingKindMismatch);
}

TEST(SauiValidatorTest, RejectsMissingRequiredBinding) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.components[0].bindings.clear();
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kMissingRequiredBinding);
}

TEST(SauiValidatorTest, ChartWithoutProvenanceIsRejected) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.data["prices"].has_provenance = false;
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kChartRequirementMissing);
}

TEST(SauiValidatorTest, ChartWithOnePointIsRejected) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.data["prices"].series.points.resize(1);
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kChartRequirementMissing);
}

TEST(SauiValidatorTest, ChartMissingUnitsPropIsRejected) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.components[0].props.Remove("units");
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kMissingRequiredProperty);
}

TEST(SauiValidatorTest, ChartMissingAccessibleNameIsRejected) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.components[0].accessible_name.clear();
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kMissingAccessibleName);
}

TEST(SauiValidatorTest, TruncatedBarChartMustSaySo) {
  AdaptiveSurface surface = ParseOk(R"json({
    "schema_version": 1,
    "kind": "dashboard",
    "title": "Bars",
    "components": [
      {"id": "bars", "type": "bar_chart",
       "props": {"title": "T", "x_label": "X", "y_label": "Y",
                  "units": "count", "baseline_zero": false},
       "bindings": {"data": "rows"},
       "accessible_name": "Bar chart"}
    ],
    "data": {
      "rows": {"kind": "table",
               "columns": [{"key": "k", "label": "K"},
                            {"key": "v", "label": "V"}],
               "rows": [["a", 10.0], ["b", 12.0]],
               "provenance": {"source_name": "fixture",
                               "retrieved_at_ms": 5000.0,
                               "effective_at_ms": 4000.0}}
    }
  })json");
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kTruncatedAxisNotIndicated);

  surface.components[0].props.Set("axis_truncation_indicated", true);
  EXPECT_TRUE(ValidateSurface(surface).has_value());
}

TEST(SauiValidatorTest, UnknownActionReferenceIsRejected) {
  AdaptiveSurface surface = ParseOk(R"json({
    "schema_version": 1,
    "kind": "response",
    "components": [
      {"id": "b", "type": "button", "props": {"label": "Go"},
       "accessible_name": "Go", "actions": ["missing_action"]}
    ]
  })json");
  EXPECT_EQ(ValidateSurface(surface).error(),
            SauiError::kUnknownActionReference);
}

TEST(SauiValidatorTest, NonResponseSurfaceRequiresTitle) {
  AdaptiveSurface surface = ParseOk(kChartSurfaceJson);
  surface.title.clear();
  EXPECT_EQ(ValidateSurface(surface).error(), SauiError::kInvalidTitle);
}

TEST(SauiValidatorTest, EntryChartEligibilityGate) {
  DataEntry entry;
  entry.kind = DataEntryKind::kSeries;
  entry.series.points.resize(2);
  EXPECT_FALSE(EntryChartEligible(entry));  // no provenance

  entry.has_provenance = true;
  entry.provenance.source_name = "fixture";
  entry.provenance.retrieved_at = base::Time::UnixEpoch() + base::Seconds(1);
  entry.provenance.effective_at = base::Time::UnixEpoch() + base::Seconds(1);
  EXPECT_TRUE(EntryChartEligible(entry));

  entry.series.points.resize(1);
  EXPECT_FALSE(EntryChartEligible(entry));  // one point is not a chart

  DataEntry scalar;
  scalar.kind = DataEntryKind::kScalar;
  scalar.has_provenance = true;
  scalar.provenance = entry.provenance;
  EXPECT_FALSE(EntryChartEligible(scalar));  // a scalar is never a chart
}

}  // namespace
}  // namespace seoul
