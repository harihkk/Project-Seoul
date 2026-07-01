// Project Seoul Adaptive UI (SAUI).
// Presentation selection: decides whether a response should be text alone,
// one composed component, a coordinated multi-component surface, or an
// editable form, and whether it should persist. Deterministic rules over
// explicit signals; a chart is never selected when the available data cannot
// honestly back one.

#ifndef SEOUL_BROWSER_SAUI_SAUI_SELECTION_H_
#define SEOUL_BROWSER_SAUI_SAUI_SELECTION_H_

#include <vector>

#include "seoul/browser/saui/saui_types.h"

namespace seoul {

enum class PresentationForm {
  kTextOnly,
  kSingleComponent,
  kComposedSurface,
  kEditableForm,
};

enum class SelectionReason {
  kUserRequestedTextOnly,
  kUserRequestedVisual,
  kMissingInputsFormBeatsQuestions,
  kSingleScalarNoChart,
  kNoEligibleData,
  kTimeSeriesEligible,
  kComparisonAcrossEntities,
  kGeoDataPresent,
  kMonitoringPersists,
  kTaskResultPersists,
};

// Explicit inputs to the decision. Callers derive these from the goal, the
// collected tool results, and the user's phrasing; nothing here inspects raw
// model text.
struct PresentationSignals {
  bool user_requested_visual = false;
  bool user_requested_text_only = false;
  // Count of required inputs the user has not supplied yet (a form beats a
  // chain of one-question turns when this is 2+).
  int missing_required_inputs = 0;
  // Data availability, computed against real tool results (see
  // EntryChartEligible for the chart gate).
  int chart_eligible_entries = 0;
  int record_entries = 0;
  bool only_single_scalar = false;
  bool comparison_across_entities = false;
  bool has_geo_coordinates = false;
  bool monitoring = false;
  bool task_produces_persistent_result = false;
};

struct PresentationDecision {
  PresentationForm form = PresentationForm::kTextOnly;
  bool persist = false;
  std::vector<SelectionReason> reasons;
};

PresentationDecision SelectPresentation(const PresentationSignals& signals);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_SELECTION_H_
