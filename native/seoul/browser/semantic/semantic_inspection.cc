// Project Seoul semantic data fabric.

#include "seoul/browser/semantic/semantic_inspection.h"

namespace seoul {

const FieldSpec* FindFieldByRole(const SemanticSchema& schema,
                                 SemanticRole role) {
  for (const FieldSpec& field : schema.fields) {
    if (field.role == role) {
      return &field;
    }
  }
  return nullptr;
}

std::vector<const FieldSpec*> MeasureFields(const SemanticSchema& schema) {
  std::vector<const FieldSpec*> measures;
  for (const FieldSpec& field : schema.fields) {
    if (IsMeasureRole(field.role)) {
      measures.push_back(&field);
    }
  }
  return measures;
}

std::vector<const FieldSpec*> DimensionFields(const SemanticSchema& schema) {
  std::vector<const FieldSpec*> dimensions;
  for (const FieldSpec& field : schema.fields) {
    if (field.role == SemanticRole::kDimension ||
        field.role == SemanticRole::kCategory) {
      dimensions.push_back(&field);
    }
  }
  return dimensions;
}

bool HasTemporalAxis(const SemanticSchema& schema) {
  return FindFieldByRole(schema, SemanticRole::kTimestamp) != nullptr;
}

bool OhlcEligible(const SemanticSchema& schema) {
  return HasTemporalAxis(schema) &&
         FindFieldByRole(schema, SemanticRole::kOpen) &&
         FindFieldByRole(schema, SemanticRole::kHigh) &&
         FindFieldByRole(schema, SemanticRole::kLow) &&
         FindFieldByRole(schema, SemanticRole::kClose);
}

bool GeoEligible(const SemanticSchema& schema) {
  return FindFieldByRole(schema, SemanticRole::kLatitude) &&
         FindFieldByRole(schema, SemanticRole::kLongitude);
}

size_t RowCount(const SemanticResult& result) {
  const base::ListValue* list = result.data.GetIfList();
  return list ? list->size() : 0;
}

bool ComparableEntities(const SemanticResult& result) {
  if (result.schema.shape != SemanticShape::kEntityCollection &&
      result.schema.shape != SemanticShape::kTable &&
      result.schema.shape != SemanticShape::kCube) {
    return false;
  }
  if (RowCount(result) < 2) {
    return false;
  }
  if (!MeasureFields(result.schema).empty()) {
    return true;
  }
  size_t non_identifier_fields = 0;
  for (const FieldSpec& field : result.schema.fields) {
    if (field.role != SemanticRole::kIdentifier) {
      ++non_identifier_fields;
    }
  }
  return non_identifier_fields >= 2;
}

bool ChartWouldMislead(const SemanticResult& result) {
  if (MeasureFields(result.schema).empty()) {
    return true;  // nothing quantitative to plot
  }
  // A single point along the axis is a number, not a trend.
  return RowCount(result) < 2;
}

}  // namespace seoul
