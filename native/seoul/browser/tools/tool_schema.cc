// Project Seoul general-purpose operator: tool layer.

#include "seoul/browser/tools/tool_schema.h"

#include <cmath>

#include "url/gurl.h"

namespace seoul {

SchemaField::SchemaField() = default;
SchemaField::SchemaField(const SchemaField&) = default;
SchemaField::SchemaField(SchemaField&&) = default;
SchemaField& SchemaField::operator=(const SchemaField&) = default;
SchemaField& SchemaField::operator=(SchemaField&&) = default;
SchemaField::~SchemaField() = default;

ToolSchema::ToolSchema() = default;
ToolSchema::ToolSchema(const ToolSchema&) = default;
ToolSchema::ToolSchema(ToolSchema&&) = default;
ToolSchema& ToolSchema::operator=(const ToolSchema&) = default;
ToolSchema& ToolSchema::operator=(ToolSchema&&) = default;
ToolSchema::~ToolSchema() = default;

SchemaViolation::SchemaViolation() = default;
SchemaViolation::SchemaViolation(const SchemaViolation&) = default;
SchemaViolation::SchemaViolation(SchemaViolation&&) = default;
SchemaViolation& SchemaViolation::operator=(const SchemaViolation&) = default;
SchemaViolation& SchemaViolation::operator=(SchemaViolation&&) = default;
SchemaViolation::~SchemaViolation() = default;

namespace {

bool ValidFieldName(const std::string& name) {
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

bool WellFormedFields(const std::vector<SchemaField>& fields, size_t depth) {
  if (depth > kMaxSchemaDepth || fields.size() > kMaxSchemaFields) {
    return false;
  }
  for (const SchemaField& field : fields) {
    if (!ValidFieldName(field.name)) {
      return false;
    }
    switch (field.kind) {
      case SchemaFieldKind::kEnum:
        if (field.enum_values.empty() ||
            field.enum_values.size() > kMaxSchemaEnumValues) {
          return false;
        }
        break;
      case SchemaFieldKind::kList:
        if (field.children.size() != 1 ||
            !WellFormedFields(field.children, depth + 1)) {
          return false;
        }
        break;
      case SchemaFieldKind::kObject:
        if (field.children.empty() ||
            !WellFormedFields(field.children, depth + 1)) {
          return false;
        }
        break;
      case SchemaFieldKind::kInteger:
      case SchemaFieldKind::kNumber:
        if (field.has_range && field.min_value > field.max_value) {
          return false;
        }
        break;
      case SchemaFieldKind::kString:
      case SchemaFieldKind::kUrl:
      case SchemaFieldKind::kBoolean:
        break;
    }
    // Duplicate names within one level are malformed.
    size_t same_name = 0;
    for (const SchemaField& other : fields) {
      if (other.name == field.name) {
        ++same_name;
      }
    }
    if (same_name != 1) {
      return false;
    }
  }
  return true;
}

base::unexpected<SchemaViolation> Violation(SchemaViolationKind kind,
                                            const std::string& path) {
  SchemaViolation violation;
  violation.kind = kind;
  violation.field_path = path;
  return base::unexpected(violation);
}

SchemaValidationResult ValidateValue(const SchemaField& field,
                                     const base::Value& value,
                                     const std::string& path,
                                     size_t depth);

SchemaValidationResult ValidateDict(const std::vector<SchemaField>& fields,
                                    const base::DictValue& dict,
                                    const std::string& path,
                                    size_t depth) {
  if (depth > kMaxSchemaDepth) {
    return Violation(SchemaViolationKind::kDepthExceeded, path);
  }
  auto join = [&path](const std::string& name) {
    return path.empty() ? name : path + "." + name;
  };
  for (const auto [key, value] : dict) {
    const SchemaField* declared = nullptr;
    for (const SchemaField& field : fields) {
      if (field.name == key) {
        declared = &field;
        break;
      }
    }
    if (!declared) {
      return Violation(SchemaViolationKind::kUnknownField, join(key));
    }
    if (auto result = ValidateValue(*declared, value, join(key), depth);
        !result.has_value()) {
      return result;
    }
  }
  for (const SchemaField& field : fields) {
    if (field.required && !dict.contains(field.name)) {
      return Violation(SchemaViolationKind::kMissingRequiredField,
                       join(field.name));
    }
  }
  return base::ok();
}

SchemaValidationResult ValidateValue(const SchemaField& field,
                                     const base::Value& value,
                                     const std::string& path,
                                     size_t depth) {
  switch (field.kind) {
    case SchemaFieldKind::kString: {
      if (!value.is_string()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      if (value.GetString().size() > field.max_length) {
        return Violation(SchemaViolationKind::kTooLong, path);
      }
      return base::ok();
    }
    case SchemaFieldKind::kInteger: {
      if (!value.is_int()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      if (field.has_range && (value.GetInt() < field.min_value ||
                              value.GetInt() > field.max_value)) {
        return Violation(SchemaViolationKind::kOutOfRange, path);
      }
      return base::ok();
    }
    case SchemaFieldKind::kNumber: {
      if (!value.is_double() && !value.is_int()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      const double number = value.GetDouble();
      if (!std::isfinite(number)) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      if (field.has_range &&
          (number < field.min_value || number > field.max_value)) {
        return Violation(SchemaViolationKind::kOutOfRange, path);
      }
      return base::ok();
    }
    case SchemaFieldKind::kBoolean: {
      if (!value.is_bool()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      return base::ok();
    }
    case SchemaFieldKind::kEnum: {
      if (!value.is_string()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      for (const std::string& allowed : field.enum_values) {
        if (value.GetString() == allowed) {
          return base::ok();
        }
      }
      return Violation(SchemaViolationKind::kNotInEnum, path);
    }
    case SchemaFieldKind::kUrl: {
      if (!value.is_string()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      if (value.GetString().size() > field.max_length) {
        return Violation(SchemaViolationKind::kTooLong, path);
      }
      GURL url{value.GetString()};
      if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
        return Violation(SchemaViolationKind::kInvalidUrl, path);
      }
      return base::ok();
    }
    case SchemaFieldKind::kList: {
      if (!value.is_list()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      const base::ListValue& list = value.GetList();
      if (list.size() > kMaxArgListItems) {
        return Violation(SchemaViolationKind::kTooManyItems, path);
      }
      for (size_t i = 0; i < list.size(); ++i) {
        const std::string item_path = path + "[" + std::to_string(i) + "]";
        if (auto result =
                ValidateValue(field.children[0], list[i], item_path, depth + 1);
            !result.has_value()) {
          return result;
        }
      }
      return base::ok();
    }
    case SchemaFieldKind::kObject: {
      if (!value.is_dict()) {
        return Violation(SchemaViolationKind::kWrongKind, path);
      }
      return ValidateDict(field.children, value.GetDict(), path, depth + 1);
    }
  }
  return Violation(SchemaViolationKind::kMalformedSchema, path);
}

}  // namespace

const char* SchemaViolationKindToString(SchemaViolationKind kind) {
  switch (kind) {
    case SchemaViolationKind::kMalformedSchema:
      return "malformed_schema";
    case SchemaViolationKind::kUnknownField:
      return "unknown_field";
    case SchemaViolationKind::kMissingRequiredField:
      return "missing_required_field";
    case SchemaViolationKind::kWrongKind:
      return "wrong_kind";
    case SchemaViolationKind::kOutOfRange:
      return "out_of_range";
    case SchemaViolationKind::kTooLong:
      return "too_long";
    case SchemaViolationKind::kNotInEnum:
      return "not_in_enum";
    case SchemaViolationKind::kInvalidUrl:
      return "invalid_url";
    case SchemaViolationKind::kTooManyItems:
      return "too_many_items";
    case SchemaViolationKind::kDepthExceeded:
      return "depth_exceeded";
  }
  return "malformed_schema";
}

bool IsWellFormedSchema(const ToolSchema& schema) {
  return WellFormedFields(schema.fields, /*depth=*/1);
}

SchemaValidationResult ValidateArgs(const ToolSchema& schema,
                                    const base::DictValue& args) {
  if (!IsWellFormedSchema(schema)) {
    return Violation(SchemaViolationKind::kMalformedSchema, "");
  }
  return ValidateDict(schema.fields, args, "", /*depth=*/1);
}

}  // namespace seoul
