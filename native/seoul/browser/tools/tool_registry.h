// Project Seoul general-purpose operator: tool layer.
// The dynamic tool registry. Builtins register under Seoul-owned namespaces;
// connectors register under connector.<provider>.*. Discovery is filtered by
// an explicit permission context, so a planner only ever sees tools it is
// currently allowed to use. Lookup of an unregistered id fails precisely;
// there is no fallback dispatch.

#ifndef SEOUL_BROWSER_TOOLS_TOOL_REGISTRY_H_
#define SEOUL_BROWSER_TOOLS_TOOL_REGISTRY_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "seoul/browser/tools/tool_types.h"

namespace seoul {

// What the current task/scene is allowed to reach. Built by the task layer
// from user settings, Scene defaults, and connected services.
struct ToolPermissionContext {
  DataSensitivity max_sensitivity = DataSensitivity::kOrganization;
  bool allow_network = false;
  // Connector providers currently connected and enabled. Builtin ("seoul")
  // tools are always provider-eligible; sensitivity/network still apply.
  std::set<std::string> connected_providers;
};

class ToolRegistry {
 public:
  ToolRegistry();
  ToolRegistry(const ToolRegistry&) = delete;
  ToolRegistry& operator=(const ToolRegistry&) = delete;
  ~ToolRegistry();

  // Validates and registers one descriptor. Seoul-owned root namespaces
  // ("browser", "page", "info", "canvas", "files", "workflow", "task",
  // "scene") accept only provider "seoul". Connector descriptors must live
  // under "connector.<provider>.*".
  ToolStatusResult Register(ToolDescriptor descriptor);

  // Removes every tool registered by `provider` (connector disconnect).
  // Returns how many were removed.
  size_t UnregisterProvider(const std::string& provider);

  // Replaces an existing descriptor with the same id (dynamic update, for
  // example after a connector refresh changes a schema). The updating
  // provider must own the capability; validation matches Register.
  ToolStatusResult UpdateDescriptor(ToolDescriptor descriptor);

  // Null for unknown ids: the caller must treat that as a rejected plan step,
  // never as a soft failure.
  const ToolDescriptor* Find(const ToolId& id) const;

  // Version negotiation: the descriptor if its version is at least
  // `min_version`, else null (the caller treats it as unavailable).
  const ToolDescriptor* FindCompatible(const ToolId& id,
                                       int min_version) const;

  // Dynamic availability and health, updated by connectors and health checks.
  // Unavailable capabilities never appear in ListAvailable.
  bool SetAvailability(const ToolId& id,
                       AvailabilityState state,
                       const std::string& reason);
  AvailabilityState GetAvailability(const ToolId& id) const;
  bool SetHealth(const ToolId& id, HealthState state);
  HealthState GetHealth(const ToolId& id) const;

  // Deterministic (id-ordered) list of tools permitted under `context`,
  // excluding unavailable capabilities.
  std::vector<const ToolDescriptor*> ListAvailable(
      const ToolPermissionContext& context) const;

  size_t size() const { return tools_.size(); }

 private:
  struct Entry {
    ToolDescriptor descriptor;
    AvailabilityState availability = AvailabilityState::kAvailable;
    std::string availability_reason;
    HealthState health = HealthState::kUnknown;
  };

  bool PermittedUnder(const Entry& entry,
                      const ToolPermissionContext& context) const;
  ToolStatusResult ValidateDescriptor(const ToolDescriptor& descriptor) const;

  std::map<ToolId, Entry> tools_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_TOOLS_TOOL_REGISTRY_H_
