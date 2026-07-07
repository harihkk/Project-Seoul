// Project Seoul connected tools.
// Imports reviewed OpenAPI operations as typed capabilities. The input is an
// already-fetched, user-reviewed OpenAPI document (base::Value); the importer
// converts each operation into a CapabilityDescriptor under
// connector.<provider>.* with schema-typed inputs and honest risk defaults
// (GET is read-only; anything else is an external side effect requiring
// approval). The importer performs no network I/O.

#ifndef SEOUL_BROWSER_CONNECTORS_OPENAPI_CAPABILITY_IMPORTER_H_
#define SEOUL_BROWSER_CONNECTORS_OPENAPI_CAPABILITY_IMPORTER_H_

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

inline constexpr size_t kMaxOpenApiOperations = 128;

enum class OpenApiImportError {
  kNotAnOpenApiDocument,
  kNoOperations,
  kTooManyOperations,
  kMissingOperationId,
  kDuplicateOperationId,
  kInvalidOperationId,
  kUnsupportedParameterLocation,
  kSchemaImportFailed,
};

const char* OpenApiImportErrorToString(OpenApiImportError error);

struct OpenApiImportFailure {
  OpenApiImportError error = OpenApiImportError::kNotAnOpenApiDocument;
  std::string detail;  // operation id or path

  friend bool operator==(const OpenApiImportFailure&,
                         const OpenApiImportFailure&) = default;
};

// One imported operation: the capability descriptor plus the HTTP binding the
// runtime dispatcher needs to execute it.
struct OpenApiOperationBinding {
  ToolDescriptor descriptor;
  std::string http_method;    // lowercase: "get", "post", ...
  std::string path_template;  // "/assets/{asset_id}"
};

using OpenApiImportResult =
    base::expected<std::vector<OpenApiOperationBinding>,
                   OpenApiImportFailure>;

// Imports every operation from `document` for connector `provider`. Only
// operations with an operationId and JSON-typed parameters import; the whole
// import fails precisely on the first unsupported construct rather than
// silently skipping it.
OpenApiImportResult ImportOpenApiCapabilities(
    const base::DictValue& document,
    const std::string& provider);

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_OPENAPI_CAPABILITY_IMPORTER_H_
