// Project Seoul general-purpose operator: tool layer.

#include "seoul/browser/tools/tool_registry.h"

#include <utility>

namespace seoul {

ToolPermissionContext::ToolPermissionContext() = default;
ToolPermissionContext::ToolPermissionContext(const ToolPermissionContext&) =
    default;
ToolPermissionContext::ToolPermissionContext(ToolPermissionContext&&) =
    default;
ToolPermissionContext& ToolPermissionContext::operator=(
    const ToolPermissionContext&) = default;
ToolPermissionContext& ToolPermissionContext::operator=(
    ToolPermissionContext&&) = default;
ToolPermissionContext::~ToolPermissionContext() = default;

namespace {

// Root namespaces reserved for Seoul builtins. A connector can never shadow a
// native browser action.
constexpr const char* kSeoulNamespaces[] = {
    "browser", "page", "info", "canvas", "files", "workflow", "task", "scene",
};

bool IsSeoulNamespace(const std::string& root) {
  for (const char* reserved : kSeoulNamespaces) {
    if (root == reserved) {
      return true;
    }
  }
  return false;
}

}  // namespace

ToolId ToolId::FromString(std::string_view value) {
  ToolId id;
  if (IsValidString(value)) {
    id.value_ = std::string(value);
  }
  return id;
}

bool ToolId::IsValidString(std::string_view value) {
  if (value.empty() || value.size() > kMaxToolIdLength) {
    return false;
  }
  size_t segments = 1;
  bool at_segment_start = true;
  for (char c : value) {
    if (c == '.') {
      if (at_segment_start) {
        return false;  // empty segment
      }
      ++segments;
      at_segment_start = true;
      continue;
    }
    if (at_segment_start) {
      if (c < 'a' || c > 'z') {
        return false;
      }
      at_segment_start = false;
      continue;
    }
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
      return false;
    }
  }
  return !at_segment_start && segments >= 2 && segments <= 4;
}

std::string ToolId::root_namespace() const {
  const size_t dot = value_.find('.');
  return dot == std::string::npos ? value_ : value_.substr(0, dot);
}

ToolRegistry::ToolRegistry() = default;
ToolRegistry::~ToolRegistry() = default;

ToolStatusResult ToolRegistry::ValidateDescriptor(
    const ToolDescriptor& descriptor) const {
  if (!descriptor.id.is_valid()) {
    return base::unexpected(ToolError::kInvalidId);
  }
  if (descriptor.name.empty() || descriptor.description.empty() ||
      descriptor.provider.empty() || descriptor.version < 1 ||
      descriptor.retry.max_attempts < 1 ||
      descriptor.capability_tags.size() > kMaxCapabilityTags ||
      !IsWellFormedSchema(descriptor.input_schema) ||
      !IsWellFormedSchema(descriptor.output_schema)) {
    return base::unexpected(ToolError::kInvalidDescriptor);
  }
  const std::string root = descriptor.id.root_namespace();
  if (IsSeoulNamespace(root)) {
    if (descriptor.provider != "seoul") {
      return base::unexpected(ToolError::kReservedNamespace);
    }
  } else if (root == "connector") {
    // connector.<provider>.<tool...>: the second segment must equal the
    // registering provider so one connector cannot impersonate another.
    const std::string& value = descriptor.id.value();
    const size_t first_dot = value.find('.');
    const size_t second_dot = value.find('.', first_dot + 1);
    if (second_dot == std::string::npos) {
      return base::unexpected(ToolError::kInvalidId);
    }
    const std::string owner =
        value.substr(first_dot + 1, second_dot - first_dot - 1);
    if (owner != descriptor.provider) {
      return base::unexpected(ToolError::kProviderMismatch);
    }
  } else {
    return base::unexpected(ToolError::kReservedNamespace);
  }
  return base::ok();
}

ToolStatusResult ToolRegistry::Register(ToolDescriptor descriptor) {
  if (auto valid = ValidateDescriptor(descriptor); !valid.has_value()) {
    return valid;
  }
  if (tools_.find(descriptor.id) != tools_.end()) {
    return base::unexpected(ToolError::kDuplicateId);
  }
  if (tools_.size() >= kMaxRegisteredTools) {
    return base::unexpected(ToolError::kLimitExceeded);
  }
  Entry entry;
  const ToolId id = descriptor.id;
  entry.descriptor = std::move(descriptor);
  tools_.emplace(id, std::move(entry));
  return base::ok();
}

