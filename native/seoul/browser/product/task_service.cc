// Project Seoul product runtime: the task service.

#include "seoul/browser/product/task_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "seoul/browser/tasks/plan_validator.h"

namespace seoul {

TaskSnapshot::TaskSnapshot() = default;
TaskSnapshot::TaskSnapshot(const TaskSnapshot&) = default;
TaskSnapshot& TaskSnapshot::operator=(const TaskSnapshot&) = default;
TaskSnapshot::TaskSnapshot(TaskSnapshot&&) = default;
TaskSnapshot& TaskSnapshot::operator=(TaskSnapshot&&) = default;
TaskSnapshot::~TaskSnapshot() = default;

TaskService::ActiveTask::ActiveTask() = default;
TaskService::ActiveTask::~ActiveTask() = default;

TaskService::TaskService(ToolRegistry* registry,
                         CapabilityExecutorRegistry* executors,
                         Planner* planner,
                         base::RepeatingCallback<base::Time()> clock)
    : registry_(registry),
      executors_(executors),
      planner_(planner),
      clock_(std::move(clock)) {}

TaskService::~TaskService() = default;

TaskId TaskService::StartTask(const std::string& goal,
                              const LiveWindowKey& window,
                              const ToolPermissionContext& context,
                              bool use_model,
                              bool prefer_local) {
  if (shutting_down_ || goal.empty() || goal.size() > kMaxGoalLength ||
      tasks_.size() >= kMaxTasksInDeck) {
    return TaskId();
  }
  const TaskId task_id = TaskId::GenerateNew();
  auto task = std::make_unique<ActiveTask>();
  task->goal = goal;
  task->window = window;
  task->context = context;
  task->use_model = use_model;
  task->prefer_local = prefer_local;
  tasks_[task_id] = std::move(task);
  planner_->BuildPlan(goal, context, use_model, prefer_local,
                      base::BindOnce(&TaskService::OnPlanned,
                                     weak_factory_.GetWeakPtr(), task_id));
  return task_id;
}

TaskId TaskService::StartTaskWithPlan(const std::string& goal,
                                      Plan plan,
                                      PlanOrigin origin,
                                      const LiveWindowKey& window,
                                      const ToolPermissionContext& context) {
  if (shutting_down_ || tasks_.size() >= kMaxTasksInDeck) {
    return TaskId();
  }
  if (!ValidatePlan(plan, *registry_, context).has_value()) {
    return TaskId();
  }
  const TaskId task_id = TaskId::GenerateNew();
  auto task = std::make_unique<ActiveTask>();
  task->goal = goal;
  task->window = window;
  task->context = context;
  task->plan_origin = origin;
  task->execution = std::make_unique<TaskExecution>(
      task_id, std::move(plan),
      base::BindRepeating([](ToolRegistry* registry,
                             const ToolId& id) { return registry->Find(id); },
                          registry_.get()),
      clock_);
  tasks_[task_id] = std::move(task);
  tasks_[task_id]->execution->Start();
  Pump(task_id);
  return task_id;
}

void TaskService::OnPlanned(TaskId task_id, PlannerResult result) {
  ActiveTask* task = FindTask(task_id);
  if (!task || shutting_down_) {
    return;
  }
  if (!result.ok) {
    // The task never became executable; represent that as a failed execution
    // with the planner's reason so the deck can show it honestly.
    Plan empty_plan;
    empty_plan.goal = task->goal;
    PlanStep unavailable;
    unavailable.id = "planning";
    unavailable.kind = PlanStepKind::kUserInput;
    unavailable.prompt = result.failure;
    empty_plan.steps.push_back(std::move(unavailable));
    task->execution = std::make_unique<TaskExecution>(
        task_id, std::move(empty_plan),
        base::BindRepeating([](ToolRegistry* registry,
                               const ToolId& id) { return registry->Find(id); },
                            registry_.get()),
        clock_);
    task->execution->Start();
    task->execution->Cancel();
    task->pending_approval_prompt = result.failure;
    NotifyUpdated(task_id);
    FinishNotify(task_id);
    return;
  }
  task->plan_origin = result.origin;
  task->execution = std::make_unique<TaskExecution>(
      task_id, std::move(result.plan),
      base::BindRepeating([](ToolRegistry* registry,
                             const ToolId& id) { return registry->Find(id); },
                          registry_.get()),
      clock_);
  task->execution->Start();
  NotifyUpdated(task_id);
  Pump(task_id);
}

void TaskService::Pump(const TaskId& task_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution || shutting_down_) {
    return;
  }
  // Re-entrancy guard: if a synchronous executor completes a step inside
  // DispatchStep (which runs inside PumpOnce), the resulting OnStepOutcome
  // routes back here. Rather than recurse (unbounded stack for a synchronous
  // multi-step plan, and duplicate terminal notifications), flag a follow-up
  // pass and let the top-level loop below run it.
  if (task->driving) {
    task->pump_pending = true;
    return;
  }
  task->driving = true;
  do {
    task->pump_pending = false;
    PumpOnce(task_id);
    task = FindTask(task_id);
  } while (task && task->pump_pending && !shutting_down_);
  if (task) {
    task->driving = false;
  }
}

