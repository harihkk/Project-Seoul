// Project Seoul connected tools.

#include "seoul/browser/connectors/fake_connector.h"

#include <utility>

namespace seoul {

FakeConnector::FakeConnector(std::string provider, std::string account_label)
    : provider_(std::move(provider)),
      account_label_(std::move(account_label)) {}

FakeConnector::~FakeConnector() = default;

std::string FakeConnector::provider() const {
  return provider_;
}

std::string FakeConnector::display_name() const {
  return provider_ + " (fake)";
}

ConnectorState FakeConnector::state() const {
  return state_;
}

ConnectorAccount FakeConnector::account() const {
  ConnectorAccount account;
  account.account_label = account_label_;
  account.granted_scopes = {"read"};
  return account;
}

void FakeConnector::AddTool(const std::string& tool_suffix, bool well_formed) {
  tool_specs_.emplace_back(tool_suffix, well_formed);
}

std::vector<ToolDescriptor> FakeConnector::DiscoverTools() const {
  std::vector<ToolDescriptor> tools;
  for (const auto& [suffix, well_formed] : tool_specs_) {
    ToolDescriptor descriptor;
    descriptor.id = ToolId::FromString("connector." + provider_ + "." + suffix);
    descriptor.name = suffix;
    descriptor.description = "fake connector tool";
    descriptor.provider = provider_;
    descriptor.sensitivity = DataSensitivity::kPersonal;
    descriptor.requires_network = true;
    descriptor.observation_contract = "fake result";
    if (well_formed) {
      SchemaField query;
      query.name = "query";
      query.kind = SchemaFieldKind::kString;
      descriptor.input_schema.fields.push_back(query);
    } else {
      // Duplicate field names make the schema malformed -> registration
      // rejects it, exercising connect rollback.
      SchemaField a;
      a.name = "dup";
      a.kind = SchemaFieldKind::kString;
      descriptor.input_schema.fields.push_back(a);
      descriptor.input_schema.fields.push_back(a);
    }
    tools.push_back(std::move(descriptor));
  }
  return tools;
}

}  // namespace seoul
