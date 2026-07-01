// Project Seoul Adaptive UI (SAUI).
// Unit tests for presentation selection: text vs component vs composed
// surface vs form, and persistence.

#include "seoul/browser/saui/saui_selection.h"

#include <algorithm>

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

bool HasReason(const PresentationDecision& decision, SelectionReason reason) {
  return std::find(decision.reasons.begin(), decision.reasons.end(), reason) !=
         decision.reasons.end();
}

TEST(SauiSelectionTest, SingleScalarNeverBecomesAChart) {
  PresentationSignals signals;
  signals.only_single_scalar = true;
  signals.user_requested_visual = true;  // even when asked for a visual
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_EQ(decision.form, PresentationForm::kTextOnly);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kSingleScalarNoChart));
}

TEST(SauiSelectionTest, NoEligibleDataMeansText) {
  PresentationSignals signals;
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_EQ(decision.form, PresentationForm::kTextOnly);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kNoEligibleData));
}

TEST(SauiSelectionTest, MissingInputsPreferAFormOverQuestions) {
  PresentationSignals signals;
  signals.missing_required_inputs = 3;
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_EQ(decision.form, PresentationForm::kEditableForm);
  EXPECT_TRUE(
      HasReason(decision, SelectionReason::kMissingInputsFormBeatsQuestions));
}

TEST(SauiSelectionTest, TextOnlyRequestWins) {
  PresentationSignals signals;
  signals.user_requested_text_only = true;
  signals.chart_eligible_entries = 2;
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_EQ(decision.form, PresentationForm::kTextOnly);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kUserRequestedTextOnly));
}

TEST(SauiSelectionTest, OneEligibleSeriesIsASingleComponent) {
  PresentationSignals signals;
  signals.chart_eligible_entries = 1;
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_EQ(decision.form, PresentationForm::kSingleComponent);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kTimeSeriesEligible));
}

TEST(SauiSelectionTest, ComparisonComposesASurface) {
  PresentationSignals signals;
  signals.chart_eligible_entries = 2;
  signals.record_entries = 2;
  signals.comparison_across_entities = true;
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_EQ(decision.form, PresentationForm::kComposedSurface);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kComparisonAcrossEntities));
}

TEST(SauiSelectionTest, MonitoringAndTaskResultsPersist) {
  PresentationSignals signals;
  signals.chart_eligible_entries = 1;
  signals.monitoring = true;
  PresentationDecision decision = SelectPresentation(signals);
  EXPECT_TRUE(decision.persist);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kMonitoringPersists));

  signals.monitoring = false;
  signals.task_produces_persistent_result = true;
  decision = SelectPresentation(signals);
  EXPECT_TRUE(decision.persist);
  EXPECT_TRUE(HasReason(decision, SelectionReason::kTaskResultPersists));
}

}  // namespace
}  // namespace seoul
