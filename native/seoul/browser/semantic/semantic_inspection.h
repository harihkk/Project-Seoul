// Project Seoul semantic data fabric.
// Shape-and-role queries the adaptive interface compiler reasons with. These
// are generic structural questions ("is there a temporal axis", "are these
// entities comparable", "is an open/high/low/close interval summary present"),
// never domain questions.

#ifndef SEOUL_BROWSER_SEMANTIC_SEMANTIC_INSPECTION_H_
#define SEOUL_BROWSER_SEMANTIC_SEMANTIC_INSPECTION_H_

#include <vector>

#include "seoul/browser/semantic/semantic_types.h"

namespace seoul {

// First field with `role`, or null.
const FieldSpec* FindFieldByRole(const SemanticSchema& schema,
                                 SemanticRole role);

std::vector<const FieldSpec*> MeasureFields(const SemanticSchema& schema);
std::vector<const FieldSpec*> DimensionFields(const SemanticSchema& schema);

// A kTimestamp field exists (any list shape can carry a temporal axis).
bool HasTemporalAxis(const SemanticSchema& schema);

// Open, high, low, and close roles are all present alongside a temporal
// axis: an interval-summary (candlestick) presentation is eligible.
bool OhlcEligible(const SemanticSchema& schema);

// Latitude and longitude roles are present.
bool GeoEligible(const SemanticSchema& schema);

// Row count of a list-shaped result's data (0 for non-list shapes).
size_t RowCount(const SemanticResult& result);

// Two or more rows sharing a schema with at least one measure or at least
// two non-identifier fields: a table/comparison presentation is meaningful.
bool ComparableEntities(const SemanticResult& result);

// A chart would mislead: fewer than two data points along the intended axis,
// or no measure field at all.
bool ChartWouldMislead(const SemanticResult& result);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SEMANTIC_SEMANTIC_INSPECTION_H_
