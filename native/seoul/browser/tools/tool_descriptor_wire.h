// Project Seoul general-purpose operator: tool layer.
// Canonical JSON wire codec for capability descriptors, defined by
// protocol/capability-descriptor.schema.json (shared with the TypeScript
// Canvas Design Lab fixture catalog). Parse consumes an untrusted document
// and produces a typed ToolDescriptor or a precise error message;
// ParseToolDescriptor(ToolDescriptorToValue(d)) reproduces `d` except for
// schema-field bounds native ignores on the wire.

#ifndef SEOUL_BROWSER_TOOLS_TOOL_DESCRIPTOR_WIRE_H_
#define SEOUL_BROWSER_TOOLS_TOOL_DESCRIPTOR_WIRE_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

// Wire-name mappings, exposed for the protocol parity gate
// (scripts/check-protocol.mjs) and conformance tests.
const char* DataSensitivityToWire(DataSensitivity sensitivity);
bool DataSensitivityFromWire(std::string_view s, DataSensitivity* out);
const char* RiskCategoryToWire(RiskCategory risk);
bool RiskCategoryFromWire(std::string_view s, RiskCategory* out);
const char* ApprovalPolicyToWire(ApprovalPolicy approval);
bool ApprovalPolicyFromWire(std::string_view s, ApprovalPolicy* out);
const char* IdempotencyClassToWire(IdempotencyClass idempotency);
bool IdempotencyClassFromWire(std::string_view s, IdempotencyClass* out);
const char* FreshnessSemanticsToWire(FreshnessSemantics freshness);
bool FreshnessSemanticsFromWire(std::string_view s, FreshnessSemantics* out);

// Serializes a ToolSchema to the bounded JSON Schema subset used on the wire
// (the same subset ToolSchemaFromJsonSchema imports).
base::DictValue ToolSchemaToJsonSchema(const ToolSchema& schema);

base::DictValue ToolDescriptorToValue(const ToolDescriptor& descriptor);

// Parses one untrusted canonical descriptor document. The returned
// descriptor's schemas satisfy IsWellFormedSchema. The error is a precise
// human-readable reason (this is a wire codec, not the registry; registry
// admission still applies its own rules).
base::expected<ToolDescriptor, std::string> ParseToolDescriptor(
    const base::Value& value);

}  // namespace seoul

#endif  // SEOUL_BROWSER_TOOLS_TOOL_DESCRIPTOR_WIRE_H_
