// Project Seoul connected tools.

#include "seoul/browser/connectors/connector.h"

namespace seoul {

ConnectorAccount::ConnectorAccount() = default;
ConnectorAccount::ConnectorAccount(const ConnectorAccount&) = default;
ConnectorAccount::ConnectorAccount(ConnectorAccount&&) = default;
ConnectorAccount& ConnectorAccount::operator=(const ConnectorAccount&) =
    default;
ConnectorAccount& ConnectorAccount::operator=(ConnectorAccount&&) = default;
ConnectorAccount::~ConnectorAccount() = default;

}  // namespace seoul
