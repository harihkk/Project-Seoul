// Project Seoul general-purpose operator: task layer.

#include "seoul/browser/tasks/plan_validator.h"

#include <set>
#include <string>

#include "seoul/browser/tools/tool_schema.h"

namespace seoul {

// Clone-based copy semantics for PlanStep (its base::DictValue args member is
// move-only). Move and destruction are member-wise.
PlanStep::PlanStep() = default;
PlanStep::PlanStep(PlanStep&&) = default;
PlanStep& PlanStep::operator=(PlanStep&&) = default;
PlanStep::~PlanStep() = default;
PlanStep::PlanStep(const PlanStep& other)
    : id(other.id),
      kind(other.kind),
      tool(other.tool),
      args(other.args.Clone()),
      prompt(other.prompt),
      requires_approval(other.requires_approval),
      guard(other.guard),
      parallel_group(other.parallel_group),
      loop_group(other.loop_group),
      max_iterations(other.max_iterations) {}
PlanStep& PlanStep::operator=(const PlanStep& other) {
  id = other.id;
  kind = other.kind;
  tool = other.tool;
  args = other.args.Clone();
  prompt = other.prompt;
  requires_approval = other.requires_approval;
  guard = other.guard;
  parallel_group = other.parallel_group;
  loop_group = other.loop_group;
  max_iterations = other.max_iterations;
  return *this;
}

namespace {

base::unexpected<PlanViolation> Violation(PlanError error,
                                          const std::string& step_id) {
  PlanViolation violation;
  violation.error = error;
  violation.step_id = step_id;
  return base::unexpected(violation);
}

bool ValidStepId(const std::string& id) {
  if (id.empty() || id.size() > 64) {
    return false;
  }
  if (id[0] < 'a' || id[0] > 'z') {
    return false;
  }
  for (char c : id) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-')) {
      return false;
    }
  }
  return true;
}

// A step needs an approval gate when the tool's policy or risk demands one:
// approval-required tools, irreversible mutations, and external side effects.
bool NeedsApproval(const ToolDescriptor& descriptor) {
  return descriptor.approval == ApprovalPolicy::kAlwaysRequired ||
         descriptor.approval == ApprovalPolicy::kFirstUsePerScope ||
         descriptor.risk == RiskCategory::kIrreversibleMutation ||
         descriptor.risk == RiskCategory::kExternalSideEffect;
}

}  // namespace

PlanValidationResult ValidatePlan(const Plan& plan,
                                  const ToolRegistry& registry,
                                  const ToolPermissionContext& context) {
  if (plan.steps.empty()) {
    return Violation(PlanError::kEmptyPlan, "");
  }
  if (plan.budgets.max_steps <= 0 || plan.budgets.max_retries_per_step < 0 ||
      plan.budgets.max_replans < 0 || plan.budgets.max_model_calls < 0 ||
      plan.budgets.max_navigations < 0 ||
      plan.budgets.max_cloud_cost_microdollars < 0 ||
      plan.budgets.max_duration <= base::TimeDelta()) {
    return Violation(PlanError::kInvalidBudgets, "");
  }
  if (plan.steps.size() > static_cast<size_t>(plan.budgets.max_steps)) {
    return Violation(PlanError::kTooManySteps, "");
  }

  std::set<std::string> all_ids;
  for (const PlanStep& step : plan.steps) {
    if (!ValidStepId(step.id)) {
      return Violation(PlanError::kInvalidStepId, step.id);
    }
    if (!all_ids.insert(step.id).second) {
      return Violation(PlanError::kDuplicateStepId, step.id);
    }
  }

  std::set<std::string> earlier_ids;

  for (const PlanStep& step : plan.steps) {
    if (step.guard.has_value()) {
      if (earlier_ids.find(step.guard->depends_on_step) == earlier_ids.end()) {
        return Violation(
            all_ids.find(step.guard->depends_on_step) != all_ids.end()
                ? PlanError::kGuardForwardReference
                : PlanError::kGuardUnknownStep,
            step.id);
      }
    }

    if (step.loop_group != 0) {
      if (step.max_iterations <= 0) {
        return Violation(PlanError::kLoopUnbounded, step.id);
      }
      if (step.max_iterations > kMaxLoopIterations) {
        return Violation(PlanError::kLoopTooLarge, step.id);
      }
    }

    switch (step.kind) {
      case PlanStepKind::kApprovalGate:
      case PlanStepKind::kUserInput: {
        if (step.prompt.empty()) {
          return Violation(PlanError::kMissingPrompt, step.id);
        }
        break;
      }
      case PlanStepKind::kToolCall: {
        const ToolDescriptor* descriptor = registry.Find(step.tool);
        if (!descriptor) {
          return Violation(PlanError::kUnknownTool, step.id);
        }
        bool permitted = false;
        for (const ToolDescriptor* available :
             registry.ListAvailable(context)) {
          if (available->id == descriptor->id) {
            permitted = true;
            break;
          }
        }
        if (!permitted) {
          return Violation(PlanError::kToolNotPermitted, step.id);
        }
        if (auto args_valid = ValidateArgs(descriptor->input_schema, step.args);
            !args_valid.has_value()) {
          return Violation(PlanError::kArgsInvalid, step.id);
        }
        if (step.parallel_group != 0 &&
            descriptor->risk != RiskCategory::kReadOnly) {
          return Violation(PlanError::kParallelMutation, step.id);
        }
        if (NeedsApproval(*descriptor)) {
          // Approval belongs to the exact capability step whose live scope is
          // resolved at execution. A generic earlier approval gate cannot
          // substitute: it is not bound to the tab, origin, destination, or
          // connector service and would bypass AgentPermissionService.
          if (!step.requires_approval) {
            return Violation(PlanError::kMissingApprovalGate, step.id);
          }
        }
        break;
      }
    }
    earlier_ids.insert(step.id);
  }
  return base::ok();
}

const char* PlanErrorToString(PlanError error) {
  switch (error) {
    case PlanError::kEmptyPlan:
      return "empty_plan";
    case PlanError::kTooManySteps:
      return "too_many_steps";
    case PlanError::kDuplicateStepId:
      return "duplicate_step_id";
    case PlanError::kInvalidStepId:
      return "invalid_step_id";
    case PlanError::kUnknownTool:
      return "unknown_tool";
    case PlanError::kToolNotPermitted:
      return "tool_not_permitted";
    case PlanError::kArgsInvalid:
      return "args_invalid";
    case PlanError::kMissingApprovalGate:
      return "missing_approval_gate";
    case PlanError::kParallelMutation:
      return "parallel_mutation";
    case PlanError::kLoopUnbounded:
      return "loop_unbounded";
    case PlanError::kLoopTooLarge:
      return "loop_too_large";
    case PlanError::kGuardUnknownStep:
      return "guard_unknown_step";
    case PlanError::kGuardForwardReference:
      return "guard_forward_reference";
    case PlanError::kInvalidBudgets:
      return "invalid_budgets";
    case PlanError::kMissingPrompt:
      return "missing_prompt";
  }
  return "empty_plan";
}

}  // namespace seoul
