// Project Seoul semantic data fabric.
// One generic, versioned result model for every capability outcome. A result
// is a shape (scalar, record, collection, table, series, intervals, events,
// hierarchy, graph, geospatial, route, document, citations, media, artifact,
// form schema, action set, diff, code structure, composite) plus fields whose
// meaning is carried by semantic roles and metadata, never by business-domain
// types. The interface compiler reasons over shapes and roles only, so a
// schema Seoul has never seen still validates and renders.

#ifndef SEOUL_BROWSER_SEMANTIC_SEMANTIC_TYPES_H_
#define SEOUL_BROWSER_SEMANTIC_SEMANTIC_TYPES_H_

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/data/provenance.h"

namespace seoul {

inline constexpr int kSemanticSchemaVersion = 1;
inline constexpr size_t kMaxSemanticFields = 128;
inline constexpr size_t kMaxSemanticRows = 10000;
inline constexpr size_t kMaxCompositeParts = 16;
inline constexpr size_t kMaxCompositeDepth = 4;
inline constexpr size_t kMaxFieldIdLength = 64;
inline constexpr size_t kMaxFieldLabelLength = 200;

// The generic shapes a semantic result can take. Shape describes structure;
// roles describe meaning. There are no domain shapes.
enum class SemanticShape {
  kScalar,
  kRecord,
  kEntityCollection,
  kTable,
  kCube,  // a table with two or more dimension-role fields
  kSeries,
  kTimeSeries,
  kIntervalSeries,
  kEventStream,
  kStatusStream,
  kHierarchy,
  kGraph,
  kGeoFeatures,
  kRoute,
  kDocument,
  kDocumentSections,
  kCitations,
  kMedia,
  kArtifact,
  kFormSchema,  // typed inputs Seoul still needs from the user
  kActionSet,
  kDiff,
  kCodeStructure,
  kComposite,
};

const char* SemanticShapeToString(SemanticShape shape);
bool SemanticShapeFromString(std::string_view s, SemanticShape* out);

// Field meaning, independent of domain. kOpen/kHigh/kLow/kClose are generic
// interval-summary roles (any measure sampled over an interval), not finance
// concepts.
enum class SemanticRole {
  kNone,
  kIdentifier,
  kName,
  kDescription,
  kMeasure,
  kDimension,
  kTimestamp,
  kIntervalStart,
  kIntervalEnd,
  kDuration,
  kLatitude,
  kLongitude,
  kUrl,
  kMediaUrl,
  kCategory,
  kStatus,
  kSeverity,
  kMoney,
  kPercentage,
  kCount,
  kOpen,
  kHigh,
  kLow,
  kClose,
  kParentReference,  // hierarchy
  kSourceNode,       // graph edge
  kTargetNode,       // graph edge
  kBody,             // document/diff body
  kCitationRef,
  kMimeType,
};

const char* SemanticRoleToString(SemanticRole role);
bool SemanticRoleFromString(std::string_view s, SemanticRole* out);

// True for roles that quantify (usable as a chart measure).
bool IsMeasureRole(SemanticRole role);

enum class FieldPrimitive {
  kString,
  kInteger,
  kNumber,
  kBoolean,
  kTimestamp,  // milliseconds since the Unix epoch, as a number
};

enum class ValueClass {
  kCategorical,
  kOrdinal,
  kContinuous,
  kFreeText,
};

enum class FieldSensitivity {
  kPublic,
  kPersonal,
  kSensitive,
};

// One field's identity, type, and semantics.
struct FieldSpec {
  FieldSpec();
  FieldSpec(const FieldSpec&);
  FieldSpec(FieldSpec&&);
  FieldSpec& operator=(const FieldSpec&);
  FieldSpec& operator=(FieldSpec&&);
  ~FieldSpec();

  std::string id;  // [a-z][a-z0-9_]{0,63}
  std::string label;
  std::string description;
  FieldPrimitive primitive = FieldPrimitive::kString;
  bool nullable = true;
  SemanticRole role = SemanticRole::kNone;
  std::string unit;           // display unit ("C", "ms", "km")
  std::string currency_code;  // ISO alpha code for kMoney fields
  std::string locale;
  std::string timezone;  // IANA zone for timestamp display
  std::string format;    // provider display-format hint
  ValueClass value_class = ValueClass::kContinuous;
  double confidence = 1.0;  // [0, 1]
  int precision = -1;       // significant fraction digits; -1 unknown
  FieldSensitivity sensitivity = FieldSensitivity::kPublic;
  bool sortable = true;
  bool filterable = true;
  bool aggregatable = false;

