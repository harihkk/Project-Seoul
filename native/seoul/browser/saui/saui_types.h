// Project Seoul Adaptive UI (SAUI).
// The Seoul-owned declarative surface document. A model emits data conforming
// to this schema; the trusted Seoul renderer creates the interface. No
// component carries executable code, raw HTML, or event-handler scripts; a
// component action maps to a typed event dispatched back to Seoul.

#ifndef SEOUL_BROWSER_SAUI_SAUI_TYPES_H_
#define SEOUL_BROWSER_SAUI_SAUI_TYPES_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "seoul/browser/data/provenance.h"
#include "seoul/browser/saui/saui_limits.h"

namespace seoul {

// Strong id for a surface, allocated by Seoul (never by the model).
class SurfaceId {
 public:
  SurfaceId();
  SurfaceId(const SurfaceId&);
  SurfaceId(SurfaceId&&);
  SurfaceId& operator=(const SurfaceId&);
  SurfaceId& operator=(SurfaceId&&);
  ~SurfaceId();

  static SurfaceId GenerateNew() {
    SurfaceId id;
    id.uuid_ = base::Uuid::GenerateRandomV4();
    return id;
  }
  static SurfaceId FromString(std::string_view s) {
    SurfaceId id;
    id.uuid_ = base::Uuid::ParseLowercase(s);
    return id;
  }
  bool is_valid() const { return uuid_.is_valid(); }
  std::string value() const {
    return uuid_.is_valid() ? uuid_.AsLowercaseString() : std::string();
  }
  friend bool operator==(const SurfaceId& a, const SurfaceId& b) {
    return a.uuid_ == b.uuid_;
  }
  friend bool operator<(const SurfaceId& a, const SurfaceId& b) {
    return a.uuid_ < b.uuid_;
  }

 private:
  base::Uuid uuid_;
};

// Presentation context of a surface.
enum class SurfaceKind {
  kResponse,        // inline answer in the conversation
  kForm,            // input collection
  kDashboard,       // pinned, persistent, data-refreshing
  kReport,          // citation-carrying document
  kApproval,        // action approval request
  kWorkflowCanvas,  // editable workflow graph
  kTaskStatus,      // live task progress
  kMonitor,         // recurring observation results
};

enum class ComponentCategory {
  kFoundation,
  kLayout,
  kInput,
  kData,
  kChart,
  kWorkflow,
  kTask,
  kMap,
  kCode,
};

// The trusted, domain-neutral component catalog. Every renderable component
// type is a generic primitive selected by data shape and semantics, never by
// business domain; unknown types are rejected at parse time. Catalog metadata
// for each type lives in saui_catalog.cc. Keep kFileTree last: the catalog
// table's static_assert uses it as the enum bound.
enum class ComponentType {
  // Foundation.
  kText,
  kRichText,
  kHeading,
  kDivider,
  kBadge,
  kIcon,
  kImage,
  kLink,
  kCitation,
  kSourceList,
  kProgress,
  kSpinner,
  kErrorState,
  kEmptyState,
  // Layout.
  kStack,
  kRow,
  kGrid,
  kTabs,
  kCollapsibleSection,
  kResizablePanel,
  kCarousel,
  kDetailDrawer,
  // Input.
  kButton,
  kSegmentedControl,
  kCheckbox,
  kRadioGroup,
  kSelect,
  kTextInput,
  kNumericInput,
  kDateInput,
  kDateRangeInput,
  kTimeInput,
  kSlider,
  kSearchField,
  kFilePicker,
  kConfirmation,
  // A form generated dynamically from a typed schema bound as data (used for
  // missing task parameters and connector inputs).
  kSchemaForm,
  // Data.
  kKeyValueCard,
  kMetric,
  // Generic entity presentation: any record with an identifier/name and
  // typed fields renders as an entity card regardless of domain.
  kEntityCard,
  kList,
  kTable,
  kSortableTable,
  kComparisonMatrix,
  kTimeline,
  kActivityLog,
  kTree,
  kPagination,
  kFilterChips,
  kDocument,
  kMedia,
  kFile,
  // Charts and visualizations. Candlestick is retained as a generic
  // open/high/low/close interval-summary primitive, not a finance widget.
  kLineChart,
  kAreaChart,
  kBarChart,
  kStackedBarChart,
  kScatterChart,
  kPieChart,
  kCandlestickChart,
  kRangeChart,
  kSparkline,
  kHistogram,
  kHeatMap,
  kNetworkGraph,
  // Maps.
  kMap,
  kGeoLayer,
  kMapMarkerList,
  // Workflow.
  kTaskGraph,
  kWorkflowNode,
  kWorkflowEdge,
  kTriggerCard,
  kApprovalRequest,
  kExecutionStatus,
  kResultCard,
  kRetryControl,
  // Task transparency.
  kPlanView,
  kCurrentStep,
  kActionReceipt,
  kBlockerList,
  kCostSummary,
  kProviderIndicator,
  // Code and technical.
  kCodeBlock,
  kDiffView,
  kDiagnosticList,
  kLogViewer,
  kStackTrace,
  kFileTree,  // keep last: catalog static_assert bound
};

enum class ComponentState {
  kReady,
  kLoading,
  kError,
  kEmpty,
};

// Whether the renderer should expect in-place data refreshes for a component.
enum class UpdatePolicy {
  kStatic,
  kLive,
};

// Typed data slots referenced by component bindings. Data never lives inside
// component props; components bind entries by name.
enum class DataEntryKind {
  kScalar,  // one primitive value
  kRecord,  // one flat dictionary of primitives (a card's fields)
  kSeries,  // ordered numeric points, optionally time-indexed
  kTable,   // columns plus rows of primitives
};

struct SeriesPoint {
  bool has_time = false;
  base::Time time;  // set when has_time
  double x = 0.0;   // used when !has_time
  double y = 0.0;

