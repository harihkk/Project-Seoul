// Project Seoul general-purpose operator: task layer.
// Plan validation against the tool registry and a permission context. A plan
// executes only if every tool call resolves to a registered permitted tool
// with schema-valid arguments, risky steps are approval-gated, loops are
// bounded, and parallel groups are read-only.

#ifndef SEOUL_BROWSER_TASKS_PLAN_VALIDATOR_H_
#define SEOUL_BROWSER_TASKS_PLAN_VALIDATOR_H_

#include "seoul/browser/tasks/plan_types.h"
#include "seoul/browser/tools/tool_registry.h"

namespace seoul {

PlanValidationResult ValidatePlan(const Plan& plan,
                                  const ToolRegistry& registry,
                                  const ToolPermissionContext& context);

}  // namespace seoul

#endif  // SEOUL_BROWSER_TASKS_PLAN_VALIDATOR_H_
