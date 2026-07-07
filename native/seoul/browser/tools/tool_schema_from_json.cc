// Project Seoul general-purpose operator: tool layer.

#include "seoul/browser/tools/tool_schema_from_json.h"

#include <utility>

namespace seoul {

namespace {

base::unexpected<JsonSchemaImportFailure> Failure(
    JsonSchemaImportError error,
    const std::string& path) {
  JsonSchemaImportFailure failure;
  failure.error = error;
  failure.path = path;
  return base::unexpected(failure);
}

bool ValidPropertyName(const std::string& name) {
  if (name.empty() || name.size() > 64) {
    return false;
  }
  if (name[0] < 'a' || name[0] > 'z') {
    return false;
  }
  for (char c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
      return false;
    }
  }
  return true;
}

using FieldResult = base::expected<SchemaField, JsonSchemaImportFailure>;

FieldResult FieldFromSchema(const std::string& name,
                            const base::DictValue& schema,
                            const std::string& path,
                            size_t depth);

base::expected<std::vector<SchemaField>, JsonSchemaImportFailure>
FieldsFromObjectSchema(const base::DictValue& schema,
                       const std::string& path,
                       size_t depth) {
  if (depth > kMaxSchemaDepth) {
    return Failure(JsonSchemaImportError::kDepthExceeded, path);
  }
  std::vector<SchemaField> fields;
  const base::DictValue* properties = schema.FindDict("properties");
  if (properties) {
    if (properties->size() > kMaxSchemaFields) {
      return Failure(JsonSchemaImportError::kTooManyProperties, path);
    }
    for (const auto [name, property] : *properties) {
      if (!ValidPropertyName(name)) {
        return Failure(JsonSchemaImportError::kInvalidPropertyName,
                       path + ".properties." + name);
      }
      const base::DictValue* property_dict = property.GetIfDict();
      if (!property_dict) {
        return Failure(JsonSchemaImportError::kMalformedSchema,
                       path + ".properties." + name);
      }
      auto field = FieldFromSchema(name, *property_dict,
                                   path + ".properties." + name, depth);
      if (!field.has_value()) {
        return base::unexpected(field.error());
      }
      fields.push_back(std::move(field.value()));
    }
  }
  if (const base::ListValue* required = schema.FindList("required")) {
    for (const base::Value& entry : *required) {
      if (!entry.is_string()) {
        return Failure(JsonSchemaImportError::kMalformedSchema,
                       path + ".required");
      }
      for (SchemaField& field : fields) {
        if (field.name == entry.GetString()) {
          field.required = true;
        }
      }
    }
  }
  return fields;
}

FieldResult FieldFromSchema(const std::string& name,
                            const base::DictValue& schema,
                            const std::string& path,
                            size_t depth) {
  SchemaField field;
  field.name = name;
  if (const std::string* description = schema.FindString("description")) {
    field.description = *description;
  }
  const std::string* type = schema.FindString("type");
  if (!type) {
    return Failure(JsonSchemaImportError::kUnsupportedType, path);
  }
  if (*type == "string") {
    if (const base::ListValue* enum_values = schema.FindList("enum")) {
      field.kind = SchemaFieldKind::kEnum;
      if (enum_values->empty() ||
          enum_values->size() > kMaxSchemaEnumValues) {
        return Failure(JsonSchemaImportError::kInvalidEnum, path);
      }
      for (const base::Value& value : *enum_values) {
        if (!value.is_string()) {
          return Failure(JsonSchemaImportError::kInvalidEnum, path);
        }
        field.enum_values.push_back(value.GetString());
      }
      return field;
    }
    const std::string* format = schema.FindString("format");
    field.kind = (format && (*format == "uri" || *format == "url"))
                     ? SchemaFieldKind::kUrl
                     : SchemaFieldKind::kString;
    return field;
  }
  if (*type == "integer") {
    field.kind = SchemaFieldKind::kInteger;
    std::optional<double> minimum = schema.FindDouble("minimum");
    std::optional<double> maximum = schema.FindDouble("maximum");
    if (minimum.has_value() && maximum.has_value()) {
      field.has_range = true;
      field.min_value = *minimum;
      field.max_value = *maximum;
    }
    return field;
  }
  if (*type == "number") {
    field.kind = SchemaFieldKind::kNumber;
    std::optional<double> minimum = schema.FindDouble("minimum");
    std::optional<double> maximum = schema.FindDouble("maximum");
    if (minimum.has_value() && maximum.has_value()) {
      field.has_range = true;
      field.min_value = *minimum;
      field.max_value = *maximum;
    }
    return field;
  }
  if (*type == "boolean") {
    field.kind = SchemaFieldKind::kBoolean;
    return field;
  }
  if (*type == "array") {
    const base::DictValue* items = schema.FindDict("items");
    if (!items) {
      return Failure(JsonSchemaImportError::kMalformedSchema,
                     path + ".items");
    }
    field.kind = SchemaFieldKind::kList;
    auto item_field =
        FieldFromSchema("item", *items, path + ".items", depth + 1);
    if (!item_field.has_value()) {
      return base::unexpected(item_field.error());
    }
    field.children.push_back(std::move(item_field.value()));
    return field;
  }
  if (*type == "object") {
    field.kind = SchemaFieldKind::kObject;
    auto children = FieldsFromObjectSchema(schema, path, depth + 1);
    if (!children.has_value()) {
      return base::unexpected(children.error());
    }
    if (children->empty()) {
      return Failure(JsonSchemaImportError::kMalformedSchema, path);
    }
    field.children = std::move(children.value());
    return field;
  }
  return Failure(JsonSchemaImportError::kUnsupportedType, path);
}

}  // namespace

JsonSchemaImportResult ToolSchemaFromJsonSchema(
    const base::DictValue& json_schema) {
  const std::string* type = json_schema.FindString("type");
  if (!type || *type != "object") {
    return Failure(JsonSchemaImportError::kNotAnObjectSchema, "");
  }
  auto fields = FieldsFromObjectSchema(json_schema, "", /*depth=*/1);
  if (!fields.has_value()) {
    return base::unexpected(fields.error());
  }
  ToolSchema schema;
  schema.fields = std::move(fields.value());
  if (!IsWellFormedSchema(schema)) {
    return Failure(JsonSchemaImportError::kMalformedSchema, "");
  }
  return schema;
}

const char* JsonSchemaImportErrorToString(JsonSchemaImportError error) {
  switch (error) {
    case JsonSchemaImportError::kNotAnObjectSchema:
      return "not_an_object_schema";
    case JsonSchemaImportError::kUnsupportedType:
      return "unsupported_type";
    case JsonSchemaImportError::kUnsupportedKeyword:
      return "unsupported_keyword";
    case JsonSchemaImportError::kInvalidPropertyName:
      return "invalid_property_name";
    case JsonSchemaImportError::kTooManyProperties:
      return "too_many_properties";
    case JsonSchemaImportError::kDepthExceeded:
      return "depth_exceeded";
    case JsonSchemaImportError::kInvalidEnum:
      return "invalid_enum";
    case JsonSchemaImportError::kMalformedSchema:
      return "malformed_schema";
  }
  return "malformed_schema";
}

}  // namespace seoul