ToolStatusResult ToolRegistry::UpdateDescriptor(ToolDescriptor descriptor) {
  if (auto valid = ValidateDescriptor(descriptor); !valid.has_value()) {
    return valid;
  }
  auto it = tools_.find(descriptor.id);
  if (it == tools_.end()) {
    return base::unexpected(ToolError::kUnknownTool);
  }
  if (it->second.descriptor.provider != descriptor.provider) {
    // A different provider can never take over an existing capability id.
    return base::unexpected(ToolError::kProviderMismatch);
  }
  it->second.descriptor = std::move(descriptor);
  return base::ok();
}

size_t ToolRegistry::UnregisterProvider(const std::string& provider) {
  size_t removed = 0;
  for (auto it = tools_.begin(); it != tools_.end();) {
    if (it->second.descriptor.provider == provider) {
      it = tools_.erase(it);
      ++removed;
    } else {
      ++it;
    }
  }
  return removed;
}

const ToolDescriptor* ToolRegistry::Find(const ToolId& id) const {
  auto it = tools_.find(id);
  return it == tools_.end() ? nullptr : &it->second.descriptor;
}

const ToolDescriptor* ToolRegistry::FindCompatible(const ToolId& id,
                                                   int min_version) const {
  const ToolDescriptor* descriptor = Find(id);
  if (!descriptor || descriptor->version < min_version) {
    return nullptr;
  }
  return descriptor;
}

bool ToolRegistry::SetAvailability(const ToolId& id,
                                   AvailabilityState state,
                                   const std::string& reason) {
  auto it = tools_.find(id);
  if (it == tools_.end()) {
    return false;
  }
  it->second.availability = state;
  it->second.availability_reason = reason;
  return true;
}

AvailabilityState ToolRegistry::GetAvailability(const ToolId& id) const {
  auto it = tools_.find(id);
  return it == tools_.end() ? AvailabilityState::kUnavailable
                            : it->second.availability;
}

bool ToolRegistry::SetHealth(const ToolId& id, HealthState state) {
  auto it = tools_.find(id);
  if (it == tools_.end()) {
    return false;
  }
  it->second.health = state;
  return true;
}

HealthState ToolRegistry::GetHealth(const ToolId& id) const {
  auto it = tools_.find(id);
  return it == tools_.end() ? HealthState::kUnknown : it->second.health;
}

bool ToolRegistry::PermittedUnder(const Entry& entry,
                                  const ToolPermissionContext& context) const {
  if (entry.availability == AvailabilityState::kUnavailable) {
    return false;
  }
  const ToolDescriptor& descriptor = entry.descriptor;
  if (static_cast<int>(descriptor.sensitivity) >
      static_cast<int>(context.max_sensitivity)) {
    return false;
  }
  if (descriptor.requires_network && !context.allow_network) {
    return false;
  }
  if (descriptor.provider != "seoul" &&
      context.connected_providers.find(descriptor.provider) ==
          context.connected_providers.end()) {
    return false;
  }
  return true;
}

std::vector<const ToolDescriptor*> ToolRegistry::ListAvailable(
    const ToolPermissionContext& context) const {
  std::vector<const ToolDescriptor*> available;
  for (const auto& [id, entry] : tools_) {
    if (PermittedUnder(entry, context)) {
      available.push_back(&entry.descriptor);
    }
  }
  return available;
}

const char* AvailabilityStateToString(AvailabilityState state) {
  switch (state) {
    case AvailabilityState::kAvailable:
      return "available";
    case AvailabilityState::kDegraded:
      return "degraded";
    case AvailabilityState::kUnavailable:
      return "unavailable";
  }
  return "unavailable";
}

const char* HealthStateToString(HealthState state) {
  switch (state) {
    case HealthState::kUnknown:
      return "unknown";
    case HealthState::kHealthy:
      return "healthy";
    case HealthState::kUnhealthy:
      return "unhealthy";
  }
  return "unknown";
}

const char* ToolErrorToString(ToolError error) {
  switch (error) {
    case ToolError::kInvalidId:
      return "invalid_id";
    case ToolError::kDuplicateId:
      return "duplicate_id";
    case ToolError::kUnknownTool:
      return "unknown_tool";
    case ToolError::kInvalidDescriptor:
      return "invalid_descriptor";
    case ToolError::kReservedNamespace:
      return "reserved_namespace";
    case ToolError::kProviderMismatch:
      return "provider_mismatch";
    case ToolError::kLimitExceeded:
      return "limit_exceeded";
  }
  return "invalid_descriptor";
}

}  // namespace seoul
