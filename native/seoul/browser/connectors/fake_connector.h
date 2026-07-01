// Project Seoul connected tools.
// A deterministic fake connector for tests: exposes a configurable set of
// typed tools under its provider namespace. Test support only.

#ifndef SEOUL_BROWSER_CONNECTORS_FAKE_CONNECTOR_H_
#define SEOUL_BROWSER_CONNECTORS_FAKE_CONNECTOR_H_

#include <string>
#include <vector>

#include "seoul/browser/connectors/connector.h"

namespace seoul {

class FakeConnector : public Connector {
 public:
  FakeConnector(std::string provider, std::string account_label);
  ~FakeConnector() override;

  std::string provider() const override;
  std::string display_name() const override;
  ConnectorState state() const override;
  ConnectorAccount account() const override;
  std::vector<ToolDescriptor> DiscoverTools() const override;

  // Adds a tool the connector will advertise. `tool_suffix` is appended to
  // connector.<provider>.; `well_formed` false intentionally produces an
  // invalid descriptor to exercise rollback.
  void AddTool(const std::string& tool_suffix, bool well_formed = true);
  void set_state(ConnectorState state) { state_ = state; }

 private:
  std::string provider_;
  std::string account_label_;
  ConnectorState state_ = ConnectorState::kConnected;
  std::vector<std::pair<std::string, bool>> tool_specs_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_FAKE_CONNECTOR_H_
