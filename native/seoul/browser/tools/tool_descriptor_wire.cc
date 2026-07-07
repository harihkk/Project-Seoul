// Project Seoul general-purpose operator: tool layer.

#include "seoul/browser/tools/tool_descriptor_wire.h"

#include <utility>

#include "base/time/time.h"
#include "seoul/browser/tools/tool_schema.h"
#include "seoul/browser/tools/tool_schema_from_json.h"

namespace seoul {

namespace {

constexpr int kWireProtocolVersion = 1;

base::DictValue SchemaFieldToJson(const SchemaField& field) {
  base::DictValue dict;
  switch (field.kind) {
    case SchemaFieldKind::kString:
      dict.Set("type", "string");
      break;
    case SchemaFieldKind::kInteger:
      dict.Set("type", "integer");
      break;
    case SchemaFieldKind::kNumber:
      dict.Set("type", "number");
      break;
    case SchemaFieldKind::kBoolean:
      dict.Set("type", "boolean");
      break;
    case SchemaFieldKind::kEnum: {
      dict.Set("type", "string");
      base::ListValue values;
      for (const std::string& value : field.enum_values) {
        values.Append(value);
      }
      dict.Set("enum", std::move(values));
      break;
    }
    case SchemaFieldKind::kUrl:
      dict.Set("type", "string");
      dict.Set("format", "uri");
      break;
    case SchemaFieldKind::kList:
      dict.Set("type", "array");
      if (!field.children.empty()) {
        dict.Set("items", SchemaFieldToJson(field.children[0]));
      }
      break;
    case SchemaFieldKind::kObject: {
      dict.Set("type", "object");
      base::DictValue properties;
      base::ListValue required;
      for (const SchemaField& child : field.children) {
        if (child.required) {
          required.Append(child.name);
        }
        properties.Set(child.name, SchemaFieldToJson(child));
      }
      dict.Set("properties", std::move(properties));
      if (!required.empty()) {
        dict.Set("required", std::move(required));
      }
      break;
    }
  }
  if (!field.description.empty()) {
    dict.Set("description", field.description);
  }
  if (field.has_range) {
    dict.Set("minimum", field.min_value);
    dict.Set("maximum", field.max_value);
  }
  return dict;
}

}  // namespace

const char* DataSensitivityToWire(DataSensitivity sensitivity) {
  switch (sensitivity) {
    case DataSensitivity::kNone:
      return "none";
    case DataSensitivity::kOrganization:
      return "organization";
    case DataSensitivity::kPageContent:
      return "page_content";
    case DataSensitivity::kPersonal:
      return "personal";
    case DataSensitivity::kCredentialAdjacent:
      return "credential_adjacent";
  }
  return "none";
}

bool DataSensitivityFromWire(std::string_view s, DataSensitivity* out) {
  constexpr std::pair<std::string_view, DataSensitivity> kNames[] = {
      {"none", DataSensitivity::kNone},
      {"organization", DataSensitivity::kOrganization},
      {"page_content", DataSensitivity::kPageContent},
      {"personal", DataSensitivity::kPersonal},
      {"credential_adjacent", DataSensitivity::kCredentialAdjacent}};
  for (const auto& [name, value] : kNames) {
    if (s == name) {
      *out = value;
      return true;
    }
  }
  return false;
}

const char* RiskCategoryToWire(RiskCategory risk) {
  switch (risk) {
    case RiskCategory::kReadOnly:
      return "read_only";
    case RiskCategory::kReversibleMutation:
      return "reversible_mutation";
    case RiskCategory::kIrreversibleMutation:
      return "irreversible_mutation";
    case RiskCategory::kExternalSideEffect:
      return "external_side_effect";
  }
  return "read_only";
}

bool RiskCategoryFromWire(std::string_view s, RiskCategory* out) {
  constexpr std::pair<std::string_view, RiskCategory> kNames[] = {
      {"read_only", RiskCategory::kReadOnly},
      {"reversible_mutation", RiskCategory::kReversibleMutation},
      {"irreversible_mutation", RiskCategory::kIrreversibleMutation},
      {"external_side_effect", RiskCategory::kExternalSideEffect}};
  for (const auto& [name, value] : kNames) {
    if (s == name) {
      *out = value;
      return true;
    }
  }
  return false;
}

const char* ApprovalPolicyToWire(ApprovalPolicy approval) {
  switch (approval) {
    case ApprovalPolicy::kNeverRequired:
      return "never_required";
    case ApprovalPolicy::kFirstUsePerScope:
      return "first_use_per_scope";
    case ApprovalPolicy::kAlwaysRequired:
      return "always_required";
  }
  return "never_required";
}

bool ApprovalPolicyFromWire(std::string_view s, ApprovalPolicy* out) {
  constexpr std::pair<std::string_view, ApprovalPolicy> kNames[] = {
      {"never_required", ApprovalPolicy::kNeverRequired},
      {"first_use_per_scope", ApprovalPolicy::kFirstUsePerScope},
      {"always_required", ApprovalPolicy::kAlwaysRequired}};
  for (const auto& [name, value] : kNames) {
    if (s == name) {
      *out = value;
      return true;
    }
  }
  return false;
}

const char* IdempotencyClassToWire(IdempotencyClass idempotency) {
  switch (idempotency) {
    case IdempotencyClass::kIdempotent:
      return "idempotent";
    case IdempotencyClass::kNotIdempotent:
      return "not_idempotent";
  }
  return "not_idempotent";
}

bool IdempotencyClassFromWire(std::string_view s, IdempotencyClass* out) {
  if (s == "idempotent") {
    *out = IdempotencyClass::kIdempotent;
    return true;
  }
  if (s == "not_idempotent") {
    *out = IdempotencyClass::kNotIdempotent;
    return true;
  }
  return false;
}

const char* FreshnessSemanticsToWire(FreshnessSemantics freshness) {
  switch (freshness) {
    case FreshnessSemantics::kRealTime:
      return "real_time";
    case FreshnessSemantics::kNearRealTime:
      return "near_real_time";
    case FreshnessSemantics::kCached:
      return "cached";
    case FreshnessSemantics::kStatic:
      return "static";
  }
  return "cached";
}

bool FreshnessSemanticsFromWire(std::string_view s, FreshnessSemantics* out) {
  constexpr std::pair<std::string_view, FreshnessSemantics> kNames[] = {
      {"real_time", FreshnessSemantics::kRealTime},
      {"near_real_time", FreshnessSemantics::kNearRealTime},
      {"cached", FreshnessSemantics::kCached},
      {"static", FreshnessSemantics::kStatic}};
  for (const auto& [name, value] : kNames) {
    if (s == name) {
      *out = value;
      return true;
    }
  }
  return false;
}

base::DictValue ToolSchemaToJsonSchema(const ToolSchema& schema) {
  base::DictValue dict;
  dict.Set("type", "object");
  base::DictValue properties;
  base::ListValue required;
  for (const SchemaField& field : schema.fields) {
    if (field.required) {
      required.Append(field.name);
    }
    properties.Set(field.name, SchemaFieldToJson(field));
  }
  dict.Set("properties", std::move(properties));
  dict.Set("required", std::move(required));
  return dict;
}

base::DictValue ToolDescriptorToValue(const ToolDescriptor& descriptor) {
  base::DictValue dict;
  dict.Set("schema_version", kWireProtocolVersion);
  dict.Set("id", descriptor.id.value());
  dict.Set("version", descriptor.version);
  dict.Set("name", descriptor.name);
  dict.Set("description", descriptor.description);
  dict.Set("provider", descriptor.provider);
  dict.Set("input_schema", ToolSchemaToJsonSchema(descriptor.input_schema));
  dict.Set("output_schema", ToolSchemaToJsonSchema(descriptor.output_schema));
  if (!descriptor.capability_tags.empty()) {
    base::ListValue tags;
    for (const std::string& tag : descriptor.capability_tags) {
      tags.Append(tag);
    }
    dict.Set("capability_tags", std::move(tags));
  }
  dict.Set("requires_network", descriptor.requires_network);
  dict.Set("sensitivity", DataSensitivityToWire(descriptor.sensitivity));
  dict.Set("risk", RiskCategoryToWire(descriptor.risk));
  dict.Set("approval", ApprovalPolicyToWire(descriptor.approval));
  dict.Set("timeout_ms", static_cast<int>(descriptor.timeout.InMilliseconds()));
  dict.Set("cancellable", descriptor.cancellable);
  dict.Set("supports_streaming", descriptor.supports_streaming);
  dict.Set("idempotency", IdempotencyClassToWire(descriptor.idempotency));
  dict.Set("freshness", FreshnessSemanticsToWire(descriptor.freshness));
  base::DictValue retry;
  retry.Set("max_attempts", descriptor.retry.max_attempts);
  retry.Set("backoff_ms", descriptor.retry.backoff_ms);
  dict.Set("retry", std::move(retry));
  if (!descriptor.observation_contract.empty()) {
    dict.Set("observation_contract", descriptor.observation_contract);
  }
  if (!descriptor.verifier_id.empty()) {
    dict.Set("verifier_id", descriptor.verifier_id);
  }
  if (!descriptor.preferred_component.empty()) {
    dict.Set("preferred_component", descriptor.preferred_component);
  }
  return dict;
}

base::expected<ToolDescriptor, std::string> ParseToolDescriptor(
    const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected("descriptor must be an object");
  }
  std::optional<int> schema_version = dict->FindInt("schema_version");
  if (!schema_version.has_value() ||
      *schema_version != kWireProtocolVersion) {
    return base::unexpected("unsupported descriptor schema_version");
  }
  ToolDescriptor descriptor;
  const std::string* id = dict->FindString("id");
  if (!id) {
    return base::unexpected("id missing");
  }
  descriptor.id = ToolId::FromString(*id);
  if (!descriptor.id.is_valid()) {
    return base::unexpected("invalid tool id: " + *id);
  }
  descriptor.version = dict->FindInt("version").value_or(1);
  if (descriptor.version < 1) {
    return base::unexpected("version must be >= 1");
  }
  const std::string* name = dict->FindString("name");
  const std::string* description = dict->FindString("description");
  const std::string* provider = dict->FindString("provider");
  if (!name || name->empty() || !description || description->empty() ||
      !provider || provider->empty()) {
    return base::unexpected("name/description/provider required");
  }
  descriptor.name = *name;
  descriptor.description = *description;
  descriptor.provider = *provider;
  for (const auto [key, schema_key] :
       {std::pair{"input_schema", &ToolDescriptor::input_schema},
        std::pair{"output_schema", &ToolDescriptor::output_schema}}) {
    if (const base::DictValue* schema_dict = dict->FindDict(key)) {
      auto schema = ToolSchemaFromJsonSchema(*schema_dict);
      if (!schema.has_value()) {
        return base::unexpected(
            std::string(key) + ": " +
            JsonSchemaImportErrorToString(schema.error().error) + " at " +
            schema.error().path);
      }
      descriptor.*schema_key = std::move(schema.value());
    }
  }
  if (const base::ListValue* tags = dict->FindList("capability_tags")) {
    if (tags->size() > kMaxCapabilityTags) {
      return base::unexpected("too many capability_tags");
    }
    for (const base::Value& tag : *tags) {
      if (!tag.is_string()) {
        return base::unexpected("capability_tags must be strings");
      }
      descriptor.capability_tags.push_back(tag.GetString());
    }
  }
  descriptor.requires_network =
      dict->FindBool("requires_network").value_or(false);
  if (const std::string* sensitivity = dict->FindString("sensitivity")) {
    if (!DataSensitivityFromWire(*sensitivity, &descriptor.sensitivity)) {
      return base::unexpected("unknown sensitivity: " + *sensitivity);
    }
  }
  if (const std::string* risk = dict->FindString("risk")) {
    if (!RiskCategoryFromWire(*risk, &descriptor.risk)) {
      return base::unexpected("unknown risk: " + *risk);
    }
  }
  if (const std::string* approval = dict->FindString("approval")) {
    if (!ApprovalPolicyFromWire(*approval, &descriptor.approval)) {
      return base::unexpected("unknown approval: " + *approval);
    }
  }
  if (std::optional<int> timeout_ms = dict->FindInt("timeout_ms")) {
    if (*timeout_ms < 1) {
      return base::unexpected("timeout_ms must be >= 1");
    }
    descriptor.timeout = base::Milliseconds(*timeout_ms);
  }
  descriptor.cancellable = dict->FindBool("cancellable").value_or(true);
  descriptor.supports_streaming =
      dict->FindBool("supports_streaming").value_or(false);
  if (const std::string* idempotency = dict->FindString("idempotency")) {
    if (!IdempotencyClassFromWire(*idempotency, &descriptor.idempotency)) {
      return base::unexpected("unknown idempotency: " + *idempotency);
    }
  }
  if (const std::string* freshness = dict->FindString("freshness")) {
    if (!FreshnessSemanticsFromWire(*freshness, &descriptor.freshness)) {
      return base::unexpected("unknown freshness: " + *freshness);
    }
  }
  if (const base::DictValue* retry = dict->FindDict("retry")) {
    descriptor.retry.max_attempts = retry->FindInt("max_attempts").value_or(1);
    descriptor.retry.backoff_ms = retry->FindInt("backoff_ms").value_or(0);
    if (descriptor.retry.max_attempts < 1 ||
        descriptor.retry.backoff_ms < 0) {
      return base::unexpected("invalid retry policy");
    }
  }
  if (const std::string* contract =
          dict->FindString("observation_contract")) {
    descriptor.observation_contract = *contract;
  }
  if (const std::string* verifier = dict->FindString("verifier_id")) {
    descriptor.verifier_id = *verifier;
  }
  if (const std::string* component =
          dict->FindString("preferred_component")) {
    descriptor.preferred_component = *component;
  }
  return descriptor;
}

}  // namespace seoul
