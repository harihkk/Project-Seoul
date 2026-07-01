// Project Seoul connected tools.
// Manages explicitly connected connectors and mirrors their typed tools into
// the shared Tool Registry. Connecting registers a connector's discovered
// tools; disconnecting removes exactly that provider's tools. An external
// connector can never register a tool outside its own connector.<provider>.*
// namespace (the Tool Registry enforces ownership).

#ifndef SEOUL_BROWSER_CONNECTORS_CONNECTOR_REGISTRY_H_
#define SEOUL_BROWSER_CONNECTORS_CONNECTOR_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "seoul/browser/connectors/connector.h"
#include "seoul/browser/tools/tool_registry.h"

namespace seoul {

enum class ConnectorError {
  kNullConnector,
  kDuplicateProvider,
  kUnknownProvider,
  kReservedProvider,  // "seoul" and reserved roots are not connectors
  kToolRegistrationFailed,
};

const char* ConnectorErrorToString(ConnectorError error);

using ConnectorStatusResult = base::expected<void, ConnectorError>;

class ConnectorRegistry {
 public:
  // The tool registry outlives this object (owned by the profile service).
  explicit ConnectorRegistry(ToolRegistry* tool_registry);
  ConnectorRegistry(const ConnectorRegistry&) = delete;
  ConnectorRegistry& operator=(const ConnectorRegistry&) = delete;
  ~ConnectorRegistry();

  // Connects a connector and registers its discovered tools. If any tool
  // fails registration (bad namespace, duplicate), the whole connect is
  // rolled back so no partial tool set is exposed.
  ConnectorStatusResult Connect(std::unique_ptr<Connector> connector);

  // Disconnects a provider and removes exactly its tools.
  ConnectorStatusResult Disconnect(const std::string& provider);

  // Re-discovers and re-registers a connected provider's tools (for example
  // after a scope change).
  ConnectorStatusResult Refresh(const std::string& provider);

  const Connector* Find(const std::string& provider) const;
  std::vector<const Connector*> Connected() const;  // provider-ordered
  size_t size() const { return connectors_.size(); }

 private:
  ConnectorStatusResult RegisterConnectorTools(Connector* connector);

  raw_ptr<ToolRegistry> tool_registry_;
  std::map<std::string, std::unique_ptr<Connector>> connectors_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_CONNECTOR_REGISTRY_H_
