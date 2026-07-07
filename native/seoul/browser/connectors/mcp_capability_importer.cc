// Project Seoul connected tools.

#include "seoul/browser/connectors/mcp_capability_importer.h"

#include <set>
#include <utility>

#include "seoul/browser/tools/tool_schema_from_json.h"

namespace seoul {

namespace {

base::unexpected<McpImportFailure> Failure(McpImportError error,
                                           const std::string& detail) {
  McpImportFailure failure;
  failure.error = error;
  failure.detail = detail;
  return base::unexpected(failure);
}

bool SanitizeToolName(const std::string& name, std::string* sanitized) {
  std::string out;
  for (char c : name) {
    if (c >= 'A' && c <= 'Z') {
      out.push_back(static_cast<char>(c - 'A' + 'a'));
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out.push_back(c);
    } else if (c == '_' || c == '-' || c == '.' || c == '/') {
      out.push_back('_');
    } else {
      return false;
    }
  }
  if (out.empty() || out.size() > 64 || out[0] < 'a' || out[0] > 'z') {
    return false;
  }
  *sanitized = std::move(out);
  return true;
}

}  // namespace

McpImportResult ImportMcpCapabilities(const base::DictValue& tools_list,
                                      const std::string& provider) {
  const base::ListValue* tools = tools_list.FindList("tools");
  if (!tools) {
    return Failure(McpImportError::kNotAToolList, "");
  }
  if (tools->empty()) {
    return Failure(McpImportError::kNoTools, "");
  }
  if (tools->size() > kMaxMcpTools) {
    return Failure(McpImportError::kTooManyTools, "");
  }

  std::vector<ToolDescriptor> descriptors;
  std::set<std::string> seen;
  for (const base::Value& tool_value : *tools) {
    const base::DictValue* tool = tool_value.GetIfDict();
    if (!tool) {
      return Failure(McpImportError::kNotAToolList, "");
    }
    const std::string* name = tool->FindString("name");
    if (!name) {
      return Failure(McpImportError::kMissingName, "");
    }
    std::string sanitized;
    if (!SanitizeToolName(*name, &sanitized)) {
      return Failure(McpImportError::kInvalidName, *name);
    }
    if (!seen.insert(sanitized).second) {
      return Failure(McpImportError::kDuplicateName, sanitized);
    }
    const std::string* description = tool->FindString("description");
    if (!description || description->empty()) {
      return Failure(McpImportError::kMissingDescription, sanitized);
    }

    ToolDescriptor descriptor;
    descriptor.id =
        ToolId::FromString("connector." + provider + "." + sanitized);
    if (!descriptor.id.is_valid()) {
      return Failure(McpImportError::kInvalidName, sanitized);
    }
    descriptor.provider = provider;
    descriptor.name = *name;
    descriptor.description = *description;
    descriptor.requires_network = true;
    descriptor.sensitivity = DataSensitivity::kPersonal;
    descriptor.observation_contract = "MCP tools/call result";

    // MCP annotations: only an explicit readOnlyHint downgrades risk.
    const bool read_only =
        tool->FindBoolByDottedPath("annotations.readOnlyHint")
            .value_or(false);
    if (read_only) {
      descriptor.risk = RiskCategory::kReadOnly;
      descriptor.approval = ApprovalPolicy::kNeverRequired;
      descriptor.idempotency = IdempotencyClass::kIdempotent;
    } else {
      descriptor.risk = RiskCategory::kExternalSideEffect;
      descriptor.approval = ApprovalPolicy::kAlwaysRequired;
    }

    if (const base::DictValue* input_schema =
            tool->FindDict("inputSchema")) {
      auto imported = ToolSchemaFromJsonSchema(*input_schema);
      if (!imported.has_value()) {
        return Failure(McpImportError::kSchemaImportFailed, sanitized);
      }
      descriptor.input_schema = std::move(imported.value());
    }
    descriptors.push_back(std::move(descriptor));
  }
  return descriptors;
}

const char* McpImportErrorToString(McpImportError error) {
  switch (error) {
    case McpImportError::kNotAToolList:
      return "not_a_tool_list";
    case McpImportError::kNoTools:
      return "no_tools";
    case McpImportError::kTooManyTools:
      return "too_many_tools";
    case McpImportError::kMissingName:
      return "missing_name";
    case McpImportError::kInvalidName:
      return "invalid_name";
    case McpImportError::kDuplicateName:
      return "duplicate_name";
    case McpImportError::kMissingDescription:
      return "missing_description";
    case McpImportError::kSchemaImportFailed:
      return "schema_import_failed";
  }
  return "not_a_tool_list";
}

}  // namespace seoul
