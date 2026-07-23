// Project Seoul general-purpose operator: task layer.
// Bounded execution of one validated plan: offer a step, run it, observe the
// real result, verify it, and decide (continue / retry / replan / ask the
// user / stop). A mutation whose outcome is unknown is never retried
// automatically. Execution state checkpoints to a value and restores, so a
// task can continue after a browser restart.

#ifndef SEOUL_BROWSER_TASKS_TASK_EXECUTION_H_
#define SEOUL_BROWSER_TASKS_TASK_EXECUTION_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/values.h"
#include "seoul/browser/tasks/plan_types.h"
#include "seoul/browser/tasks/task_types.h"

namespace seoul {

// Resolves a tool id to its descriptor (registry lookup); execution needs
// risk and idempotency for the retry rules.
using ToolResolver =
    base::RepeatingCallback<const ToolDescriptor*(const ToolId&)>;

// What the caller should do after recording a step outcome.
enum class ExecutionDecision {
  kContinue,
  kRetryStep,
  kReplan,
  kAskUser,
  kStop,
};

const char* ExecutionDecisionToString(ExecutionDecision decision);

// What the caller should do next when asking for work.
struct NextAction {
  enum class Kind {
    kRunStep,
    kAwaitApproval,
    kAwaitInput,
    kCompleted,
    kStopped,
  };

  NextAction();
  NextAction(const NextAction&);
  NextAction(NextAction&&);
  NextAction& operator=(const NextAction&);
  NextAction& operator=(NextAction&&);
  ~NextAction();

  Kind kind = Kind::kCompleted;
  std::string step_id;  // set for kRunStep/kAwaitApproval/kAwaitInput
  TaskFailureReason failure = TaskFailureReason::kNone;  // set for kStopped
};

// The observed, verified result of one step run.
struct StepOutcome {
  StepOutcome();
  StepOutcome(const StepOutcome&);
  StepOutcome(StepOutcome&&);
  StepOutcome& operator=(const StepOutcome&);
  StepOutcome& operator=(StepOutcome&&);
  ~StepOutcome();

  StepStatus status = StepStatus::kSucceeded;
  std::string observed_summary;
  VerificationRecord verification;
  ExecutionRoute route = ExecutionRoute::kDeterministic;
  int64_t cost_microdollars = 0;
  int model_calls = 0;
  int navigations = 0;
};

struct BudgetUsage {
  int steps_executed = 0;
  int model_calls = 0;
  int navigations = 0;
  int64_t cloud_cost_microdollars = 0;
  int replans_used = 0;
};

class TaskExecution {
 public:
  // `plan` must already have passed ValidatePlan.
  TaskExecution(TaskId task_id,
                Plan plan,
                ToolResolver resolve_tool,
                base::RepeatingCallback<base::Time()> clock);
  TaskExecution(const TaskExecution&) = delete;
  TaskExecution& operator=(const TaskExecution&) = delete;
  ~TaskExecution();

  const TaskId& task_id() const { return task_id_; }
  TaskState state() const { return state_; }
  TaskFailureReason failure_reason() const { return failure_reason_; }
  const Plan& plan() const { return plan_; }
  const std::vector<ActionReceipt>& receipts() const { return receipts_; }
  const BudgetUsage& usage() const { return usage_; }
  StepStatus step_status(const std::string& step_id) const;

  // kDraft -> kExecuting. Stamps the start time for the duration budget.
  NextAction Start();
  // The next thing to do given guards, loops, approvals, and prior outcomes.
  NextAction Advance();
  // Marks an offered tool step running. Fails (kStopped is never returned
  // here; the bool is false) when the step is not currently eligible.
  bool BeginStep(const std::string& step_id);
  // Approval response for an approval gate or a requires_approval step.
  // Rejection skips the gated step and continues: "run this but stop before
  // submitting" completes the rest of the plan.
  NextAction RecordApproval(const std::string& step_id, bool approved);
  // Typed user input for a kUserInput step.
  NextAction RecordUserInput(const std::string& step_id,
                             base::DictValue input);
  // Observation + verification for a running step; returns the decision.
  ExecutionDecision RecordStepOutcome(const std::string& step_id,
                                      const StepOutcome& outcome);

  NextAction Pause();
  NextAction ResumeFromPause();
  NextAction Cancel();

  // Serialized execution state (step statuses, retries, loop iterations,
  // usage). Restore continues a task after restart with the same plan.
  base::DictValue Checkpoint() const;
  static std::unique_ptr<TaskExecution> RestoreFromCheckpoint(
      TaskId task_id,
      Plan plan,
      ToolResolver resolve_tool,
      base::RepeatingCallback<base::Time()> clock,
      const base::DictValue& checkpoint);

 private:
  struct StepState {
    StepStatus status = StepStatus::kPending;
    int retries = 0;
    bool approved = false;
    bool approval_requested = false;
  };

  const PlanStep* FindStep(const std::string& step_id) const;
  StepState* FindState(const std::string& step_id);
  bool GuardSatisfied(const PlanStep& step, bool* should_skip);
  bool BudgetsExceeded(TaskFailureReason* reason) const;
  void CompleteIfDone();
  NextAction Stopped(TaskFailureReason reason);

  TaskId task_id_;
  Plan plan_;
  ToolResolver resolve_tool_;
  base::RepeatingCallback<base::Time()> clock_;
  TaskState state_ = TaskState::kDraft;
  TaskFailureReason failure_reason_ = TaskFailureReason::kNone;
  base::Time started_at_;
  std::map<std::string, StepState> step_states_;
  std::map<int, int> loop_iterations_;  // loop_group -> completed iterations
  std::map<std::string, int> action_repeat_counts_;  // tool+args -> runs
  std::vector<ActionReceipt> receipts_;
  BudgetUsage usage_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_TASKS_TASK_EXECUTION_H_