  friend bool operator==(const SeriesPoint&, const SeriesPoint&) = default;
};

struct DataSeries {
  DataSeries();
  DataSeries(const DataSeries&);
  DataSeries(DataSeries&&);
  DataSeries& operator=(const DataSeries&);
  DataSeries& operator=(DataSeries&&);
  ~DataSeries();

  std::string x_unit;
  std::string y_unit;
  std::vector<SeriesPoint> points;

  friend bool operator==(const DataSeries&, const DataSeries&) = default;
};

// Kept an aggregate on purpose: parsers build columns with brace
// initialization (columns.push_back({key, label})), which declaring any
// constructor would break.
struct TableColumn {
  std::string key;
  std::string label;

  friend bool operator==(const TableColumn&, const TableColumn&) = default;
};

// base::Value, base::DictValue, and base::ListValue are move-only (their
// copy constructors are deleted; use Clone()). The structs below hold them by
// value, so they declare explicit clone-based copy semantics. Every aggregate
// composed only of these (AdaptiveSurface, DataEntry) then becomes copyable
// automatically.
struct DataTable {
  DataTable();
  DataTable(const DataTable&);
  DataTable(DataTable&&);
  DataTable& operator=(const DataTable&);
  DataTable& operator=(DataTable&&);
  ~DataTable();

  std::vector<TableColumn> columns;
  // Row cells are primitives only (string/double/bool/int); validated at
  // parse. Cell count is bounded by kMaxTableRows * kMaxTableColumns.
  std::vector<base::ListValue> rows;

  friend bool operator==(const DataTable&, const DataTable&) = default;
};

struct DataEntry {
  DataEntry();
  DataEntry(const DataEntry&);
  DataEntry(DataEntry&&);
  DataEntry& operator=(const DataEntry&);
  DataEntry& operator=(DataEntry&&);
  ~DataEntry();

  DataEntryKind kind = DataEntryKind::kScalar;
  base::Value scalar;        // kScalar
  base::DictValue record;  // kRecord
  DataSeries series;         // kSeries
  DataTable table;           // kTable
  bool has_provenance = false;
  DataProvenance provenance;

