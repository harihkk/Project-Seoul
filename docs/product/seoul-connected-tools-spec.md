# Seoul Connected Tools Specification

Status: Current compile and runtime evidence is maintained in the product
readiness report.

Connected tools let Seoul reach explicitly connected external services (mail,
calendar, files) through the same Tool Registry the general planner uses. This
spec describes the source in `native/seoul/browser/connectors/` and the ownership
enforcement in `native/seoul/browser/tools/tool_registry.cc`. A connector never
injects native browser actions and never assumes a specific protocol.

## Connector seam

`native/seoul/browser/connectors/connector.h` defines the `Connector` interface:
a stable `provider()` id (the second segment of every tool id it registers), a
`display_name`, a `ConnectorState` (`kDisconnected`, `kConnecting`, `kConnected`,
`kError`), a `ConnectorAccount`, and `DiscoverTools()` returning typed
`ToolDescriptor`s. `ConnectorAccount` holds an `account_label` and
`granted_scopes` and, per the header, never holds tokens or credentials; those
live in the platform secure store outside this module.

## Mirroring connector tools into the shared registry

`native/seoul/browser/connectors/connector_registry.cc` mirrors a connector's
tools into the shared `ToolRegistry`. `RegisterConnectorTools` calls
`DiscoverTools`, forces each descriptor's `provider` to match the connector, and
registers each one. Once connected, the planner sees these tools through the same
`ToolRegistry::ListAvailable` path as builtins, subject to the permission
context.

## Namespace ownership enforcement

`ToolRegistry::Register` enforces ownership so an external tool server cannot
shadow a native browser action. Seoul-owned roots ("browser", "page", "info",
"canvas", "files", "workflow", "task", "scene") accept only provider "seoul"; any
other provider under those roots is `kReservedNamespace`. A connector tool must
live under `connector.<provider>.<tool...>`, and the second segment must equal
the registering provider, so one connector cannot impersonate another
(`kProviderMismatch` otherwise). Any root that is neither a Seoul namespace nor
"connector" is rejected. `ToolId` itself requires two to four
`[a-z][a-z0-9_]*` segments.

## Atomic connect with rollback

`ConnectorRegistry::Connect` rejects a null connector and rejects the reserved
provider "seoul" (`kReservedProvider`) and a duplicate provider
(`kDuplicateProvider`). It then registers the connector's tools; if any single
tool fails registration (bad namespace, duplicate id), `RegisterConnectorTools`
calls `UnregisterProvider` to roll back every tool already registered for that
provider and returns `kToolRegistrationFailed`, so no partial tool set is ever
exposed. Only after all tools register does the connector join the registry.

## Disconnect removes exactly that provider's tools

`Disconnect` calls `ToolRegistry::UnregisterProvider(provider)`, which removes
exactly the tools whose `provider` field matches, then drops the connector.
`Refresh` re-discovers a connected provider's tools by unregistering and
re-registering them (for example after a scope change). Because removal is keyed
on the provider field, disconnecting one provider never affects another's tools
or the builtins.

## MCP compatibility, not dependency

The connector header states an MCP adapter is one possible connector
implementation, not a dependency: the seam is protocol-neutral. Any transport
that can present typed `ToolDescriptor`s under its own
`connector.<provider>.*` namespace is a valid connector.

## An external tool server cannot inject native actions

Because the reserved Seoul roots accept only provider "seoul", and every
connector descriptor is forced to its own provider and confined to its
`connector.<provider>.*` namespace, a connected external tool server can expose
its own service tools but can never register or overwrite a `browser.*`,
`page.*`, `task.*`, or other native tool. The registry, not the connector,
enforces this boundary at registration time.
