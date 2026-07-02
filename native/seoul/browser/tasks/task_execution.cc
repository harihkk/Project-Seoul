// Project Seoul general-purpose operator: task layer.

#include "seoul/browser/tasks/task_execution.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"

namespace seoul {

namespace {

bool IsTerminalStep(StepStatus status) {
  switch (status) {
    case StepStatus::kSucceeded:
    case StepStatus::kFailed:
    case StepStatus::kOutcomeUnknown:
    case StepStatus::kSkipped:
    case StepStatus::kCancelled:
      return true;
    case StepStatus::kPending:
    case StepStatus::kAwaitingApproval:
    case StepStatus::kAwaitingInput:
    case StepStatus::kRunning:
      return false;
  }
  return false;
}

std::string RepeatKey(const PlanStep& step) {
  std::string serialized_args;
  base::JSONWriter::Write(step.args, &serialized_args);
  return step.tool.value() + "|" + serialized_args;
}

}  // namespace

TaskExecution::TaskExecution(TaskId task_id,
                             Plan plan,
                             ToolResolver resolve_tool,
                             base::RepeatingCallback<base::Time()> clock)
    : task_id_(task_id),
      plan_(std::move(plan)),
      resolve_tool_(std::move(resolve_tool)),
      clock_(std::move(clock)) {
  for (const PlanStep& step : plan_.steps) {
    step_states_[step.id] = StepState();
  }
}

TaskExecution::~TaskExecution() = default;

const PlanStep* TaskExecution::FindStep(const std::string& step_id) const {
  for (const PlanStep& step : plan_.steps) {
    if (step.id == step_id) {
      return &step;
    }
  }
  return nullptr;
}

TaskExecution::StepState* TaskExecution::FindState(const std::string& step_id) {
  auto it = step_states_.find(step_id);
  return it == step_states_.end() ? nullptr : &it->second;
}

StepStatus TaskExecution::step_status(const std::string& step_id) const {
  auto it = step_states_.find(step_id);
  return it == step_states_.end() ? StepStatus::kPending : it->second.status;
}

bool TaskExecution::GuardSatisfied(const PlanStep& step, bool* should_skip) {
  *should_skip = false;
  if (!step.guard.has_value()) {
    return true;
  }
  const StepStatus dependency = step_status(step.guard->depends_on_step);
  if (!IsTerminalStep(dependency)) {
    return false;  // dependency still running; not eligible yet
  }
  const bool dependency_succeeded = dependency == StepStatus::kSucceeded;
  if (step.guard->require_success != dependency_succeeded) {
    *should_skip = true;
  }
  return true;
}

bool TaskExecution::BudgetsExceeded(TaskFailureReason* reason) const {
  const TaskBudgets& budgets = plan_.budgets;
  if (usage_.steps_executed > budgets.max_steps ||
      usage_.model_calls > budgets.max_model_calls ||
      usage_.navigations > budgets.max_navigations ||
      usage_.cloud_cost_microdollars > budgets.max_cloud_cost_microdollars) {
    *reason = TaskFailureReason::kBudgetExhausted;
    return true;
  }
  if (!started_at_.is_null() &&
      clock_.Run() - started_at_ > budgets.max_duration) {
    *reason = TaskFailureReason::kBudgetExhausted;
    return true;
  }
  for (const auto& [key, count] : action_repeat_counts_) {
    if (count > budgets.max_repeat_actions) {
      *reason = TaskFailureReason::kBudgetExhausted;
      return true;
    }
  }
  return false;
}

NextAction TaskExecution::Stopped(TaskFailureReason reason) {
  state_ = reason == TaskFailureReason::kUserStopped ? TaskState::kCancelled
                                                     : TaskState::kFailed;
  failure_reason_ = reason;
  NextAction action;
  action.kind = NextAction::Kind::kStopped;
  action.failure = reason;
  return action;
}

void TaskExecution::CompleteIfDone() {
  for (const PlanStep& step : plan_.steps) {
    if (!IsTerminalStep(step_status(step.id))) {
      return;
    }
    // Loop groups with remaining iterations are not done.
    if (step.loop_group != 0 &&
        step_status(step.id) == StepStatus::kSucceeded &&
        loop_iterations_[step.loop_group] + 1 < step.max_iterations) {
      return;
    }
  }
  // Everything is terminal. The task completes only when every step either
  // succeeded, was skipped, or failed with a successfully executed
  // failure-handler (a dependent step guarded on this step's failure).
  for (const PlanStep& step : plan_.steps) {
    const StepStatus status = step_status(step.id);
    if (status == StepStatus::kOutcomeUnknown ||
        status == StepStatus::kCancelled) {
      state_ = TaskState::kFailed;
      failure_reason_ = TaskFailureReason::kStepFailed;
      return;
    }
    if (status != StepStatus::kFailed) {
      continue;
    }
    bool handled = false;
    for (const PlanStep& handler : plan_.steps) {
      if (handler.guard.has_value() &&
          handler.guard->depends_on_step == step.id &&
          !handler.guard->require_success &&
          step_status(handler.id) == StepStatus::kSucceeded) {
        handled = true;
        break;
      }
    }
    if (!handled) {
      state_ = TaskState::kFailed;
      failure_reason_ = TaskFailureReason::kStepFailed;
      return;
    }
  }
  state_ = TaskState::kCompleted;
}

NextAction TaskExecution::Start() {
  if (state_ != TaskState::kDraft) {
    NextAction action;
    action.kind = NextAction::Kind::kStopped;
    action.failure = failure_reason_;
    return action;
  }
  state_ = TaskState::kExecuting;
  started_at_ = clock_.Run();
  return Advance();
}

NextAction TaskExecution::Advance() {
  if (state_ == TaskState::kCompleted) {
    NextAction action;
    action.kind = NextAction::Kind::kCompleted;
    return action;
  }
  if (state_ != TaskState::kExecuting) {
    NextAction action;
    action.kind = NextAction::Kind::kStopped;
    action.failure = failure_reason_;
    return action;
  }
  TaskFailureReason budget_reason;
  if (BudgetsExceeded(&budget_reason)) {
    return Stopped(budget_reason);
  }

  for (const PlanStep& step : plan_.steps) {
    StepState* step_state = FindState(step.id);
    // Restart completed loop-group steps when iterations remain and every
    // step in the group is terminal without failure.
    if (step.loop_group != 0 && step_state->status == StepStatus::kSucceeded &&
        loop_iterations_[step.loop_group] + 1 < step.max_iterations) {
      bool group_done = true;
      bool group_failed = false;
      for (const PlanStep& other : plan_.steps) {
        if (other.loop_group != step.loop_group) {
          continue;
        }
        const StepStatus status = step_status(other.id);
        if (!IsTerminalStep(status)) {
          group_done = false;
        }
        if (status == StepStatus::kFailed ||
            status == StepStatus::kOutcomeUnknown ||
            status == StepStatus::kCancelled) {
          group_failed = true;
        }
      }
      if (group_done && !group_failed) {
        ++loop_iterations_[step.loop_group];
        for (const PlanStep& other : plan_.steps) {
          if (other.loop_group == step.loop_group) {
            FindState(other.id)->status = StepStatus::kPending;
          }
        }
      }
    }

    if (step_state->status != StepStatus::kPending) {
      continue;
    }
    bool should_skip = false;
    if (!GuardSatisfied(step, &should_skip)) {
      continue;
    }
    if (should_skip) {
      step_state->status = StepStatus::kSkipped;
      continue;
    }

    NextAction action;
    action.step_id = step.id;
    switch (step.kind) {
      case PlanStepKind::kApprovalGate:
        step_state->status = StepStatus::kAwaitingApproval;
        step_state->approval_requested = true;
        action.kind = NextAction::Kind::kAwaitApproval;
        return action;
      case PlanStepKind::kUserInput:
        step_state->status = StepStatus::kAwaitingInput;
        action.kind = NextAction::Kind::kAwaitInput;
        return action;
      case PlanStepKind::kToolCall:
        if (step.requires_approval && !step_state->approved) {
          step_state->status = StepStatus::kAwaitingApproval;
          step_state->approval_requested = true;
          action.kind = NextAction::Kind::kAwaitApproval;
          return action;
        }
        action.kind = NextAction::Kind::kRunStep;
        return action;
    }
  }

  CompleteIfDone();
  NextAction action;
  if (state_ == TaskState::kCompleted) {
    action.kind = NextAction::Kind::kCompleted;
    return action;
  }
  if (state_ == TaskState::kFailed) {
    action.kind = NextAction::Kind::kStopped;
    action.failure = failure_reason_;
    return action;
  }
  // Nothing runnable but not everything terminal: blocked on an unmet guard
  // chain; report it as an invalid assumption rather than spinning.
  return Stopped(TaskFailureReason::kAssumptionInvalid);
}

bool TaskExecution::BeginStep(const std::string& step_id) {
  if (state_ != TaskState::kExecuting) {
    return false;
  }
  const PlanStep* step = FindStep(step_id);
  StepState* step_state = FindState(step_id);
  if (!step || !step_state || step->kind != PlanStepKind::kToolCall) {
    return false;
  }
  if (step_state->status != StepStatus::kPending ||
      (step->requires_approval && !step_state->approved)) {
    return false;
  }
  bool should_skip = false;
  if (!GuardSatisfied(*step, &should_skip) || should_skip) {
    return false;
  }
  step_state->status = StepStatus::kRunning;
  ++usage_.steps_executed;
  // The repeat-action guard catches a planner that re-runs the same action
  // unintentionally. A bounded loop repeats the same tool+args by design and
  // is already capped by its max_iterations, so loop-group steps are excluded
  // here; otherwise a legitimate loop longer than max_repeat_actions would be
  // killed as if it were runaway repetition.
  if (step->loop_group == 0) {
    ++action_repeat_counts_[RepeatKey(*step)];
  }
  return true;
}

NextAction TaskExecution::RecordApproval(const std::string& step_id,
                                         bool approved) {
  const PlanStep* step = FindStep(step_id);
  StepState* step_state = FindState(step_id);
  if (!step || !step_state ||
      step_state->status != StepStatus::kAwaitingApproval) {
    return Advance();
  }
  if (step->kind == PlanStepKind::kApprovalGate) {
    step_state->status =
        approved ? StepStatus::kSucceeded : StepStatus::kSkipped;
    if (!approved) {
      // A rejected gate is a stop boundary: everything after it is skipped
      // and the work already done is kept ("prepare but do not submit").
      bool after_gate = false;
      for (const PlanStep& later : plan_.steps) {
        if (later.id == step->id) {
          after_gate = true;
          continue;
        }
        if (!after_gate) {
          continue;
        }
        StepState* later_state = FindState(later.id);
        if (later_state && !IsTerminalStep(later_state->status)) {
          later_state->status = StepStatus::kSkipped;
        }
      }
    }
  } else {
    if (approved) {
      step_state->approved = true;
      step_state->status = StepStatus::kPending;
    } else {
      // The user declined this action; the step is skipped, the task
      // continues (prepare-but-do-not-submit).
      step_state->status = StepStatus::kSkipped;
    }
  }
  return Advance();
}

NextAction TaskExecution::RecordUserInput(const std::string& step_id,
                                          base::Value::Dict input) {
  const PlanStep* step = FindStep(step_id);
  StepState* step_state = FindState(step_id);
  if (!step || !step_state ||
      step_state->status != StepStatus::kAwaitingInput) {
    return Advance();
  }
  step_state->status = StepStatus::kSucceeded;
  ActionReceipt receipt;
  receipt.step_id = step_id;
  receipt.status = StepStatus::kSucceeded;
  receipt.started_at = clock_.Run();
  receipt.finished_at = receipt.started_at;
  std::string serialized;
  base::JSONWriter::Write(input, &serialized);
  receipt.observed_summary = "user input collected";
  receipt.route = ExecutionRoute::kDeterministic;
  if (receipts_.size() < kMaxReceiptsPerTask) {
    receipts_.push_back(std::move(receipt));
  }
  return Advance();
}

ExecutionDecision TaskExecution::RecordStepOutcome(const std::string& step_id,
                                                   const StepOutcome& outcome) {
  const PlanStep* step = FindStep(step_id);
  StepState* step_state = FindState(step_id);
  if (!step || !step_state || step_state->status != StepStatus::kRunning ||
      state_ != TaskState::kExecuting) {
    return ExecutionDecision::kStop;
  }

  usage_.model_calls += outcome.model_calls;
  usage_.navigations += outcome.navigations;
  usage_.cloud_cost_microdollars += outcome.cost_microdollars;

  // A "success" whose explicit verification failed is a failure: Seoul
  // confirms actual results instead of trusting dispatch.
  StepStatus effective = outcome.status;
  if (effective == StepStatus::kSucceeded &&
      !outcome.verification.method.empty() && !outcome.verification.verified) {
    effective = StepStatus::kFailed;
  }
  step_state->status = effective;

  ActionReceipt receipt;
  receipt.step_id = step_id;
  receipt.tool = step->tool;
  receipt.status = effective;
  receipt.started_at = clock_.Run();
  receipt.finished_at = receipt.started_at;
  receipt.observed_summary = outcome.observed_summary;
  receipt.verification = outcome.verification;
  receipt.route = outcome.route;
  receipt.cost_microdollars = outcome.cost_microdollars;
  receipt.model_calls = outcome.model_calls;
  receipt.navigations = outcome.navigations;
  if (receipts_.size() < kMaxReceiptsPerTask) {
    receipts_.push_back(std::move(receipt));
  }

  TaskFailureReason budget_reason;
  if (BudgetsExceeded(&budget_reason)) {
    Stopped(budget_reason);
    return ExecutionDecision::kStop;
  }

  if (effective == StepStatus::kSucceeded) {
    CompleteIfDone();
    return state_ == TaskState::kFailed ? ExecutionDecision::kStop
                                        : ExecutionDecision::kContinue;
  }

  const ToolDescriptor* descriptor =
      resolve_tool_ ? resolve_tool_.Run(step->tool) : nullptr;
  const bool read_only =
      descriptor && descriptor->risk == RiskCategory::kReadOnly;
  const bool idempotent =
      descriptor && descriptor->idempotency == IdempotencyClass::kIdempotent;

  if (effective == StepStatus::kOutcomeUnknown) {
    if (!read_only) {
      // The mutation may or may not have happened. Retrying could duplicate
      // it; only the user can decide.
      state_ = TaskState::kPaused;
      return ExecutionDecision::kAskUser;
    }
    if (step_state->retries < plan_.budgets.max_retries_per_step) {
      ++step_state->retries;
      step_state->status = StepStatus::kPending;
      return ExecutionDecision::kRetryStep;
    }
  }

  if (effective == StepStatus::kFailed && (read_only || idempotent) &&
      step_state->retries < plan_.budgets.max_retries_per_step) {
    ++step_state->retries;
    step_state->status = StepStatus::kPending;
    return ExecutionDecision::kRetryStep;
  }

  if (usage_.replans_used < plan_.budgets.max_replans) {
    // A replan is a bounded re-attempt with a revised approach. The step
    // returns to pending; the caller revises its arguments (or swaps in a
    // revised plan seeded from Checkpoint()) before running it again.
    ++usage_.replans_used;
    step_state->status = StepStatus::kPending;
    return ExecutionDecision::kReplan;
  }
  state_ = TaskState::kPaused;
  return ExecutionDecision::kAskUser;
}

NextAction TaskExecution::Pause() {
  if (state_ == TaskState::kExecuting) {
    state_ = TaskState::kPaused;
  }
  NextAction action;
  action.kind = NextAction::Kind::kStopped;
  action.failure = TaskFailureReason::kNone;
  return action;
}

NextAction TaskExecution::ResumeFromPause() {
  if (state_ == TaskState::kPaused) {
    state_ = TaskState::kExecuting;
  }
  return Advance();
}

NextAction TaskExecution::Cancel() {
  if (state_ == TaskState::kCompleted || state_ == TaskState::kCancelled) {
    NextAction action;
    action.kind = NextAction::Kind::kStopped;
    action.failure = failure_reason_;
    return action;
  }
  for (auto& [id, step_state] : step_states_) {
    if (!IsTerminalStep(step_state.status)) {
      step_state.status = StepStatus::kCancelled;
    }
  }
  return Stopped(TaskFailureReason::kUserStopped);
}

base::Value::Dict TaskExecution::Checkpoint() const {
  base::Value::Dict checkpoint;
  checkpoint.Set("task_id", task_id_.value());
  checkpoint.Set("state", TaskStateToString(state_));
  base::Value::Dict steps;
  for (const auto& [id, step_state] : step_states_) {
    base::Value::Dict entry;
    entry.Set("status", StepStatusToString(step_state.status));
    entry.Set("retries", step_state.retries);
    entry.Set("approved", step_state.approved);
    steps.Set(id, std::move(entry));
  }
  checkpoint.Set("steps", std::move(steps));
  base::Value::Dict loops;
  for (const auto& [group, iterations] : loop_iterations_) {
    loops.Set(std::to_string(group), iterations);
  }
  checkpoint.Set("loops", std::move(loops));
  base::Value::Dict usage;
  usage.Set("steps_executed", usage_.steps_executed);
  usage.Set("model_calls", usage_.model_calls);
  usage.Set("navigations", usage_.navigations);
  usage.Set("cloud_cost_microdollars",
            static_cast<double>(usage_.cloud_cost_microdollars));
  usage.Set("replans_used", usage_.replans_used);
  checkpoint.Set("usage", std::move(usage));
  return checkpoint;
}

// static
std::unique_ptr<TaskExecution> TaskExecution::RestoreFromCheckpoint(
    TaskId task_id,
    Plan plan,
    ToolResolver resolve_tool,
    base::RepeatingCallback<base::Time()> clock,
    const base::Value::Dict& checkpoint) {
  const std::string* stored_id = checkpoint.FindString("task_id");
  if (!stored_id || *stored_id != task_id.value()) {
    return nullptr;
  }
  auto execution = std::make_unique<TaskExecution>(
      task_id, std::move(plan), std::move(resolve_tool), std::move(clock));
  const base::Value::Dict* steps = checkpoint.FindDict("steps");
  if (!steps) {
    return nullptr;
  }
  auto parse_status = [](const std::string& s, StepStatus* out) {
    static constexpr std::pair<const char*, StepStatus> kStatuses[] = {
        {"pending", StepStatus::kPending},
        {"awaiting_approval", StepStatus::kAwaitingApproval},
        {"awaiting_input", StepStatus::kAwaitingInput},
        {"running", StepStatus::kRunning},
        {"succeeded", StepStatus::kSucceeded},
        {"failed", StepStatus::kFailed},
        {"outcome_unknown", StepStatus::kOutcomeUnknown},
        {"skipped", StepStatus::kSkipped},
        {"cancelled", StepStatus::kCancelled},
    };
    for (const auto& [name, status] : kStatuses) {
      if (s == name) {
        *out = status;
        return true;
      }
    }
    return false;
  };
  for (auto& [id, step_state] : execution->step_states_) {
    const base::Value::Dict* entry = steps->FindDict(id);
    if (!entry) {
      return nullptr;  // checkpoint does not match this plan
    }
    const std::string* status = entry->FindString("status");
    StepStatus parsed;
    if (!status || !parse_status(*status, &parsed)) {
      return nullptr;
    }
    // A step that was mid-run when the browser stopped has an unknown
    // outcome; it must not silently re-run.
    step_state.status =
        parsed == StepStatus::kRunning ? StepStatus::kOutcomeUnknown : parsed;
    step_state.retries = entry->FindInt("retries").value_or(0);
    step_state.approved = entry->FindBool("approved").value_or(false);
  }
  if (const base::Value::Dict* loops = checkpoint.FindDict("loops")) {
    for (const auto [group, iterations] : *loops) {
      int group_number = 0;
      if (base::StringToInt(group, &group_number) && iterations.is_int()) {
        execution->loop_iterations_[group_number] = iterations.GetInt();
      }
    }
  }
  if (const base::Value::Dict* usage = checkpoint.FindDict("usage")) {
    execution->usage_.steps_executed =
        usage->FindInt("steps_executed").value_or(0);
    execution->usage_.model_calls = usage->FindInt("model_calls").value_or(0);
    execution->usage_.navigations = usage->FindInt("navigations").value_or(0);
    execution->usage_.cloud_cost_microdollars = static_cast<int64_t>(
        usage->FindDouble("cloud_cost_microdollars").value_or(0.0));
    execution->usage_.replans_used = usage->FindInt("replans_used").value_or(0);
  }
  execution->state_ = TaskState::kExecuting;
  execution->started_at_ = execution->clock_.Run();
  return execution;
}

const char* TaskStateToString(TaskState state) {
  switch (state) {
    case TaskState::kDraft:
      return "draft";
    case TaskState::kPlanning:
      return "planning";
    case TaskState::kAwaitingApproval:
      return "awaiting_approval";
    case TaskState::kExecuting:
      return "executing";
    case TaskState::kPaused:
      return "paused";
    case TaskState::kMonitoring:
      return "monitoring";
    case TaskState::kCompleted:
      return "completed";
    case TaskState::kFailed:
      return "failed";
    case TaskState::kCancelled:
      return "cancelled";
  }
  return "draft";
}

const char* StepStatusToString(StepStatus status) {
  switch (status) {
    case StepStatus::kPending:
      return "pending";
    case StepStatus::kAwaitingApproval:
      return "awaiting_approval";
    case StepStatus::kAwaitingInput:
      return "awaiting_input";
    case StepStatus::kRunning:
      return "running";
    case StepStatus::kSucceeded:
      return "succeeded";
    case StepStatus::kFailed:
      return "failed";
    case StepStatus::kOutcomeUnknown:
      return "outcome_unknown";
    case StepStatus::kSkipped:
      return "skipped";
    case StepStatus::kCancelled:
      return "cancelled";
  }
  return "pending";
}

const char* ExecutionRouteToString(ExecutionRoute route) {
  switch (route) {
    case ExecutionRoute::kDeterministic:
      return "deterministic";
    case ExecutionRoute::kLocalModel:
      return "local";
    case ExecutionRoute::kCloudModel:
      return "cloud";
  }
  return "deterministic";
}

const char* TaskFailureReasonToString(TaskFailureReason reason) {
  switch (reason) {
    case TaskFailureReason::kNone:
      return "none";
    case TaskFailureReason::kStepFailed:
      return "step_failed";
    case TaskFailureReason::kBudgetExhausted:
      return "budget_exhausted";
    case TaskFailureReason::kAssumptionInvalid:
      return "assumption_invalid";
    case TaskFailureReason::kUserStopped:
      return "user_stopped";
    case TaskFailureReason::kProviderUnavailable:
      return "provider_unavailable";
  }
  return "none";
}

const char* ExecutionDecisionToString(ExecutionDecision decision) {
  switch (decision) {
    case ExecutionDecision::kContinue:
      return "continue";
    case ExecutionDecision::kRetryStep:
      return "retry_step";
    case ExecutionDecision::kReplan:
      return "replan";
    case ExecutionDecision::kAskUser:
      return "ask_user";
    case ExecutionDecision::kStop:
      return "stop";
  }
  return "stop";
}

}  // namespace seoul
