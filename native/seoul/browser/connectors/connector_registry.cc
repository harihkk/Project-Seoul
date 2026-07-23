// Project Seoul connected tools.

#include "seoul/browser/connectors/connector_registry.h"

#include <utility>

namespace seoul {

ConnectorRegistry::ConnectorRegistry(ToolRegistry* tool_registry)
    : tool_registry_(tool_registry) {}

ConnectorRegistry::~ConnectorRegistry() = default;

ConnectorStatusResult ConnectorRegistry::RegisterConnectorTools(
    Connector* connector) {
  const std::string provider = connector->provider();
  std::vector<ToolDescriptor> tools = connector->DiscoverTools();
  for (ToolDescriptor& descriptor : tools) {
    // Force the provider to match; the registry additionally enforces that
    // the id lives under connector.<provider>.*.
    descriptor.provider = provider;
    if (!tool_registry_->Register(std::move(descriptor)).has_value()) {
      // Roll back any tools registered so far for this provider.
      tool_registry_->UnregisterProvider(provider);
      return base::unexpected(ConnectorError::kToolRegistrationFailed);
    }
  }
  return base::ok();
}

ConnectorStatusResult ConnectorRegistry::Connect(
    std::unique_ptr<Connector> connector) {
  if (!connector) {
    return base::unexpected(ConnectorError::kNullConnector);
  }
  const std::string provider = connector->provider();
  if (provider.empty() || provider == "seoul") {
    return base::unexpected(ConnectorError::kReservedProvider);
  }
  if (connectors_.find(provider) != connectors_.end()) {
    return base::unexpected(ConnectorError::kDuplicateProvider);
  }
  if (auto result = RegisterConnectorTools(connector.get());
      !result.has_value()) {
    return result;
  }
  connectors_[provider] = std::move(connector);
  return base::ok();
}

ConnectorStatusResult ConnectorRegistry::Disconnect(
    const std::string& provider) {
  auto it = connectors_.find(provider);
  if (it == connectors_.end()) {
    return base::unexpected(ConnectorError::kUnknownProvider);
  }
  tool_registry_->UnregisterProvider(provider);
  connectors_.erase(it);
  return base::ok();
}

ConnectorStatusResult ConnectorRegistry::Refresh(const std::string& provider) {
  auto it = connectors_.find(provider);
  if (it == connectors_.end()) {
    return base::unexpected(ConnectorError::kUnknownProvider);
  }
  tool_registry_->UnregisterProvider(provider);
  return RegisterConnectorTools(it->second.get());
}

const Connector* ConnectorRegistry::Find(const std::string& provider) const {
  auto it = connectors_.find(provider);
  return it == connectors_.end() ? nullptr : it->second.get();
}

std::vector<const Connector*> ConnectorRegistry::Connected() const {
  std::vector<const Connector*> result;
  for (const auto& [provider, connector] : connectors_) {
    result.push_back(connector.get());
  }
  return result;
}

const char* ConnectorStateToString(ConnectorState state) {
  switch (state) {
    case ConnectorState::kDisconnected:
      return "disconnected";
    case ConnectorState::kConnecting:
      return "connecting";
    case ConnectorState::kConnected:
      return "connected";
    case ConnectorState::kError:
      return "error";
  }
  return "disconnected";
}

const char* ConnectorErrorToString(ConnectorError error) {
  switch (error) {
    case ConnectorError::kNullConnector:
      return "null_connector";
    case ConnectorError::kDuplicateProvider:
      return "duplicate_provider";
    case ConnectorError::kUnknownProvider:
      return "unknown_provider";
    case ConnectorError::kReservedProvider:
      return "reserved_provider";
    case ConnectorError::kToolRegistrationFailed:
      return "tool_registration_failed";
  }
  return "null_connector";
}

}  // namespace seoul
