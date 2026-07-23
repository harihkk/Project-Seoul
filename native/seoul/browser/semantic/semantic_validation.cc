// Project Seoul semantic data fabric.

#include "seoul/browser/semantic/semantic_validation.h"

#include <cmath>
#include <set>
#include <utility>

namespace seoul {

SemanticViolation::SemanticViolation() = default;
SemanticViolation::SemanticViolation(const SemanticViolation&) = default;
SemanticViolation::SemanticViolation(SemanticViolation&&) = default;
SemanticViolation& SemanticViolation::operator=(const SemanticViolation&) =
    default;
SemanticViolation& SemanticViolation::operator=(SemanticViolation&&) = default;
SemanticViolation::~SemanticViolation() = default;

namespace {

base::unexpected<SemanticViolation> Violation(SemanticFabricError error,
                                              const std::string& detail) {
  SemanticViolation violation;
  violation.error = error;
  violation.detail = detail;
  return base::unexpected(violation);
}

bool ValidFieldId(const std::string& id) {
  if (id.empty() || id.size() > kMaxFieldIdLength) {
    return false;
  }
  if (id[0] < 'a' || id[0] > 'z') {
    return false;
  }
  for (char c : id) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
      return false;
    }
  }
  return true;
}

// True when the data for this shape is a list of rows.
bool IsListShape(SemanticShape shape) {
  switch (shape) {
    case SemanticShape::kEntityCollection:
    case SemanticShape::kTable:
    case SemanticShape::kCube:
    case SemanticShape::kSeries:
    case SemanticShape::kTimeSeries:
    case SemanticShape::kIntervalSeries:
    case SemanticShape::kEventStream:
    case SemanticShape::kStatusStream:
    case SemanticShape::kHierarchy:
    case SemanticShape::kGeoFeatures:
    case SemanticShape::kRoute:
    case SemanticShape::kDocumentSections:
    case SemanticShape::kCitations:
    case SemanticShape::kMedia:
    case SemanticShape::kFormSchema:
    case SemanticShape::kActionSet:
    case SemanticShape::kCodeStructure:
      return true;
    default:
      return false;
  }
}

bool IsDictShape(SemanticShape shape) {
  switch (shape) {
    case SemanticShape::kRecord:
    case SemanticShape::kDocument:
    case SemanticShape::kArtifact:
    case SemanticShape::kDiff:
      return true;
    default:
      return false;
  }
}

const FieldSpec* FindRole(const std::vector<FieldSpec>& fields,
                          SemanticRole role) {
  for (const FieldSpec& field : fields) {
    if (field.role == role) {
      return &field;
    }
  }
  return nullptr;
}

size_t CountRole(const std::vector<FieldSpec>& fields, SemanticRole role) {
  size_t count = 0;
  for (const FieldSpec& field : fields) {
    if (field.role == role) {
      ++count;
    }
  }
  return count;
}

bool HasMeasure(const std::vector<FieldSpec>& fields) {
  for (const FieldSpec& field : fields) {
    if (IsMeasureRole(field.role)) {
      return true;
    }
  }
  return false;
}

SemanticValidationResult ValidateFields(const std::vector<FieldSpec>& fields,
                                        bool allow_empty) {
  if (fields.size() > kMaxSemanticFields) {
    return Violation(SemanticFabricError::kTooManyFields, "");
  }
  if (fields.empty() && !allow_empty) {
    return Violation(SemanticFabricError::kEmptySchema, "");
  }
  std::set<std::string> seen;
  for (const FieldSpec& field : fields) {
    if (!ValidFieldId(field.id)) {
      return Violation(SemanticFabricError::kInvalidFieldId, field.id);
    }
    if (!seen.insert(field.id).second) {
      return Violation(SemanticFabricError::kDuplicateFieldId, field.id);
    }
    if (field.label.size() > kMaxFieldLabelLength) {
      return Violation(SemanticFabricError::kInvalidLabel, field.id);
    }
    if (!std::isfinite(field.confidence) || field.confidence < 0.0 ||
        field.confidence > 1.0) {
      return Violation(SemanticFabricError::kOutOfRangeValue, field.id);
    }
  }
  return base::ok();
}

