// Project Seoul semantic data fabric.
// Schema and result validation plus streaming merge. Validation is entirely
// shape-and-role driven: it enforces that a schema's roles are coherent for
// its shape and that data conforms to the schema, for schemas Seoul has never
// seen before. Nothing here knows any business domain.

#ifndef SEOUL_BROWSER_SEMANTIC_SEMANTIC_VALIDATION_H_
#define SEOUL_BROWSER_SEMANTIC_SEMANTIC_VALIDATION_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/semantic/semantic_types.h"

namespace seoul {

enum class SemanticFabricError {
  kUnsupportedSchemaVersion,
  kEmptySchema,
  kInvalidFieldId,
  kDuplicateFieldId,
  kInvalidLabel,
  kTooManyFields,
  kMissingRoleForShape,   // shape requires a role the schema lacks
  kConflictingRoles,      // for example two kTimestamp fields on a time series
  kCompositeTooDeep,
  kCompositePartMismatch,
  kInvalidShapeData,      // data has the wrong top-level structure
  kUnknownField,          // data carries a key the schema does not declare
  kFieldTypeMismatch,
  kNonFiniteNumber,
  kMissingRequiredField,  // non-nullable field absent from a row
  kRowLimitExceeded,
  kUnavailableFieldPresent,  // declared-unavailable field appears in data
  kNotStreaming,
  kOutOfRangeValue,
};

const char* SemanticFabricErrorToString(SemanticFabricError error);

struct SemanticViolation {
  SemanticFabricError error = SemanticFabricError::kInvalidShapeData;
  std::string detail;  // field id, part name, or row index text

  friend bool operator==(const SemanticViolation&,
                         const SemanticViolation&) = default;
};

using SemanticValidationResult = base::expected<void, SemanticViolation>;

// Structural and role coherence of a schema, for any shape.
SemanticValidationResult ValidateSemanticSchema(const SemanticSchema& schema);

// Full result validation: schema validity, shape-conformant data, per-field
// primitive checks, no undeclared keys, no fabricated unavailable fields,
// bounded rows, finite numbers.
SemanticValidationResult ValidateSemanticResult(const SemanticResult& result);

// Appends rows to a streaming/partial list-shaped result. Rows are validated
// against the schema first; the merge is atomic (no partial append).
SemanticValidationResult MergeStreamingRows(SemanticResult& result,
                                            const base::Value::List& rows);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SEMANTIC_SEMANTIC_VALIDATION_H_
