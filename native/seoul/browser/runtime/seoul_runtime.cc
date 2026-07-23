// Project Seoul runtime composition.

#include "seoul/browser/runtime/seoul_runtime.h"

#include <string>
#include <utility>
#include <vector>

#include "seoul/browser/connectors/generic_capabilities.h"
#include "seoul/browser/connectors/local_file_capability.h"

namespace seoul {

SeoulRuntime::SeoulRuntime(SceneResolvers scene_resolvers)
    : connectors_(&capabilities_), scenes_(std::move(scene_resolvers)) {
  RegisterBuiltinCapabilities();
}

SeoulRuntime::~SeoulRuntime() {
  if (!shutting_down_) {
    Shutdown();
  }
}

void SeoulRuntime::RegisterBuiltinCapabilities() {
  // The built-in "seoul" capabilities are the browser command catalog, the
  // generic information seams, and local-file access. Each has a production
  // owner (this runtime); the task planner only ever sees registered,
  // permitted capabilities from here.
  for (const ToolDescriptor& descriptor : BuildBrowserCapabilities()) {
    capabilities_.Register(descriptor);
  }
  for (const ToolDescriptor& descriptor : BuildInformationCapabilities()) {
    capabilities_.Register(descriptor);
  }
  for (const ToolDescriptor& descriptor : BuildLocalFileCapabilities()) {
    capabilities_.Register(descriptor);
  }
}

RoutingPolicy SeoulRuntime::MakeRoutingPolicy(
    bool cloud_enabled,
    bool local_available,
    int64_t remaining_budget_microdollars) const {
  RoutingPolicy policy;
  policy.cloud_enabled = cloud_enabled;
  policy.local_available = local_available;
  policy.remaining_budget_microdollars = remaining_budget_microdollars;
  return policy;
}

void SeoulRuntime::Shutdown() {
  shutting_down_ = true;
  // Disconnect connectors first: each disconnect removes exactly that
  // provider's capabilities from the graph, so no dangling capability
  // outlives its connector. Collect provider names before mutating the map.
  std::vector<std::string> providers;
  for (const Connector* connector : connectors_.Connected()) {
    providers.push_back(connector->provider());
  }
  for (const std::string& provider : providers) {
    connectors_.Disconnect(provider);
  }
}

}  // namespace seoul
