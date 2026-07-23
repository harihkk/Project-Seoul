// Project Seoul general-purpose operator: task layer.

#include "seoul/browser/tasks/plan_types.h"

namespace seoul {

// PlanStep needs clone-based copy semantics for its move-only base::DictValue
// args member; those definitions live next to the validator in
// plan_validator.cc.

StepGuard::StepGuard() = default;
StepGuard::StepGuard(const StepGuard&) = default;
StepGuard::StepGuard(StepGuard&&) = default;
StepGuard& StepGuard::operator=(const StepGuard&) = default;
StepGuard& StepGuard::operator=(StepGuard&&) = default;
StepGuard::~StepGuard() = default;

Plan::Plan() = default;
Plan::Plan(const Plan&) = default;
Plan::Plan(Plan&&) = default;
Plan& Plan::operator=(const Plan&) = default;
Plan& Plan::operator=(Plan&&) = default;
Plan::~Plan() = default;

PlanViolation::PlanViolation() = default;
PlanViolation::PlanViolation(const PlanViolation&) = default;
PlanViolation::PlanViolation(PlanViolation&&) = default;
PlanViolation& PlanViolation::operator=(const PlanViolation&) = default;
PlanViolation& PlanViolation::operator=(PlanViolation&&) = default;
PlanViolation::~PlanViolation() = default;

}  // namespace seoul
