// Project Seoul general-purpose operator: tool layer.
// Conversion from a bounded JSON Schema subset (the shape used by OpenAPI
// operations and MCP tool definitions) into a typed ToolSchema. Supported:
// type object with properties/required, string (plus enum and uri format),
// integer, number, boolean, array with typed items, and nested objects up to
// the ToolSchema depth bound. Anything else is rejected precisely, never
// guessed.

#ifndef SEOUL_BROWSER_TOOLS_TOOL_SCHEMA_FROM_JSON_H_
#define SEOUL_BROWSER_TOOLS_TOOL_SCHEMA_FROM_JSON_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/tools/tool_schema.h"

namespace seoul {

enum class JsonSchemaImportError {
  kNotAnObjectSchema,
  kUnsupportedType,
  kUnsupportedKeyword,
  kInvalidPropertyName,
  kTooManyProperties,
  kDepthExceeded,
  kInvalidEnum,
  kMalformedSchema,
};

const char* JsonSchemaImportErrorToString(JsonSchemaImportError error);

struct JsonSchemaImportFailure {
  JsonSchemaImportError error = JsonSchemaImportError::kMalformedSchema;
  std::string path;  // "properties.query.items"

  friend bool operator==(const JsonSchemaImportFailure&,
                         const JsonSchemaImportFailure&) = default;
};

using JsonSchemaImportResult =
    base::expected<ToolSchema, JsonSchemaImportFailure>;

// Converts a JSON Schema object ({"type":"object","properties":{...},
// "required":[...]}) into a ToolSchema. The result always satisfies
// IsWellFormedSchema.
JsonSchemaImportResult ToolSchemaFromJsonSchema(
    const base::DictValue& json_schema);

}  // namespace seoul

#endif  // SEOUL_BROWSER_TOOLS_TOOL_SCHEMA_FROM_JSON_H_
