// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_selection.h"

namespace seoul {

PresentationDecision SelectPresentation(const PresentationSignals& signals) {
  PresentationDecision decision;
  decision.persist =
      signals.monitoring || signals.task_produces_persistent_result;
  if (signals.monitoring) {
    decision.reasons.push_back(SelectionReason::kMonitoringPersists);
  }
  if (signals.task_produces_persistent_result) {
    decision.reasons.push_back(SelectionReason::kTaskResultPersists);
  }

  // An explicit text-only request wins over everything except a form that is
  // required to proceed.
  if (signals.user_requested_text_only && signals.missing_required_inputs < 2) {
    decision.form = PresentationForm::kTextOnly;
    decision.reasons.push_back(SelectionReason::kUserRequestedTextOnly);
    return decision;
  }

  // Two or more missing required inputs: one editable form beats repeated
  // clarifying questions.
  if (signals.missing_required_inputs >= 2) {
    decision.form = PresentationForm::kEditableForm;
    decision.reasons.push_back(
        SelectionReason::kMissingInputsFormBeatsQuestions);
    return decision;
  }

  const int visual_entries = signals.chart_eligible_entries +
                             signals.record_entries +
                             (signals.has_geo_coordinates ? 1 : 0);

  // A single unverified/lone scalar never becomes a chart. Text states the
  // value with its source.
  if (signals.only_single_scalar) {
    decision.form = PresentationForm::kTextOnly;
    decision.reasons.push_back(SelectionReason::kSingleScalarNoChart);
    return decision;
  }

  if (visual_entries == 0) {
    decision.form = PresentationForm::kTextOnly;
    decision.reasons.push_back(SelectionReason::kNoEligibleData);
    return decision;
  }

  if (signals.user_requested_visual) {
    decision.reasons.push_back(SelectionReason::kUserRequestedVisual);
  }
  if (signals.chart_eligible_entries > 0) {
    decision.reasons.push_back(SelectionReason::kTimeSeriesEligible);
  }
  if (signals.has_geo_coordinates) {
    decision.reasons.push_back(SelectionReason::kGeoDataPresent);
  }
  if (signals.comparison_across_entities) {
    decision.reasons.push_back(SelectionReason::kComparisonAcrossEntities);
    decision.form = PresentationForm::kComposedSurface;
    return decision;
  }
  decision.form = visual_entries > 1 ? PresentationForm::kComposedSurface
                                     : PresentationForm::kSingleComponent;
  return decision;
}

}  // namespace seoul
