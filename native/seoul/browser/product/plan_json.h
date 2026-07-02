// Project Seoul product runtime: plan serialization.
// A bounded JSON codec for typed plans. Used to parse a reasoning provider's
// structured plan output (untrusted; strictly validated) and to persist plans
// inside saved workflows and task checkpoints. There is no step kind that
// carries code; unknown keys and unknown step kinds are rejected, not skipped.

#ifndef SEOUL_BROWSER_PRODUCT_PLAN_JSON_H_
#define SEOUL_BROWSER_PRODUCT_PLAN_JSON_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "seoul/browser/tasks/plan_types.h"

namespace seoul {

base::Value::Dict PlanToValue(const Plan& plan);

// Parses an untrusted plan value. Structural failures return nullopt; the
// result must still pass ValidatePlan against the permitted capability set
// before any execution.
std::optional<Plan> PlanFromValue(const base::Value::Dict& value);

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_PLAN_JSON_H_
