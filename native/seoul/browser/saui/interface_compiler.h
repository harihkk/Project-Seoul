// Project Seoul Adaptive UI (SAUI).
// The adaptive interface compiler. Input: a validated semantic result plus
// the user's interface intent. Output: a validated declarative surface
// composed from trusted generic primitives. Every decision is a
// shape-and-semantics rule (cardinality, roles, temporal/geo/hierarchy
// structure, requested representation, display constraints); there are no
// task-domain conditionals, and a schema Seoul has never seen falls back to
// generic record/table/document rendering instead of failing.

#ifndef SEOUL_BROWSER_SAUI_INTERFACE_COMPILER_H_
#define SEOUL_BROWSER_SAUI_INTERFACE_COMPILER_H_

#include <optional>
#include <string>
#include <vector>

#include "seoul/browser/saui/saui_errors.h"
#include "seoul/browser/saui/saui_types.h"
#include "seoul/browser/semantic/semantic_types.h"

namespace seoul {

// User-directed presentation intent for one turn. Field-level directives
// ("hide that column", "compare only these") carry stable field/entity ids
// resolved by the reference layer, never screen positions.
struct InterfaceIntent {
  std::string title;  // surface title; required for pinned surfaces
  // Explicit representation request ("show this as a table"). Honored only
  // when compatible with the data; a misleading request falls back with a
  // recorded reason.
  std::optional<ComponentType> requested_representation;
  bool pin = false;             // persist as a dashboard surface
  bool wants_editable = false;  // prefer a form over a read-only view
  bool percentages = false;     // normalize stacked/pie style presentations
  std::string group_by_field_id;
  std::vector<std::string> hidden_field_ids;
  std::vector<std::string> compare_entity_ids;
  int canvas_width_px = 0;  // 0 when unknown
};

// Why the compiler chose what it chose. Surfaced for transparency and tests.
enum class CompilerReason {
  kShapeScalarMetric,
  kRecordEntityCard,
  kRecordKeyValue,
  kComparableEntitiesMatrix,
  kCollectionTable,
  kTemporalMeasureLineChart,
  kOhlcCandlestick,
  kGeoMap,
  kHierarchyTree,
  kGraphNetwork,
  kIntervalsTimeline,
  kEventsTimeline,
  kStatusActivityLog,
  kFormFromMissingInputs,
  kDocumentView,
  kDocumentSections,
  kCitationsSourceList,
  kMediaGallery,
  kArtifactFile,
  kActionButtons,
  kDiffView,
  kCodeStructureTree,
  kCompositeSections,
  kEmptyResult,
  kFallbackGenericTable,
  kFallbackRecordCard,
  kUserRequestedRepresentation,
  kUserRequestRejectedMisleading,
  kChartWouldMislead,
  kRowsTruncated,
  kFieldsHidden,
  kFilteredToComparedEntities,
};

const char* CompilerReasonToString(CompilerReason reason);

struct CompiledInterface {
  AdaptiveSurface surface;
  std::vector<CompilerReason> reasons;
};

using CompileResult = base::expected<CompiledInterface, SauiError>;

// Compiles a surface for `result` under `intent`. When `existing_surface` is
// valid, the compiled surface keeps that id so the renderer updates the same
// surface in place; the same stored semantic result can be re-compiled under
// a new intent without refetching or re-reasoning. The returned surface has
// passed ValidateSurface.
CompileResult CompileInterface(const SemanticResult& result,
                               const InterfaceIntent& intent,
                               const SurfaceId& existing_surface = SurfaceId());

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_INTERFACE_COMPILER_H_
