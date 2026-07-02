// Project Seoul connected tools.
// Imports reviewed MCP tool definitions as typed capabilities. The input is
// the already-fetched tools list from an MCP server's tools/list response;
// each definition (name, description, inputSchema, optional annotations)
// becomes a CapabilityDescriptor under connector.<provider>.*. MCP is one
// supported connector protocol, not a dependency of the core: the importer is
// pure, and transport lives behind McpTransport.

#ifndef SEOUL_BROWSER_CONNECTORS_MCP_CAPABILITY_IMPORTER_H_
#define SEOUL_BROWSER_CONNECTORS_MCP_CAPABILITY_IMPORTER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

inline constexpr size_t kMaxMcpTools = 128;

enum class McpImportError {
  kNotAToolList,
  kNoTools,
  kTooManyTools,
  kMissingName,
  kInvalidName,
  kDuplicateName,
  kMissingDescription,
  kSchemaImportFailed,
};

const char* McpImportErrorToString(McpImportError error);

struct McpImportFailure {
  McpImportError error = McpImportError::kNotAToolList;
  std::string detail;

  friend bool operator==(const McpImportFailure&,
                         const McpImportFailure&) = default;
};

using McpImportResult =
    base::expected<std::vector<ToolDescriptor>, McpImportFailure>;

// Imports the tools array of a tools/list result ({"tools": [...]}) for
// connector `provider`. Tools with an explicit readOnlyHint annotation import
// as read-only; everything else defaults to an approval-gated external side
// effect (never silently safe).
McpImportResult ImportMcpCapabilities(const base::Value::Dict& tools_list,
                                      const std::string& provider);

// JSON-RPC transport seam for a reviewed MCP connection. Implementations
// (stdio pipe, local socket) live in platform layers; tests use a fake. The
// transport never interprets tool semantics.
class McpTransport {
 public:
  virtual ~McpTransport() = default;

  using ResponseCallback = base::OnceCallback<void(
      base::expected<base::Value, std::string> result)>;
  virtual void SendRequest(const std::string& method,
                           base::Value::Dict params,
                           ResponseCallback callback) = 0;
  virtual void Cancel() = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_MCP_CAPABILITY_IMPORTER_H_