SemanticValidationResult ValidateSchemaDepth(const SemanticSchema& schema,
                                             size_t depth) {
  if (depth > kMaxCompositeDepth) {
    return Violation(SemanticFabricError::kCompositeTooDeep, "");
  }
  if (schema.schema_version != kSemanticSchemaVersion) {
    return Violation(SemanticFabricError::kUnsupportedSchemaVersion, "");
  }

  if (schema.shape == SemanticShape::kComposite) {
    if (schema.parts.empty() || schema.parts.size() > kMaxCompositeParts ||
        schema.parts.size() != schema.part_names.size()) {
      return Violation(SemanticFabricError::kCompositePartMismatch, "");
    }
    std::set<std::string> names;
    for (size_t i = 0; i < schema.parts.size(); ++i) {
      if (!ValidFieldId(schema.part_names[i]) ||
          !names.insert(schema.part_names[i]).second) {
        return Violation(SemanticFabricError::kCompositePartMismatch,
                         schema.part_names[i]);
      }
      if (auto part = ValidateSchemaDepth(schema.parts[i], depth + 1);
          !part.has_value()) {
        return part;
      }
    }
    return base::ok();
  }

  if (auto fields = ValidateFields(schema.fields, /*allow_empty=*/false);
      !fields.has_value()) {
    return fields;
  }

  // Role coherence per shape. These are structural requirements a compiler
  // and renderer can rely on; they mention no domain.
  switch (schema.shape) {
    case SemanticShape::kScalar:
      if (schema.fields.size() != 1) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "scalar_needs_one_field");
      }
      break;
    case SemanticShape::kTimeSeries: {
      const size_t timestamps =
          CountRole(schema.fields, SemanticRole::kTimestamp);
      if (timestamps != 1) {
        return Violation(timestamps == 0
                             ? SemanticFabricError::kMissingRoleForShape
                             : SemanticFabricError::kConflictingRoles,
                         "timestamp");
      }
      if (!HasMeasure(schema.fields)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "measure");
      }
      break;
    }
    case SemanticShape::kIntervalSeries:
      if (!FindRole(schema.fields, SemanticRole::kIntervalStart) ||
          !FindRole(schema.fields, SemanticRole::kIntervalEnd)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "interval_start_end");
      }
      break;
    case SemanticShape::kEventStream:
    case SemanticShape::kStatusStream:
      if (!FindRole(schema.fields, SemanticRole::kTimestamp)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "timestamp");
      }
      break;
    case SemanticShape::kHierarchy:
      if (!FindRole(schema.fields, SemanticRole::kIdentifier) ||
          !FindRole(schema.fields, SemanticRole::kParentReference)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "identifier_parent");
      }
      break;
    case SemanticShape::kGraph: {
      if (!FindRole(schema.fields, SemanticRole::kIdentifier)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "node_identifier");
      }
      if (auto edges = ValidateFields(schema.edge_fields,
                                      /*allow_empty=*/false);
          !edges.has_value()) {
        return edges;
      }
      if (!FindRole(schema.edge_fields, SemanticRole::kSourceNode) ||
          !FindRole(schema.edge_fields, SemanticRole::kTargetNode)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "edge_source_target");
      }
      break;
    }
    case SemanticShape::kGeoFeatures:
    case SemanticShape::kRoute:
      if (!FindRole(schema.fields, SemanticRole::kLatitude) ||
          !FindRole(schema.fields, SemanticRole::kLongitude)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "latitude_longitude");
      }
      break;
    case SemanticShape::kCube: {
      size_t dimensions = 0;
      for (const FieldSpec& field : schema.fields) {
        if (field.role == SemanticRole::kDimension ||
            field.role == SemanticRole::kCategory) {
          ++dimensions;
        }
      }
      if (dimensions < 2 || !HasMeasure(schema.fields)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "two_dimensions_and_measure");
      }
      break;
    }
    case SemanticShape::kFormSchema:
      if (!FindRole(schema.fields, SemanticRole::kIdentifier) ||
          !FindRole(schema.fields, SemanticRole::kName)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "form_identifier_name");
      }
      break;
    case SemanticShape::kCitations:
      if (!FindRole(schema.fields, SemanticRole::kUrl)) {
        return Violation(SemanticFabricError::kMissingRoleForShape, "url");
      }
      break;
    case SemanticShape::kMedia:
      if (!FindRole(schema.fields, SemanticRole::kMediaUrl)) {
        return Violation(SemanticFabricError::kMissingRoleForShape,
                         "media_url");
      }
      break;
    case SemanticShape::kDocument:
    case SemanticShape::kDiff:
      if (!FindRole(schema.fields, SemanticRole::kBody)) {
        return Violation(SemanticFabricError::kMissingRoleForShape, "body");
      }
      break;
    default:
      break;
  }
  return base::ok();
}

