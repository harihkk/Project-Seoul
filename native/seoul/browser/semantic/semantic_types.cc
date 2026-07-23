// Project Seoul semantic data fabric.

#include "seoul/browser/semantic/semantic_types.h"

#include <utility>

#include "seoul/browser/semantic/semantic_validation.h"

namespace seoul {

FieldSpec::FieldSpec() = default;
FieldSpec::FieldSpec(const FieldSpec&) = default;
FieldSpec::FieldSpec(FieldSpec&&) = default;
FieldSpec& FieldSpec::operator=(const FieldSpec&) = default;
FieldSpec& FieldSpec::operator=(FieldSpec&&) = default;
FieldSpec::~FieldSpec() = default;

Citation::Citation() = default;
Citation::Citation(const Citation&) = default;
Citation::Citation(Citation&&) = default;
Citation& Citation::operator=(const Citation&) = default;
Citation& Citation::operator=(Citation&&) = default;
Citation::~Citation() = default;

SemanticProvenance::SemanticProvenance() = default;
SemanticProvenance::SemanticProvenance(const SemanticProvenance&) = default;
SemanticProvenance::SemanticProvenance(SemanticProvenance&&) = default;
SemanticProvenance& SemanticProvenance::operator=(const SemanticProvenance&) =
    default;
SemanticProvenance& SemanticProvenance::operator=(SemanticProvenance&&) =
    default;
SemanticProvenance::~SemanticProvenance() = default;

SemanticError::SemanticError() = default;
SemanticError::SemanticError(const SemanticError&) = default;
SemanticError::SemanticError(SemanticError&&) = default;
SemanticError& SemanticError::operator=(const SemanticError&) = default;
SemanticError& SemanticError::operator=(SemanticError&&) = default;
SemanticError::~SemanticError() = default;

SourceConflict::SourceConflict() = default;
SourceConflict::SourceConflict(const SourceConflict&) = default;
SourceConflict::SourceConflict(SourceConflict&&) = default;
SourceConflict& SourceConflict::operator=(const SourceConflict&) = default;
SourceConflict& SourceConflict::operator=(SourceConflict&&) = default;
SourceConflict::~SourceConflict() = default;

SemanticSchema::SemanticSchema() = default;
SemanticSchema::SemanticSchema(const SemanticSchema&) = default;
SemanticSchema::SemanticSchema(SemanticSchema&&) = default;
SemanticSchema& SemanticSchema::operator=(const SemanticSchema&) = default;
SemanticSchema& SemanticSchema::operator=(SemanticSchema&&) = default;
SemanticSchema::~SemanticSchema() = default;

SemanticResult::SemanticResult() = default;
SemanticResult::SemanticResult(SemanticResult&&) = default;
SemanticResult& SemanticResult::operator=(SemanticResult&&) = default;
SemanticResult::~SemanticResult() = default;
// base::Value is move-only; a result copy deep-clones the data.
SemanticResult::SemanticResult(const SemanticResult& other)
    : schema(other.schema),
      data(other.data.Clone()),
      provenance(other.provenance),
      state(other.state),
      continuation_token(other.continuation_token),
      unavailable_field_ids(other.unavailable_field_ids),
      errors(other.errors),
      conflicts(other.conflicts) {}
SemanticResult& SemanticResult::operator=(const SemanticResult& other) {
  schema = other.schema;
  data = other.data.Clone();
  provenance = other.provenance;
  state = other.state;
  continuation_token = other.continuation_token;
  unavailable_field_ids = other.unavailable_field_ids;
  errors = other.errors;
  conflicts = other.conflicts;
  return *this;
}

namespace {

struct ShapeName {
  SemanticShape shape;
  const char* name;
};

constexpr ShapeName kShapeNames[] = {
    {SemanticShape::kScalar, "scalar"},
    {SemanticShape::kRecord, "record"},
    {SemanticShape::kEntityCollection, "entity_collection"},
    {SemanticShape::kTable, "table"},
    {SemanticShape::kCube, "cube"},
    {SemanticShape::kSeries, "series"},
    {SemanticShape::kTimeSeries, "time_series"},
    {SemanticShape::kIntervalSeries, "interval_series"},
    {SemanticShape::kEventStream, "event_stream"},
    {SemanticShape::kStatusStream, "status_stream"},
    {SemanticShape::kHierarchy, "hierarchy"},
    {SemanticShape::kGraph, "graph"},
    {SemanticShape::kGeoFeatures, "geo_features"},
    {SemanticShape::kRoute, "route"},
    {SemanticShape::kDocument, "document"},
    {SemanticShape::kDocumentSections, "document_sections"},
    {SemanticShape::kCitations, "citations"},
    {SemanticShape::kMedia, "media"},
    {SemanticShape::kArtifact, "artifact"},
    {SemanticShape::kFormSchema, "form_schema"},
    {SemanticShape::kActionSet, "action_set"},
    {SemanticShape::kDiff, "diff"},
    {SemanticShape::kCodeStructure, "code_structure"},
    {SemanticShape::kComposite, "composite"},
};

struct RoleName {
  SemanticRole role;
  const char* name;
};

constexpr RoleName kRoleNames[] = {
    {SemanticRole::kNone, "none"},
    {SemanticRole::kIdentifier, "identifier"},
    {SemanticRole::kName, "name"},
    {SemanticRole::kDescription, "description"},
    {SemanticRole::kMeasure, "measure"},
    {SemanticRole::kDimension, "dimension"},
    {SemanticRole::kTimestamp, "timestamp"},
    {SemanticRole::kIntervalStart, "interval_start"},
    {SemanticRole::kIntervalEnd, "interval_end"},
    {SemanticRole::kDuration, "duration"},
    {SemanticRole::kLatitude, "latitude"},
    {SemanticRole::kLongitude, "longitude"},
    {SemanticRole::kUrl, "url"},
    {SemanticRole::kMediaUrl, "media_url"},
    {SemanticRole::kCategory, "category"},
    {SemanticRole::kStatus, "status"},
    {SemanticRole::kSeverity, "severity"},
    {SemanticRole::kMoney, "money"},
    {SemanticRole::kPercentage, "percentage"},
    {SemanticRole::kCount, "count"},
    {SemanticRole::kOpen, "open"},
    {SemanticRole::kHigh, "high"},
    {SemanticRole::kLow, "low"},
    {SemanticRole::kClose, "close"},
    {SemanticRole::kParentReference, "parent_reference"},
    {SemanticRole::kSourceNode, "source_node"},
    {SemanticRole::kTargetNode, "target_node"},
    {SemanticRole::kBody, "body"},
    {SemanticRole::kCitationRef, "citation_ref"},
    {SemanticRole::kMimeType, "mime_type"},
};

}  // namespace

const char* SemanticShapeToString(SemanticShape shape) {
  for (const ShapeName& entry : kShapeNames) {
    if (entry.shape == shape) {
      return entry.name;
    }
  }
  return "record";
}

bool SemanticShapeFromString(std::string_view s, SemanticShape* out) {
  for (const ShapeName& entry : kShapeNames) {
    if (s == entry.name) {
      *out = entry.shape;
      return true;
    }
  }
  return false;
}

const char* SemanticRoleToString(SemanticRole role) {
  for (const RoleName& entry : kRoleNames) {
    if (entry.role == role) {
      return entry.name;
    }
  }
  return "none";
}

bool SemanticRoleFromString(std::string_view s, SemanticRole* out) {
  for (const RoleName& entry : kRoleNames) {
    if (s == entry.name) {
      *out = entry.role;
      return true;
    }
  }
  return false;
}

bool IsMeasureRole(SemanticRole role) {
  switch (role) {
    case SemanticRole::kMeasure:
    case SemanticRole::kMoney:
    case SemanticRole::kPercentage:
    case SemanticRole::kCount:
    case SemanticRole::kDuration:
    case SemanticRole::kOpen:
    case SemanticRole::kHigh:
    case SemanticRole::kLow:
    case SemanticRole::kClose:
      return true;
    default:
      return false;
  }
}

const char* SemanticFabricErrorToString(SemanticFabricError error) {
  switch (error) {
    case SemanticFabricError::kUnsupportedSchemaVersion:
      return "unsupported_schema_version";
    case SemanticFabricError::kEmptySchema:
      return "empty_schema";
    case SemanticFabricError::kInvalidFieldId:
      return "invalid_field_id";
    case SemanticFabricError::kDuplicateFieldId:
      return "duplicate_field_id";
    case SemanticFabricError::kInvalidLabel:
      return "invalid_label";
    case SemanticFabricError::kTooManyFields:
      return "too_many_fields";
    case SemanticFabricError::kMissingRoleForShape:
      return "missing_role_for_shape";
    case SemanticFabricError::kConflictingRoles:
      return "conflicting_roles";
    case SemanticFabricError::kCompositeTooDeep:
      return "composite_too_deep";
    case SemanticFabricError::kCompositePartMismatch:
      return "composite_part_mismatch";
    case SemanticFabricError::kInvalidShapeData:
      return "invalid_shape_data";
    case SemanticFabricError::kUnknownField:
      return "unknown_field";
    case SemanticFabricError::kFieldTypeMismatch:
      return "field_type_mismatch";
    case SemanticFabricError::kNonFiniteNumber:
      return "non_finite_number";
    case SemanticFabricError::kMissingRequiredField:
      return "missing_required_field";
    case SemanticFabricError::kRowLimitExceeded:
      return "row_limit_exceeded";
    case SemanticFabricError::kUnavailableFieldPresent:
      return "unavailable_field_present";
    case SemanticFabricError::kNotStreaming:
      return "not_streaming";
    case SemanticFabricError::kOutOfRangeValue:
      return "out_of_range_value";
  }
  return "invalid_shape_data";
}

}  // namespace seoul
