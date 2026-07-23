// Project Seoul connected tools.
// Provider-neutral connector seam. A connector represents one explicitly
// connected external service (mail, calendar, files, ...). It exposes typed
// tools through the same Tool Registry the general planner uses; it never
// injects native browser actions and never assumes a specific protocol. An
// MCP adapter is one possible connector implementation, not a dependency.

#ifndef SEOUL_BROWSER_CONNECTORS_CONNECTOR_H_
#define SEOUL_BROWSER_CONNECTORS_CONNECTOR_H_

#include <string>
#include <vector>

#include "seoul/browser/tools/tool_types.h"

namespace seoul {

enum class ConnectorState {
  kDisconnected,
  kConnecting,
  kConnected,
  kError,
};

const char* ConnectorStateToString(ConnectorState state);

// Identity of the connected account, shown to the user. Never holds tokens or
// credentials; those live in the platform secure store, out of this module.
struct ConnectorAccount {
  ConnectorAccount();
  ConnectorAccount(const ConnectorAccount&);
  ConnectorAccount(ConnectorAccount&&);
  ConnectorAccount& operator=(const ConnectorAccount&);
  ConnectorAccount& operator=(ConnectorAccount&&);
  ~ConnectorAccount();

  std::string account_label;  // "user@example.com", "Personal Drive"
  std::vector<std::string> granted_scopes;
};

class Connector {
 public:
  virtual ~Connector() = default;

  // Stable provider id; the second segment of every tool id this connector
  // registers (connector.<provider>.*).
  virtual std::string provider() const = 0;
  virtual std::string display_name() const = 0;
  virtual ConnectorState state() const = 0;
  virtual ConnectorAccount account() const = 0;

  // The typed tools this connector currently exposes. Every descriptor's id
  // must live under connector.<provider>.* and declare provider() as its
  // provider; the registry enforces this on registration.
  virtual std::vector<ToolDescriptor> DiscoverTools() const = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_CONNECTOR_H_