  friend bool operator==(const FieldSpec&, const FieldSpec&) = default;
};

struct Citation {
  Citation();
  Citation(const Citation&);
  Citation(Citation&&);
  Citation& operator=(const Citation&);
  Citation& operator=(Citation&&);
  ~Citation();

  std::string url;
  std::string title;

  friend bool operator==(const Citation&, const Citation&) = default;
};

// Attribution for a semantic result: the generic provenance envelope plus
// provider identity, the transformations Seoul performed, and citations.
struct SemanticProvenance {
  SemanticProvenance();
  SemanticProvenance(const SemanticProvenance&);
  SemanticProvenance(SemanticProvenance&&);
  SemanticProvenance& operator=(const SemanticProvenance&);
  SemanticProvenance& operator=(SemanticProvenance&&);
  ~SemanticProvenance();

  DataProvenance base;
  std::string provider;  // capability provider identity
  std::vector<std::string> transformations;
  std::vector<Citation> citations;

  friend bool operator==(const SemanticProvenance&,
                         const SemanticProvenance&) = default;
};

struct SemanticError {
  SemanticError();
  SemanticError(const SemanticError&);
  SemanticError(SemanticError&&);
  SemanticError& operator=(const SemanticError&);
  SemanticError& operator=(SemanticError&&);
  ~SemanticError();

  std::string code;
  std::string message;

  friend bool operator==(const SemanticError&, const SemanticError&) = default;
};

// A disagreement between sources about one field; surfaced, never silently
// resolved.
struct SourceConflict {
  SourceConflict();
  SourceConflict(const SourceConflict&);
  SourceConflict(SourceConflict&&);
  SourceConflict& operator=(const SourceConflict&);
  SourceConflict& operator=(SourceConflict&&);
  ~SourceConflict();

  std::string field_id;
  std::string source_a;
  std::string source_b;
  std::string note;

  friend bool operator==(const SourceConflict&,
                         const SourceConflict&) = default;
};

enum class ResultState {
  kComplete,
  kPartial,
  kStreaming,
  kFailed,
};

// The schema of one result. `fields` describes the scalar/record fields or
// the per-row fields of list shapes; `edge_fields` describes graph edges;
// `parts`/`part_names` describe kComposite children (bounded recursion).
struct SemanticSchema {
  SemanticSchema();
  SemanticSchema(const SemanticSchema&);
  SemanticSchema(SemanticSchema&&);
  SemanticSchema& operator=(const SemanticSchema&);
  SemanticSchema& operator=(SemanticSchema&&);
  ~SemanticSchema();

  int schema_version = kSemanticSchemaVersion;
  SemanticShape shape = SemanticShape::kRecord;
  std::vector<FieldSpec> fields;
  std::vector<FieldSpec> edge_fields;
  std::vector<SemanticSchema> parts;
  std::vector<std::string> part_names;

  friend bool operator==(const SemanticSchema&,
                         const SemanticSchema&) = default;
};

// Data layout per shape (validated by ValidateSemanticResult):
// - kScalar: one primitive value.
// - kRecord / kDocument / kArtifact / kDiff: a dict keyed by field id.
// - list shapes (collection, table, cube, series, events, statuses, geo,
//   route, citations, media, form schema, action set, code structure,
//   document sections, hierarchy): a list of dicts keyed by field id.
// - kGraph: {"nodes": [dict...], "edges": [dict...]}.
// - kComposite: a dict keyed by part name, each conforming to its part.
struct SemanticResult {
  SemanticResult();
  SemanticResult(const SemanticResult&);
  SemanticResult(SemanticResult&&);
  SemanticResult& operator=(const SemanticResult&);
  SemanticResult& operator=(SemanticResult&&);
  ~SemanticResult();

  SemanticSchema schema;
  base::Value data;
  SemanticProvenance provenance;
  ResultState state = ResultState::kComplete;
  std::string continuation_token;  // pagination; empty when none
  // Fields the source could not supply. They must be absent from the data:
  // Seoul reports gaps instead of fabricating values.
  std::vector<std::string> unavailable_field_ids;
  std::vector<SemanticError> errors;
  std::vector<SourceConflict> conflicts;

  friend bool operator==(const SemanticResult&,
                         const SemanticResult&) = default;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SEMANTIC_SEMANTIC_TYPES_H_