void TaskService::PumpOnce(const TaskId& task_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution || shutting_down_) {
    return;
  }
  while (true) {
    const NextAction next = task->execution->Advance();
    switch (next.kind) {
      case NextAction::Kind::kRunStep: {
        if (!task->execution->BeginStep(next.step_id)) {
          return;  // not currently eligible; wait for an outcome
        }
        DispatchStep(task_id, next.step_id);
        // Loop again: a read-only parallel group may offer more steps.
        continue;
      }
      case NextAction::Kind::kAwaitApproval: {
        const PlanStep* step = nullptr;
        for (const PlanStep& s : task->execution->plan().steps) {
          if (s.id == next.step_id) {
            step = &s;
            break;
          }
        }
        task->pending_approval_step = next.step_id;
        task->pending_approval_prompt =
            step && !step->prompt.empty()
                ? step->prompt
                : std::string("Approve step ") + next.step_id + "?";
        NotifyUpdated(task_id);
        for (TaskServiceObserver& observer : observers_) {
          observer.OnTaskNeedsApproval(task_id, next.step_id,
                                       task->pending_approval_prompt);
        }
        return;
      }
      case NextAction::Kind::kAwaitInput: {
        task->pending_approval_step = next.step_id;
        task->pending_approval_prompt = "This task needs more input.";
        NotifyUpdated(task_id);
        return;
      }
      case NextAction::Kind::kCompleted:
      case NextAction::Kind::kStopped: {
        if (task->active_dispatch_count > 0) {
          return;  // final state settles when outcomes land
        }
        NotifyUpdated(task_id);
        FinishNotify(task_id);
        return;
      }
    }
  }
}

void TaskService::DispatchStep(const TaskId& task_id,
                               const std::string& step_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task) {
    return;
  }
  const PlanStep* step = nullptr;
  for (const PlanStep& s : task->execution->plan().steps) {
    if (s.id == step_id) {
      step = &s;
      break;
    }
  }
  if (!step || step->kind != PlanStepKind::kToolCall) {
    StepOutcome outcome;
    outcome.status = StepStatus::kFailed;
    outcome.observed_summary = "Step is not executable.";
    task->execution->RecordStepOutcome(step_id, outcome);
    return;
  }
  const ToolDescriptor* descriptor = registry_->Find(step->tool);
  CapabilityExecutor* executor =
      descriptor ? executors_->Find(step->tool, descriptor->version) : nullptr;
  if (!descriptor || !executor) {
    StepOutcome outcome;
    outcome.status = StepStatus::kFailed;
    outcome.observed_summary = "Capability has no registered executor.";
    HandleDecision(task_id,
                   task->execution->RecordStepOutcome(step_id, outcome));
    return;
  }

  ++task->active_dispatch_count;
  CapabilityRequest request;
  request.capability = step->tool;
  request.version = descriptor->version;
  request.args = step->args.Clone();
  request.task_id = task_id;
  request.step_id = step_id;
  request.window = task->window;

  auto timer = std::make_unique<base::OneShotTimer>();
  timer->Start(FROM_HERE, descriptor->timeout,
               base::BindOnce(&TaskService::OnStepTimeout,
                              weak_factory_.GetWeakPtr(), task_id, step_id));
  task->step_timers[step_id] = std::move(timer);

  executor->Execute(
      std::move(request),
      base::BindOnce(&TaskService::OnStepOutcome, weak_factory_.GetWeakPtr(),
                     task_id, step_id));
}

void TaskService::OnStepOutcome(TaskId task_id,
                                std::string step_id,
                                CapabilityOutcome outcome) {
  ActiveTask* task = FindTask(task_id);
  if (!task || shutting_down_) {
    return;
  }
  auto timer_it = task->step_timers.find(step_id);
  if (timer_it == task->step_timers.end()) {
    return;  // timeout already recorded this step; late result is dropped
  }
  task->step_timers.erase(timer_it);
  task->active_dispatch_count = std::max(0, task->active_dispatch_count - 1);

  if (outcome.semantic.has_value()) {
    task->semantic = std::move(outcome.semantic);
  }
  if (outcome.step.status == StepStatus::kOutcomeUnknown) {
    task->had_unknown_outcome_mutation = true;
  }
  HandleDecision(task_id,
                 task->execution->RecordStepOutcome(step_id, outcome.step));
}

