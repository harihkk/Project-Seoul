// Project Seoul general-purpose operator: tool layer.

#include "seoul/browser/tools/tool_types.h"

namespace seoul {

ToolDescriptor::ToolDescriptor() = default;
ToolDescriptor::ToolDescriptor(const ToolDescriptor&) = default;
ToolDescriptor::ToolDescriptor(ToolDescriptor&&) = default;
ToolDescriptor& ToolDescriptor::operator=(const ToolDescriptor&) = default;
ToolDescriptor& ToolDescriptor::operator=(ToolDescriptor&&) = default;
ToolDescriptor::~ToolDescriptor() = default;

}  // namespace seoul
