// Project Seoul general-purpose operator: tool layer.
// Typed input/output schemas for tools and validation of untrusted argument
// dictionaries against them. Planner-produced arguments are validated here
// before any tool runs; unknown fields, wrong kinds, out-of-range values, and
// non-http(s) URLs are rejected with a precise violation.

#ifndef SEOUL_BROWSER_TOOLS_TOOL_SCHEMA_H_
#define SEOUL_BROWSER_TOOLS_TOOL_SCHEMA_H_

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"

namespace seoul {

inline constexpr size_t kMaxSchemaFields = 64;
inline constexpr size_t kMaxSchemaDepth = 4;
inline constexpr size_t kMaxSchemaEnumValues = 64;
inline constexpr size_t kMaxArgStringLength = 16384;
inline constexpr size_t kMaxArgListItems = 256;

enum class SchemaFieldKind {
  kString,
  kInteger,
  kNumber,
  kBoolean,
  kEnum,    // string constrained to enum_values
  kUrl,     // http(s) URL string
  kList,    // items follow children[0]
  kObject,  // fields follow children
};

struct SchemaField {
  SchemaField();
  SchemaField(const SchemaField&);
  SchemaField(SchemaField&&);
  SchemaField& operator=(const SchemaField&);
  SchemaField& operator=(SchemaField&&);
  ~SchemaField();

  std::string name;
  SchemaFieldKind kind = SchemaFieldKind::kString;
  bool required = false;
  std::string description;
  std::vector<std::string> enum_values;  // kEnum
  double min_value = 0.0;                // kInteger/kNumber when has_range
  double max_value = 0.0;
  bool has_range = false;
  size_t max_length = kMaxArgStringLength;  // kString/kUrl
  // kObject: the object's fields. kList: exactly one element describing the
  // item shape (name it "item"; violation paths use the index instead).
  std::vector<SchemaField> children;

  // Structural equality (recursive through children); WorkflowParam and tests
  // compare schemas by value.
  friend bool operator==(const SchemaField&, const SchemaField&) = default;
};

struct ToolSchema {
  ToolSchema();
  ToolSchema(const ToolSchema&);
  ToolSchema(ToolSchema&&);
  ToolSchema& operator=(const ToolSchema&);
  ToolSchema& operator=(ToolSchema&&);
  ~ToolSchema();

  std::vector<SchemaField> fields;
};

enum class SchemaViolationKind {
  kMalformedSchema,
  kUnknownField,
  kMissingRequiredField,
  kWrongKind,
  kOutOfRange,
  kTooLong,
  kNotInEnum,
  kInvalidUrl,
  kTooManyItems,
  kDepthExceeded,
};

struct SchemaViolation {
  SchemaViolation();
  SchemaViolation(const SchemaViolation&);
  SchemaViolation(SchemaViolation&&);
  SchemaViolation& operator=(const SchemaViolation&);
  SchemaViolation& operator=(SchemaViolation&&);
  ~SchemaViolation();

  SchemaViolationKind kind = SchemaViolationKind::kMalformedSchema;
  std::string field_path;  // "query", "filters.max_price", "items[3]"

  friend bool operator==(const SchemaViolation&,
                         const SchemaViolation&) = default;
};

const char* SchemaViolationKindToString(SchemaViolationKind kind);

using SchemaValidationResult = base::expected<void, SchemaViolation>;

// True when the schema itself is well-formed (bounded, named, valid nesting).
// Registries reject descriptors whose schemas fail this.
bool IsWellFormedSchema(const ToolSchema& schema);

// Validates `args` against `schema`. Every arg key must be declared; every
// required field must be present and well-typed.
SchemaValidationResult ValidateArgs(const ToolSchema& schema,
                                    const base::DictValue& args);

}  // namespace seoul

#endif  // SEOUL_BROWSER_TOOLS_TOOL_SCHEMA_H_