void TaskService::OnStepTimeout(TaskId task_id, std::string step_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task || shutting_down_) {
    return;
  }
  auto timer_it = task->step_timers.find(step_id);
  if (timer_it == task->step_timers.end()) {
    return;
  }
  task->step_timers.erase(timer_it);
  task->active_dispatch_count = std::max(0, task->active_dispatch_count - 1);

  const PlanStep* step = nullptr;
  for (const PlanStep& s : task->execution->plan().steps) {
    if (s.id == step_id) {
      step = &s;
      break;
    }
  }
  const ToolDescriptor* descriptor =
      step ? registry_->Find(step->tool) : nullptr;
  if (descriptor) {
    CapabilityExecutor* executor =
        executors_->Find(step->tool, descriptor->version);
    if (executor) {
      executor->Cancel(task_id, step_id);
    }
  }
  StepOutcome outcome;
  // A timed-out read is a failure; a timed-out mutation has an unknown
  // outcome and is never automatically replayed.
  const bool mutating =
      descriptor && descriptor->risk != RiskCategory::kReadOnly;
  outcome.status = mutating ? StepStatus::kOutcomeUnknown : StepStatus::kFailed;
  outcome.observed_summary = "Timed out waiting for the capability.";
  if (mutating) {
    task->had_unknown_outcome_mutation = true;
  }
  HandleDecision(task_id, task->execution->RecordStepOutcome(step_id, outcome));
}

void TaskService::HandleDecision(const TaskId& task_id,
                                 ExecutionDecision decision) {
  ActiveTask* task = FindTask(task_id);
  if (!task) {
    return;
  }
  NotifyUpdated(task_id);
  switch (decision) {
    case ExecutionDecision::kContinue:
    case ExecutionDecision::kRetryStep:
      Pump(task_id);
      return;
    case ExecutionDecision::kReplan:
      Replan(task_id);
      return;
    case ExecutionDecision::kAskUser: {
      task->pending_approval_prompt =
          "The task hit a condition it cannot resolve alone.";
      NotifyUpdated(task_id);
      return;
    }
    case ExecutionDecision::kStop:
      Pump(task_id);  // lets the execution surface its terminal state
      FinishNotify(task_id);
      return;
  }
}

void TaskService::Replan(const TaskId& task_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task) {
    return;
  }
  // A mutation with an unknown outcome is never replayed automatically: the
  // user decides. This is a hard rule, not a policy knob.
  if (task->had_unknown_outcome_mutation) {
    task->pending_approval_prompt =
        "A change may or may not have been applied. Review the receipts "
        "before continuing; nothing was retried automatically.";
    task->execution->Cancel();
    NotifyUpdated(task_id);
    FinishNotify(task_id);
    return;
  }
  if (task->replans_used >= task->execution->plan().budgets.max_replans) {
    task->execution->Cancel();
    NotifyUpdated(task_id);
    FinishNotify(task_id);
    return;
  }
  ++task->replans_used;
  // Preserve the verified evidence: receipts carry across the replan.
  const std::vector<ActionReceipt>& receipts = task->execution->receipts();
  task->carried_receipts.insert(task->carried_receipts.end(), receipts.begin(),
                                receipts.end());
  // Refresh available state: the planner reads the live registry again.
  PlannerResult replanned =
      planner_->BuildDeterministic(task->goal, task->context);
  if (!replanned.ok) {
    task->execution->Cancel();
    NotifyUpdated(task_id);
    FinishNotify(task_id);
    return;
  }
  task->plan_origin = replanned.origin;
  task->execution = std::make_unique<TaskExecution>(
      task_id, std::move(replanned.plan),
      base::BindRepeating([](ToolRegistry* registry,
                             const ToolId& id) { return registry->Find(id); },
                          registry_.get()),
      clock_);
  task->execution->Start();
  NotifyUpdated(task_id);
  Pump(task_id);
}

bool TaskService::Approve(const TaskId& task_id,
                          const std::string& step_id,
                          bool approved) {
  ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution || task->pending_approval_step != step_id) {
    return false;
  }
  task->pending_approval_step.clear();
  task->pending_approval_prompt.clear();
  task->execution->RecordApproval(step_id, approved);
  NotifyUpdated(task_id);
  Pump(task_id);
  return true;
}

bool TaskService::Pause(const TaskId& task_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution) {
    return false;
  }
  task->execution->Pause();
  NotifyUpdated(task_id);
  return true;
}

bool TaskService::Resume(const TaskId& task_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution) {
    return false;
  }
  task->execution->ResumeFromPause();
  NotifyUpdated(task_id);
  Pump(task_id);
  return true;
}

