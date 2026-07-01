// Project Seoul general-purpose operator: task layer.
// Typed plans. A plan is an ordered, bounded sequence of steps over registered
// tools, with explicit approval gates, user-input points, guards on earlier
// outcomes, bounded loops, and read-only parallel groups. There is no step
// kind that executes arbitrary shell commands or generated code.

#ifndef SEOUL_BROWSER_TASKS_PLAN_TYPES_H_
#define SEOUL_BROWSER_TASKS_PLAN_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "seoul/browser/tasks/task_types.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

enum class PlanStepKind {
  kToolCall,
  kApprovalGate,  // explicit user approval before the following steps
  kUserInput,     // collect typed/form input before continuing
};

// A guard makes a step conditional on an earlier step's outcome.
struct StepGuard {
  std::string depends_on_step;
  bool require_success = true;  // false: run only if the dependency failed

  friend bool operator==(const StepGuard&, const StepGuard&) = default;
};

// base::Value::Dict is move-only; PlanStep holds one by value and declares
// clone-based copy semantics so a Plan can be copied (the task constructor and
// checkpoint restore take a Plan and the workflow compiler builds one).
struct PlanStep {
  PlanStep();
  PlanStep(const PlanStep&);
  PlanStep(PlanStep&&);
  PlanStep& operator=(const PlanStep&);
  PlanStep& operator=(PlanStep&&);
  ~PlanStep();

  std::string id;  // [a-z][a-z0-9_-]{0,63}, unique within the plan
  PlanStepKind kind = PlanStepKind::kToolCall;
  ToolId tool;             // kToolCall
  base::Value::Dict args;  // kToolCall; validated against the tool schema
  std::string prompt;      // kApprovalGate/kUserInput: what to ask
  bool requires_approval = false;  // per-step gate in addition to policy
  std::optional<StepGuard> guard;
  // Read-only steps sharing a nonzero group may run concurrently. Mutating
  // tools must use group 0 (sequential).
  int parallel_group = 0;
  // Bounded loops: steps sharing a nonzero loop_group repeat together up to
  // max_iterations times. Zero means no loop.
  int loop_group = 0;
  int max_iterations = 0;
};

struct Plan {
  std::string goal;  // the user's outcome statement, for display
  std::vector<PlanStep> steps;
  TaskBudgets budgets;
};

enum class PlanError {
  kEmptyPlan,
  kTooManySteps,
  kDuplicateStepId,
  kInvalidStepId,
  kUnknownTool,
  kToolNotPermitted,
  kArgsInvalid,
  kMissingApprovalGate,
  kParallelMutation,
  kLoopUnbounded,
  kLoopTooLarge,
  kGuardUnknownStep,
  kGuardForwardReference,
  kInvalidBudgets,
  kMissingPrompt,
};

const char* PlanErrorToString(PlanError error);

struct PlanViolation {
  PlanError error = PlanError::kEmptyPlan;
  std::string step_id;  // empty for plan-level violations

  friend bool operator==(const PlanViolation&, const PlanViolation&) = default;
};

using PlanValidationResult = base::expected<void, PlanViolation>;

inline constexpr int kMaxLoopIterations = 25;

}  // namespace seoul

#endif  // SEOUL_BROWSER_TASKS_PLAN_TYPES_H_
