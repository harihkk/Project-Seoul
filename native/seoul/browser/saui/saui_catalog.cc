// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_catalog.h"

#include <iterator>

#include "base/containers/span.h"

namespace seoul {

namespace {

constexpr const char* kTextProps[] = {"text"};
constexpr const char* kIconProps[] = {"icon"};
constexpr const char* kImageProps[] = {"src", "alt"};
constexpr const char* kLinkProps[] = {"href", "text"};
constexpr const char* kCitationProps[] = {"href", "title"};
constexpr const char* kSourcesProps[] = {"sources"};
constexpr const char* kLabelProps[] = {"label"};
constexpr const char* kOptionsProps[] = {"label", "options"};
constexpr const char* kTitleProps[] = {"title"};
constexpr const char* kChartProps[] = {"title", "x_label", "y_label", "units"};
constexpr const char* kBarChartProps[] = {"title", "x_label", "y_label",
                                          "units", "baseline_zero"};
constexpr const char* kChipsProps[] = {"chips"};
constexpr const char* kCodeProps[] = {"code"};
constexpr const char* kDiffProps[] = {"diff"};

// One row per ComponentType, in enum declaration order. The static_assert
// below keeps this table and the enum in lockstep: adding a type without a row
// (or a row without a type) fails the build. Every row is a domain-neutral
// primitive; there are deliberately no industry-specific component types.
constexpr ComponentTypeInfo kCatalog[] = {
    // Foundation.
    {ComponentType::kText, "text", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, kTextProps, 1},
    {ComponentType::kRichText, "rich_text", ComponentCategory::kFoundation,
     false, false, false, false, kBindNone, false, kTextProps, 1},
    {ComponentType::kHeading, "heading", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, kTextProps, 1},
    {ComponentType::kDivider, "divider", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, nullptr, 0},
    {ComponentType::kBadge, "badge", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, kTextProps, 1},
    {ComponentType::kIcon, "icon", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, kIconProps, 1},
    {ComponentType::kImage, "image", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, kImageProps, 2},
    {ComponentType::kLink, "link", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, kLinkProps, 2},
    {ComponentType::kCitation, "citation", ComponentCategory::kFoundation,
     false, false, false, false, kBindNone, false, kCitationProps, 2},
    {ComponentType::kSourceList, "source_list", ComponentCategory::kFoundation,
     false, false, false, false, kBindNone, false, kSourcesProps, 1},
    {ComponentType::kProgress, "progress", ComponentCategory::kFoundation,
     false, false, false, false, kBindScalar, false, kLabelProps, 1},
    {ComponentType::kSpinner, "spinner", ComponentCategory::kFoundation, false,
     false, false, false, kBindNone, false, nullptr, 0},
    {ComponentType::kErrorState, "error_state", ComponentCategory::kFoundation,
     false, false, false, false, kBindNone, false, kTextProps, 1},
    {ComponentType::kEmptyState, "empty_state", ComponentCategory::kFoundation,
     false, false, false, false, kBindNone, false, kTextProps, 1},
    // Layout.
    {ComponentType::kStack, "stack", ComponentCategory::kLayout, true, false,
     false, false, kBindNone, false, nullptr, 0},
    {ComponentType::kRow, "row", ComponentCategory::kLayout, true, false,
     false, false, kBindNone, false, nullptr, 0},
    {ComponentType::kGrid, "grid", ComponentCategory::kLayout, true, false,
     false, false, kBindNone, false, nullptr, 0},
    {ComponentType::kTabs, "tabs", ComponentCategory::kLayout, true, false,
     false, true, kBindNone, false, nullptr, 0},
    {ComponentType::kCollapsibleSection, "collapsible_section",
     ComponentCategory::kLayout, true, false, false, false, kBindNone, false,
     kTitleProps, 1},
    {ComponentType::kResizablePanel, "resizable_panel",
     ComponentCategory::kLayout, true, false, false, false, kBindNone, false,
     nullptr, 0},
    {ComponentType::kCarousel, "carousel", ComponentCategory::kLayout, true,
     false, false, true, kBindNone, false, nullptr, 0},
    {ComponentType::kDetailDrawer, "detail_drawer", ComponentCategory::kLayout,
     true, false, false, true, kBindNone, false, kTitleProps, 1},
    // Input.
    {ComponentType::kButton, "button", ComponentCategory::kInput, false, true,
     false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kSegmentedControl, "segmented_control",
     ComponentCategory::kInput, false, true, false, true, kBindNone, false,
     kOptionsProps, 2},
    {ComponentType::kCheckbox, "checkbox", ComponentCategory::kInput, false,
     true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kRadioGroup, "radio_group", ComponentCategory::kInput,
     false, true, false, true, kBindNone, false, kOptionsProps, 2},
    {ComponentType::kSelect, "select", ComponentCategory::kInput, false, true,
     false, true, kBindNone, false, kOptionsProps, 2},
    {ComponentType::kTextInput, "text_input", ComponentCategory::kInput, false,
     true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kNumericInput, "numeric_input", ComponentCategory::kInput,
     false, true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kDateInput, "date_input", ComponentCategory::kInput, false,
     true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kDateRangeInput, "date_range_input",
     ComponentCategory::kInput, false, true, false, true, kBindNone, false,
     kLabelProps, 1},
    {ComponentType::kTimeInput, "time_input", ComponentCategory::kInput, false,
     true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kSlider, "slider", ComponentCategory::kInput, false, true,
     false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kSearchField, "search_field", ComponentCategory::kInput,
     false, true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kFilePicker, "file_picker", ComponentCategory::kInput,
     false, true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kConfirmation, "confirmation", ComponentCategory::kInput,
     false, true, false, true, kBindNone, false, kLabelProps, 1},
    {ComponentType::kSchemaForm, "schema_form", ComponentCategory::kInput,
     false, true, false, true, kBindRecord | kBindTable, true, nullptr, 0},
    // Data.
    {ComponentType::kKeyValueCard, "key_value_card", ComponentCategory::kData,
     false, false, false, false, kBindRecord, true, nullptr, 0},
    {ComponentType::kMetric, "metric", ComponentCategory::kData, false, false,
     false, false, kBindScalar, true, kLabelProps, 1},
    {ComponentType::kEntityCard, "entity_card", ComponentCategory::kData,
     false, false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kList, "list", ComponentCategory::kData, false, false,
     false, false, kBindTable, false, nullptr, 0},
    {ComponentType::kTable, "table", ComponentCategory::kData, false, false,
     false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kSortableTable, "sortable_table", ComponentCategory::kData,
     false, true, false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kComparisonMatrix, "comparison_matrix",
     ComponentCategory::kData, false, false, false, true, kBindTable, true,
     nullptr, 0},
    {ComponentType::kTimeline, "timeline", ComponentCategory::kData, false,
     false, false, true, kBindTable, false, nullptr, 0},
    {ComponentType::kActivityLog, "activity_log", ComponentCategory::kData,
     false, false, false, true, kBindTable, false, nullptr, 0},
    {ComponentType::kTree, "tree", ComponentCategory::kData, false, false,
     false, true, kBindTable, false, nullptr, 0},
    {ComponentType::kPagination, "pagination", ComponentCategory::kData, false,
     true, false, true, kBindNone, false, nullptr, 0},
    {ComponentType::kFilterChips, "filter_chips", ComponentCategory::kData,
     false, true, false, true, kBindNone, false, kChipsProps, 1},
    {ComponentType::kDocument, "document", ComponentCategory::kData, false,
     false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kMedia, "media", ComponentCategory::kData, false, false,
     false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kFile, "file", ComponentCategory::kData, false, false,
     false, true, kBindRecord, true, nullptr, 0},
    // Charts and visualizations.
    {ComponentType::kLineChart, "line_chart", ComponentCategory::kChart, false,
     false, true, true, kBindSeries | kBindTable, true, kChartProps, 4},
    {ComponentType::kAreaChart, "area_chart", ComponentCategory::kChart, false,
     false, true, true, kBindSeries | kBindTable, true, kChartProps, 4},
    {ComponentType::kBarChart, "bar_chart", ComponentCategory::kChart, false,
     false, true, true, kBindTable, true, kBarChartProps, 5},
    {ComponentType::kStackedBarChart, "stacked_bar_chart",
     ComponentCategory::kChart, false, false, true, true, kBindTable, true,
     kBarChartProps, 5},
    {ComponentType::kScatterChart, "scatter_chart", ComponentCategory::kChart,
     false, false, true, true, kBindSeries | kBindTable, true, kChartProps, 4},
    {ComponentType::kPieChart, "pie_chart", ComponentCategory::kChart, false,
     false, true, true, kBindTable, true, kChartProps, 4},
    {ComponentType::kCandlestickChart, "candlestick_chart",
     ComponentCategory::kChart, false, false, true, true, kBindTable, true,
     kChartProps, 4},
    {ComponentType::kRangeChart, "range_chart", ComponentCategory::kChart,
     false, false, true, true, kBindSeries | kBindTable, true, kChartProps, 4},
    {ComponentType::kSparkline, "sparkline", ComponentCategory::kChart, false,
     false, true, true, kBindSeries, true, kChartProps, 4},
    {ComponentType::kHistogram, "histogram", ComponentCategory::kChart, false,
     false, true, true, kBindSeries | kBindTable, true, kChartProps, 4},
    {ComponentType::kHeatMap, "heat_map", ComponentCategory::kChart, false,
     false, true, true, kBindTable, true, kChartProps, 4},
    // A relational graph is a visualization but not an axis chart; the axis
    // honesty rules do not apply, so chart=false with its own requirements.
    {ComponentType::kNetworkGraph, "network_graph", ComponentCategory::kChart,
     false, false, false, true, kBindTable, true, kTitleProps, 1},
    // Maps.
    {ComponentType::kMap, "map", ComponentCategory::kMap, false, false, false,
     true, kBindTable, true, kTitleProps, 1},
    {ComponentType::kGeoLayer, "geo_layer", ComponentCategory::kMap, false,
     false, false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kMapMarkerList, "map_marker_list", ComponentCategory::kMap,
     false, true, false, true, kBindTable, true, nullptr, 0},
    // Workflow.
    {ComponentType::kTaskGraph, "task_graph", ComponentCategory::kWorkflow,
     false, false, false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kWorkflowNode, "workflow_node",
     ComponentCategory::kWorkflow, false, false, false, true, kBindRecord,
     true, nullptr, 0},
    {ComponentType::kWorkflowEdge, "workflow_edge",
     ComponentCategory::kWorkflow, false, false, false, true, kBindRecord,
     true, nullptr, 0},
    {ComponentType::kTriggerCard, "trigger_card", ComponentCategory::kWorkflow,
     false, false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kApprovalRequest, "approval_request",
     ComponentCategory::kWorkflow, false, true, false, true, kBindRecord, true,
     nullptr, 0},
    {ComponentType::kExecutionStatus, "execution_status",
     ComponentCategory::kWorkflow, false, false, false, true, kBindRecord,
     true, nullptr, 0},
    {ComponentType::kResultCard, "result_card", ComponentCategory::kWorkflow,
     false, false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kRetryControl, "retry_control",
     ComponentCategory::kWorkflow, false, true, false, true, kBindNone, false,
     kLabelProps, 1},
    // Task transparency.
    {ComponentType::kPlanView, "plan_view", ComponentCategory::kTask, false,
     false, false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kCurrentStep, "current_step", ComponentCategory::kTask,
     false, false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kActionReceipt, "action_receipt", ComponentCategory::kTask,
     false, false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kBlockerList, "blocker_list", ComponentCategory::kTask,
     false, false, false, true, kBindTable, true, nullptr, 0},
    {ComponentType::kCostSummary, "cost_summary", ComponentCategory::kTask,
     false, false, false, true, kBindRecord, true, nullptr, 0},
    {ComponentType::kProviderIndicator, "provider_indicator",
     ComponentCategory::kTask, false, false, false, true, kBindRecord, true,
     nullptr, 0},
    // Code and technical.
    {ComponentType::kCodeBlock, "code_block", ComponentCategory::kCode, false,
     false, false, false, kBindNone, false, kCodeProps, 1},
    {ComponentType::kDiffView, "diff_view", ComponentCategory::kCode, false,
     false, false, false, kBindNone, false, kDiffProps, 1},
    {ComponentType::kDiagnosticList, "diagnostic_list",
     ComponentCategory::kCode, false, false, false, true, kBindTable, true,
     nullptr, 0},
    {ComponentType::kLogViewer, "log_viewer", ComponentCategory::kCode, false,
     false, false, true, kBindTable, false, nullptr, 0},
    {ComponentType::kStackTrace, "stack_trace", ComponentCategory::kCode,
     false, false, false, false, kBindTable, false, nullptr, 0},
    {ComponentType::kFileTree, "file_tree", ComponentCategory::kCode, false,
     false, false, true, kBindTable, true, nullptr, 0},
};

// The table must cover the enum exactly, in declaration order (kFileTree is
// the last enum value; keep it last when extending both).
static_assert(std::size(kCatalog) ==
                  static_cast<size_t>(ComponentType::kFileTree) + 1,
              "component catalog and ComponentType enum are out of sync");

}  // namespace

uint8_t DataEntryKindBit(DataEntryKind kind) {
  switch (kind) {
    case DataEntryKind::kScalar:
      return kBindScalar;
    case DataEntryKind::kRecord:
      return kBindRecord;
    case DataEntryKind::kSeries:
      return kBindSeries;
    case DataEntryKind::kTable:
      return kBindTable;
  }
  return kBindNone;
}

const ComponentTypeInfo& GetComponentTypeInfo(ComponentType type) {
  const size_t index = static_cast<size_t>(type);
  // The static_assert above guarantees coverage; the index is always in range
  // for a valid enum value. base::span gives a bounds-checked access.
  return base::span(kCatalog)[index];
}

const ComponentTypeInfo* FindComponentTypeByName(std::string_view name) {
  for (const ComponentTypeInfo& info : kCatalog) {
    if (name == info.name) {
      return &info;
    }
  }
  return nullptr;
}

const char* ComponentTypeName(ComponentType type) {
  return GetComponentTypeInfo(type).name;
}

size_t ComponentTypeCount() {
  return std::size(kCatalog);
}

}  // namespace seoul