SemanticValidationResult ValidateValueForField(const FieldSpec& field,
                                               const base::Value& value) {
  switch (field.primitive) {
    case FieldPrimitive::kString:
      if (!value.is_string()) {
        return Violation(SemanticFabricError::kFieldTypeMismatch, field.id);
      }
      return base::ok();
    case FieldPrimitive::kInteger:
      if (!value.is_int()) {
        return Violation(SemanticFabricError::kFieldTypeMismatch, field.id);
      }
      return base::ok();
    case FieldPrimitive::kNumber:
    case FieldPrimitive::kTimestamp: {
      if (!value.is_double() && !value.is_int()) {
        return Violation(SemanticFabricError::kFieldTypeMismatch, field.id);
      }
      if (!std::isfinite(value.GetDouble())) {
        return Violation(SemanticFabricError::kNonFiniteNumber, field.id);
      }
      return base::ok();
    }
    case FieldPrimitive::kBoolean:
      if (!value.is_bool()) {
        return Violation(SemanticFabricError::kFieldTypeMismatch, field.id);
      }
      return base::ok();
  }
  return Violation(SemanticFabricError::kFieldTypeMismatch, field.id);
}

SemanticValidationResult ValidateRow(
    const std::vector<FieldSpec>& fields,
    const std::set<std::string>& unavailable,
    const base::DictValue& row) {
  for (const auto [key, value] : row) {
    const FieldSpec* declared = nullptr;
    for (const FieldSpec& field : fields) {
      if (field.id == key) {
        declared = &field;
        break;
      }
    }
    if (!declared) {
      return Violation(SemanticFabricError::kUnknownField, key);
    }
    if (unavailable.find(key) != unavailable.end()) {
      // The source declared it could not supply this field; carrying a value
      // anyway would be fabrication.
      return Violation(SemanticFabricError::kUnavailableFieldPresent, key);
    }
    if (value.is_none()) {
      if (!declared->nullable) {
        return Violation(SemanticFabricError::kMissingRequiredField, key);
      }
      continue;
    }
    if (auto valid = ValidateValueForField(*declared, value);
        !valid.has_value()) {
      return valid;
    }
  }
  for (const FieldSpec& field : fields) {
    if (!field.nullable && !row.contains(field.id) &&
        unavailable.find(field.id) == unavailable.end()) {
      return Violation(SemanticFabricError::kMissingRequiredField, field.id);
    }
  }
  return base::ok();
}