  friend bool operator==(const DataEntry&, const DataEntry&) = default;
};

// What a surface-level action does when a component triggers it. The payload
// is validated again by the receiving layer (tool schema, workflow editor,
// command validation); SAUI guarantees shape and bounded size only.
enum class SurfaceActionKind {
  kToolCall,       // target: ToolId string
  kLocalState,     // target: surface-local state key
  kWorkflowEdit,   // target: workflow node id
  kBrowserAction,  // target: browser command name from the launcher catalog
  kTaskApproval,   // target: task step id
  kNavigate,       // target: http(s) URL
};

struct SurfaceAction {
  SurfaceAction();
  SurfaceAction(const SurfaceAction&);
  SurfaceAction(SurfaceAction&&);
  SurfaceAction& operator=(const SurfaceAction&);
  SurfaceAction& operator=(SurfaceAction&&);
  ~SurfaceAction();

  std::string id;
  std::string label;
  SurfaceActionKind kind = SurfaceActionKind::kLocalState;
  std::string target;
  base::DictValue payload;
  bool requires_confirmation = false;

  friend bool operator==(const SurfaceAction&, const SurfaceAction&) = default;
};

struct ComponentNode {
  ComponentNode();
  ComponentNode(const ComponentNode&);
  ComponentNode(ComponentNode&&);
  ComponentNode& operator=(const ComponentNode&);
  ComponentNode& operator=(ComponentNode&&);
  ~ComponentNode();

  std::string id;  // stable, generator-assigned, charset-validated
  ComponentType type = ComponentType::kText;
  base::DictValue props;
  // slot name -> data entry name. The primary slot is "data".
  std::map<std::string, std::string> bindings;
  std::string accessible_name;
  ComponentState state = ComponentState::kReady;
  std::string state_message;
  UpdatePolicy update_policy = UpdatePolicy::kStatic;
  std::vector<std::string> action_ids;
  std::vector<ComponentNode> children;

  friend bool operator==(const ComponentNode&, const ComponentNode&) = default;
};

// Who generated the surface and from what. "deterministic" marks surfaces
// built without a model.
struct SurfaceProvenance {
  SurfaceProvenance();
  SurfaceProvenance(const SurfaceProvenance&);
  SurfaceProvenance(SurfaceProvenance&&);
  SurfaceProvenance& operator=(const SurfaceProvenance&);
  SurfaceProvenance& operator=(SurfaceProvenance&&);
  ~SurfaceProvenance();

  std::string generator;  // "deterministic" or "model:<provider>/<model>"
  base::Time created_at;
  std::vector<std::string> source_urls;

  friend bool operator==(const SurfaceProvenance&,
                         const SurfaceProvenance&) = default;
};

struct AdaptiveSurface {
  AdaptiveSurface();
  AdaptiveSurface(const AdaptiveSurface&);
  AdaptiveSurface(AdaptiveSurface&&);
  AdaptiveSurface& operator=(const AdaptiveSurface&);
  AdaptiveSurface& operator=(AdaptiveSurface&&);
  ~AdaptiveSurface();

  SurfaceId id;
  SurfaceKind kind = SurfaceKind::kResponse;
  int schema_version = kSauiSchemaVersion;
  std::string title;
  std::vector<ComponentNode> components;
  std::map<std::string, DataEntry> data;
  std::vector<SurfaceAction> actions;
  SurfaceProvenance provenance;
  bool pinned = false;
  uint64_t revision = 1;

  friend bool operator==(const AdaptiveSurface&,
                         const AdaptiveSurface&) = default;
};

const char* SurfaceKindToString(SurfaceKind kind);
bool SurfaceKindFromString(std::string_view s, SurfaceKind* out);
const char* ComponentStateToString(ComponentState state);
bool ComponentStateFromString(std::string_view s, ComponentState* out);
const char* SurfaceActionKindToString(SurfaceActionKind kind);
bool SurfaceActionKindFromString(std::string_view s, SurfaceActionKind* out);
const char* DataEntryKindToString(DataEntryKind kind);
bool DataEntryKindFromString(std::string_view s, DataEntryKind* out);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_TYPES_H_