bool TaskService::Cancel(const TaskId& task_id) {
  ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution) {
    return false;
  }
  for (auto& [step_id, timer] : task->step_timers) {
    const PlanStep* step = nullptr;
    for (const PlanStep& s : task->execution->plan().steps) {
      if (s.id == step_id) {
        step = &s;
        break;
      }
    }
    if (step) {
      const ToolDescriptor* descriptor = registry_->Find(step->tool);
      CapabilityExecutor* executor =
          descriptor ? executors_->Find(step->tool, descriptor->version)
                     : nullptr;
      if (executor) {
        executor->Cancel(task_id, step_id);
      }
    }
  }
  task->step_timers.clear();
  task->active_dispatch_count = 0;
  task->execution->Cancel();
  NotifyUpdated(task_id);
  FinishNotify(task_id);
  return true;
}

std::vector<TaskSnapshot> TaskService::Snapshots() const {
  std::vector<TaskSnapshot> out;
  out.reserve(tasks_.size());
  for (const auto& [id, task] : tasks_) {
    if (auto snapshot = Snapshot(id); snapshot.has_value()) {
      out.push_back(std::move(snapshot.value()));
    }
  }
  return out;
}

std::optional<TaskSnapshot> TaskService::Snapshot(const TaskId& task_id) const {
  const ActiveTask* task = FindTask(task_id);
  if (!task) {
    return std::nullopt;
  }
  TaskSnapshot snapshot;
  snapshot.id = task_id;
  snapshot.goal = task->goal;
  snapshot.window = task->window;
  snapshot.plan_origin = task->plan_origin;
  snapshot.replans_used = task->replans_used;
  snapshot.pending_approval_step = task->pending_approval_step;
  snapshot.pending_approval_prompt = task->pending_approval_prompt;
  snapshot.has_semantic_result = task->semantic.has_value();
  snapshot.receipts = task->carried_receipts;
  if (task->execution) {
    snapshot.state = task->execution->state();
    snapshot.failure = task->execution->failure_reason();
    snapshot.usage = task->execution->usage();
    const std::vector<ActionReceipt>& receipts = task->execution->receipts();
    snapshot.receipts.insert(snapshot.receipts.end(), receipts.begin(),
                             receipts.end());
    if (snapshot.receipts.size() > kMaxReceiptsPerTask) {
      snapshot.receipts.erase(snapshot.receipts.begin(),
                              snapshot.receipts.end() - kMaxReceiptsPerTask);
    }
  }
  return snapshot;
}

const SemanticResult* TaskService::FinalSemanticResult(
    const TaskId& task_id) const {
  const ActiveTask* task = FindTask(task_id);
  return task && task->semantic.has_value() ? &task->semantic.value() : nullptr;
}

std::optional<Plan> TaskService::PlanOf(const TaskId& task_id) const {
  const ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution) {
    return std::nullopt;
  }
  return task->execution->plan();
}

const ToolPermissionContext* TaskService::ContextOf(
    const TaskId& task_id) const {
  const ActiveTask* task = FindTask(task_id);
  return task ? &task->context : nullptr;
}

void TaskService::AddObserver(TaskServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void TaskService::RemoveObserver(TaskServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TaskService::Shutdown() {
  shutting_down_ = true;
  weak_factory_.InvalidateWeakPtrs();
  for (auto& [id, task] : tasks_) {
    task->step_timers.clear();
    if (task->execution &&
        (task->execution->state() == TaskState::kExecuting ||
         task->execution->state() == TaskState::kAwaitingApproval ||
         task->execution->state() == TaskState::kPaused)) {
      task->execution->Cancel();
    }
  }
}

void TaskService::NotifyUpdated(const TaskId& task_id) {
  if (shutting_down_) {
    return;
  }
  for (TaskServiceObserver& observer : observers_) {
    observer.OnTaskUpdated(task_id);
  }
}

void TaskService::FinishNotify(const TaskId& task_id) {
  if (shutting_down_) {
    return;
  }
  const ActiveTask* task = FindTask(task_id);
  if (!task || !task->execution) {
    return;
  }
  ActiveTask* mutable_task = FindTask(task_id);
  const TaskState state = task->execution->state();
  const bool terminal = state == TaskState::kCompleted ||
                        state == TaskState::kFailed ||
                        state == TaskState::kCancelled;
  if (!terminal || !mutable_task || mutable_task->finished_notified) {
    return;
  }
  // OnTaskFinished fires exactly once per task, even though several drive
  // paths may observe the terminal state.
  mutable_task->finished_notified = true;
  for (TaskServiceObserver& observer : observers_) {
    observer.OnTaskFinished(task_id);
  }
}

TaskService::ActiveTask* TaskService::FindTask(const TaskId& task_id) {
  auto it = tasks_.find(task_id);
  return it != tasks_.end() ? it->second.get() : nullptr;
}

const TaskService::ActiveTask* TaskService::FindTask(
    const TaskId& task_id) const {
  auto it = tasks_.find(task_id);
  return it != tasks_.end() ? it->second.get() : nullptr;
}

}  // namespace seoul