SemanticValidationResult ValidateDataForSchema(
    const SemanticSchema& schema,
    const std::set<std::string>& unavailable,
    const base::Value& data,
    size_t depth) {
  if (schema.shape == SemanticShape::kComposite) {
    const base::DictValue* dict = data.GetIfDict();
    if (!dict) {
      return Violation(SemanticFabricError::kInvalidShapeData, "composite");
    }
    for (size_t i = 0; i < schema.parts.size(); ++i) {
      const base::Value* part = dict->Find(schema.part_names[i]);
      if (!part) {
        return Violation(SemanticFabricError::kCompositePartMismatch,
                         schema.part_names[i]);
      }
      if (auto valid = ValidateDataForSchema(schema.parts[i], unavailable,
                                             *part, depth + 1);
          !valid.has_value()) {
        return valid;
      }
    }
    return base::ok();
  }
  if (schema.shape == SemanticShape::kScalar) {
    return ValidateValueForField(schema.fields[0], data);
  }
  if (IsDictShape(schema.shape)) {
    const base::DictValue* dict = data.GetIfDict();
    if (!dict) {
      return Violation(SemanticFabricError::kInvalidShapeData, "record");
    }
    return ValidateRow(schema.fields, unavailable, *dict);
  }
  if (schema.shape == SemanticShape::kGraph) {
    const base::DictValue* dict = data.GetIfDict();
    if (!dict) {
      return Violation(SemanticFabricError::kInvalidShapeData, "graph");
    }
    const base::ListValue* nodes = dict->FindList("nodes");
    const base::ListValue* edges = dict->FindList("edges");
    if (!nodes || !edges) {
      return Violation(SemanticFabricError::kInvalidShapeData,
                       "graph_nodes_edges");
    }
    if (nodes->size() > kMaxSemanticRows ||
        edges->size() > kMaxSemanticRows) {
      return Violation(SemanticFabricError::kRowLimitExceeded, "graph");
    }
    for (const base::Value& node : *nodes) {
      const base::DictValue* row = node.GetIfDict();
      if (!row) {
        return Violation(SemanticFabricError::kInvalidShapeData, "node");
      }
      if (auto valid = ValidateRow(schema.fields, unavailable, *row);
          !valid.has_value()) {
        return valid;
      }
    }
    for (const base::Value& edge : *edges) {
      const base::DictValue* row = edge.GetIfDict();
      if (!row) {
        return Violation(SemanticFabricError::kInvalidShapeData, "edge");
      }
      if (auto valid = ValidateRow(schema.edge_fields, unavailable, *row);
          !valid.has_value()) {
        return valid;
      }
    }
    return base::ok();
  }
  if (IsListShape(schema.shape)) {
    const base::ListValue* list = data.GetIfList();
    if (!list) {
      return Violation(SemanticFabricError::kInvalidShapeData, "list");
    }
    if (list->size() > kMaxSemanticRows) {
      return Violation(SemanticFabricError::kRowLimitExceeded, "");
    }
    for (const base::Value& row_value : *list) {
      const base::DictValue* row = row_value.GetIfDict();
      if (!row) {
        return Violation(SemanticFabricError::kInvalidShapeData, "row");
      }
      if (auto valid = ValidateRow(schema.fields, unavailable, *row);
          !valid.has_value()) {
        return valid;
      }
    }
    return base::ok();
  }
  return Violation(SemanticFabricError::kInvalidShapeData, "shape");
}

}  // namespace

SemanticValidationResult ValidateSemanticSchema(
    const SemanticSchema& schema) {
  return ValidateSchemaDepth(schema, /*depth=*/1);
}

SemanticValidationResult ValidateSemanticResult(
    const SemanticResult& result) {
  if (auto schema = ValidateSemanticSchema(result.schema);
      !schema.has_value()) {
    return schema;
  }
  std::set<std::string> unavailable(result.unavailable_field_ids.begin(),
                                    result.unavailable_field_ids.end());
  return ValidateDataForSchema(result.schema, unavailable, result.data,
                               /*depth=*/1);
}

SemanticValidationResult MergeStreamingRows(SemanticResult& result,
                                            const base::ListValue& rows) {
  if (result.state != ResultState::kStreaming &&
      result.state != ResultState::kPartial) {
    return Violation(SemanticFabricError::kNotStreaming, std::string());
  }
  base::ListValue* existing = result.data.GetIfList();
  if (!existing) {
    return Violation(SemanticFabricError::kInvalidShapeData, "list");
  }
  if (existing->size() + rows.size() > kMaxSemanticRows) {
    return Violation(SemanticFabricError::kRowLimitExceeded, std::string());
  }
  // Validate all incoming rows before appending any (atomic merge).
  std::set<std::string> unavailable(result.unavailable_field_ids.begin(),
                                    result.unavailable_field_ids.end());
  base::ListValue candidate = existing->Clone();
  for (const base::Value& row : rows) {
    candidate.Append(row.Clone());
  }
  base::Value candidate_value{std::move(candidate)};
  if (auto valid = ValidateDataForSchema(result.schema, unavailable,
                                         candidate_value, /*depth=*/1);
      !valid.has_value()) {
    return valid;
  }
  result.data = std::move(candidate_value);
  return base::ok();
}

}  // namespace seoul
