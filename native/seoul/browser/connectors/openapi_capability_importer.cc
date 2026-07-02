// Project Seoul connected tools.

#include "seoul/browser/connectors/openapi_capability_importer.h"

#include <set>
#include <utility>

#include "base/strings/string_util.h"
#include "seoul/browser/tools/tool_schema_from_json.h"

namespace seoul {

namespace {

base::unexpected<OpenApiImportFailure> Failure(OpenApiImportError error,
                                               const std::string& detail) {
  OpenApiImportFailure failure;
  failure.error = error;
  failure.detail = detail;
  return base::unexpected(failure);
}

// operationId sanitized to a tool-id segment: lowercase, [a-z0-9_],
// leading letter.
bool SanitizeOperationId(const std::string& operation_id,
                         std::string* sanitized) {
  std::string out;
  for (char c : operation_id) {
    if (c >= 'A' && c <= 'Z') {
      out.push_back(static_cast<char>(c - 'A' + 'a'));
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
      out.push_back(c);
    } else if (c == '_' || c == '-' || c == '.') {
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

constexpr const char* kSupportedMethods[] = {"get", "post", "put", "patch",
                                             "delete"};

bool IsSupportedMethod(const std::string& method) {
  for (const char* supported : kSupportedMethods) {
    if (method == supported) {
      return true;
    }
  }
  return false;
}

}  // namespace

OpenApiImportResult ImportOpenApiCapabilities(
    const base::Value::Dict& document,
    const std::string& provider) {
  // Accept OpenAPI 3.x ("openapi") documents with a paths object.
  const std::string* version = document.FindString("openapi");
  const base::Value::Dict* paths = document.FindDict("paths");
  if (!version || !paths) {
    return Failure(OpenApiImportError::kNotAnOpenApiDocument, "");
  }

  std::vector<OpenApiOperationBinding> bindings;
  std::set<std::string> seen_ids;
  for (const auto [path_template, path_value] : *paths) {
    const base::Value::Dict* path_item = path_value.GetIfDict();
    if (!path_item) {
      continue;
    }
    for (const auto [method, operation_value] : *path_item) {
      if (!IsSupportedMethod(method)) {
        continue;  // parameters/summary keys and unsupported verbs
      }
      const base::Value::Dict* operation = operation_value.GetIfDict();
      if (!operation) {
        continue;
      }
      if (bindings.size() >= kMaxOpenApiOperations) {
        return Failure(OpenApiImportError::kTooManyOperations,
                       path_template);
      }
      const std::string* operation_id = operation->FindString("operationId");
      if (!operation_id) {
        return Failure(OpenApiImportError::kMissingOperationId,
                       path_template);
      }
      std::string sanitized;
      if (!SanitizeOperationId(*operation_id, &sanitized)) {
        return Failure(OpenApiImportError::kInvalidOperationId,
                       *operation_id);
      }
      if (!seen_ids.insert(sanitized).second) {
        return Failure(OpenApiImportError::kDuplicateOperationId, sanitized);
      }

      OpenApiOperationBinding binding;
      binding.http_method = method;
      binding.path_template = path_template;

      ToolDescriptor& descriptor = binding.descriptor;
      descriptor.id =
          ToolId::FromString("connector." + provider + "." + sanitized);
      if (!descriptor.id.is_valid()) {
        return Failure(OpenApiImportError::kInvalidOperationId, sanitized);
      }
      descriptor.provider = provider;
      const std::string* summary = operation->FindString("summary");
      const std::string* description = operation->FindString("description");
      descriptor.name = summary ? *summary : sanitized;
      descriptor.description = description ? *description
                                           : descriptor.name;
      descriptor.requires_network = true;
      descriptor.sensitivity = DataSensitivity::kPersonal;
      if (method == "get") {
        descriptor.risk = RiskCategory::kReadOnly;
        descriptor.approval = ApprovalPolicy::kNeverRequired;
        descriptor.idempotency = IdempotencyClass::kIdempotent;
      } else {
        // A remote mutation leaves the browser; it is never silently safe.
        descriptor.risk = RiskCategory::kExternalSideEffect;
        descriptor.approval = ApprovalPolicy::kAlwaysRequired;
        descriptor.idempotency = method == "put"
                                     ? IdempotencyClass::kIdempotent
                                     : IdempotencyClass::kNotIdempotent;
      }
      descriptor.observation_contract =
          "HTTP response from " + base::ToUpperASCII(method) + " " +
          path_template;

      // Query/path parameters become schema fields.
      if (const base::Value::List* parameters =
              operation->FindList("parameters")) {
        for (const base::Value& parameter_value : *parameters) {
          const base::Value::Dict* parameter = parameter_value.GetIfDict();
          if (!parameter) {
            continue;
          }
          const std::string* in = parameter->FindString("in");
          const std::string* name = parameter->FindString("name");
          const base::Value::Dict* schema = parameter->FindDict("schema");
          if (!in || !name || !schema) {
            return Failure(OpenApiImportError::kSchemaImportFailed,
                           sanitized);
          }
          if (*in != "query" && *in != "path") {
            return Failure(
                OpenApiImportError::kUnsupportedParameterLocation,
                sanitized + ":" + *in);
          }
          base::Value::Dict object_schema;
          object_schema.Set("type", "object");
          base::Value::Dict properties;
          properties.Set(*name, schema->Clone());
          object_schema.Set("properties", std::move(properties));
          if (parameter->FindBool("required").value_or(*in == "path")) {
            base::Value::List required;
            required.Append(*name);
            object_schema.Set("required", std::move(required));
          }
          auto imported = ToolSchemaFromJsonSchema(object_schema);
          if (!imported.has_value()) {
            return Failure(OpenApiImportError::kSchemaImportFailed,
                           sanitized + ":" + *name);
          }
          for (SchemaField& field : imported->fields) {
            descriptor.input_schema.fields.push_back(std::move(field));
          }
        }
      }

      // A JSON request body merges its object properties into the input.
      const base::Value::Dict* body_schema = operation->FindDictByDottedPath(
          "requestBody.content.application/json.schema");
      if (body_schema) {
        auto imported = ToolSchemaFromJsonSchema(*body_schema);
        if (!imported.has_value()) {
          return Failure(OpenApiImportError::kSchemaImportFailed, sanitized);
        }
        for (SchemaField& field : imported->fields) {
          descriptor.input_schema.fields.push_back(std::move(field));
        }
      }
      if (!IsWellFormedSchema(descriptor.input_schema)) {
        return Failure(OpenApiImportError::kSchemaImportFailed, sanitized);
      }
      bindings.push_back(std::move(binding));
    }
  }
  if (bindings.empty()) {
    return Failure(OpenApiImportError::kNoOperations, "");
  }
  return bindings;
}

const char* OpenApiImportErrorToString(OpenApiImportError error) {
  switch (error) {
    case OpenApiImportError::kNotAnOpenApiDocument:
      return "not_an_openapi_document";
    case OpenApiImportError::kNoOperations:
      return "no_operations";
    case OpenApiImportError::kTooManyOperations:
      return "too_many_operations";
    case OpenApiImportError::kMissingOperationId:
      return "missing_operation_id";
    case OpenApiImportError::kDuplicateOperationId:
      return "duplicate_operation_id";
    case OpenApiImportError::kInvalidOperationId:
      return "invalid_operation_id";
    case OpenApiImportError::kUnsupportedParameterLocation:
      return "unsupported_parameter_location";
    case OpenApiImportError::kSchemaImportFailed:
      return "schema_import_failed";
  }
  return "not_an_openapi_document";
}

}  // namespace seoul
