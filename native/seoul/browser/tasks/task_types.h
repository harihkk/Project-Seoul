// Project Seoul general-purpose operator: task layer.
// Task identities, states, budgets, and action receipts. Every executed step
// produces a receipt recording what ran, what was observed, how the outcome
// was verified, which route (deterministic/local/cloud) did the reasoning,
// and what it cost. The Task Deck renders these directly.

#ifndef SEOUL_BROWSER_TASKS_TASK_TYPES_H_
#define SEOUL_BROWSER_TASKS_TASK_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

class TaskId {
 public:
  TaskId();
  TaskId(const TaskId&);
  TaskId(TaskId&&);
  TaskId& operator=(const TaskId&);
  TaskId& operator=(TaskId&&);
  ~TaskId();

  static TaskId GenerateNew() {
    TaskId id;
    id.uuid_ = base::Uuid::GenerateRandomV4();
    return id;
  }
  static TaskId FromString(std::string_view s) {
    TaskId id;
    id.uuid_ = base::Uuid::ParseLowercase(s);
    return id;
  }
  bool is_valid() const { return uuid_.is_valid(); }
  std::string value() const {
    return uuid_.is_valid() ? uuid_.AsLowercaseString() : std::string();
  }
  friend bool operator==(const TaskId& a, const TaskId& b) {
    return a.uuid_ == b.uuid_;
  }
  friend bool operator<(const TaskId& a, const TaskId& b) {
    return a.uuid_ < b.uuid_;
  }

 private:
  base::Uuid uuid_;
};

enum class TaskState {
  kDraft,
  kPlanning,
  kAwaitingApproval,
  kExecuting,
  kPaused,
  kMonitoring,  // recurring observation; stays visible in the Task Deck
  kCompleted,
  kFailed,
  kCancelled,
};

const char* TaskStateToString(TaskState state);

enum class StepStatus {
  kPending,
  kAwaitingApproval,
  kAwaitingInput,
  kRunning,
  kSucceeded,
  kFailed,
  kOutcomeUnknown,
  kSkipped,
  kCancelled,
};

const char* StepStatusToString(StepStatus status);

// Which layer produced the reasoning/execution for a step.
enum class ExecutionRoute {
  kDeterministic,
  kLocalModel,
  kCloudModel,
};

const char* ExecutionRouteToString(ExecutionRoute route);

enum class TaskFailureReason {
  kNone,
  kStepFailed,
  kBudgetExhausted,
  kAssumptionInvalid,  // semantic target no longer resolves
  kUserStopped,
  kProviderUnavailable,
};

const char* TaskFailureReasonToString(TaskFailureReason reason);

// Hard per-task ceilings. Execution stops (it never silently continues) when
// any is reached.
struct TaskBudgets {
  int max_steps = 50;
  int max_retries_per_step = 2;
  int max_replans = 3;
  int max_model_calls = 40;
  int max_navigations = 60;
  int max_repeat_actions = 5;  // identical tool+args executions
  base::TimeDelta max_duration = base::Minutes(15);
  int64_t max_cloud_cost_microdollars = 500000;  // 0.50 USD default ceiling
  size_t max_context_bytes = 262144;
};

struct VerificationRecord {
  VerificationRecord();
  VerificationRecord(const VerificationRecord&);
  VerificationRecord(VerificationRecord&&);
  VerificationRecord& operator=(const VerificationRecord&);
  VerificationRecord& operator=(VerificationRecord&&);
  ~VerificationRecord();

  bool verified = false;
  std::string method;  // "postcondition", "observation", "verifier:<id>"
  std::string detail;

  friend bool operator==(const VerificationRecord&,
                         const VerificationRecord&) = default;
};

// One executed step's evidence. Receipts are append-only and bounded.
struct ActionReceipt {
  ActionReceipt();
  ActionReceipt(const ActionReceipt&);
  ActionReceipt(ActionReceipt&&);
  ActionReceipt& operator=(const ActionReceipt&);
  ActionReceipt& operator=(ActionReceipt&&);
  ~ActionReceipt();

  std::string step_id;
  ToolId tool;
  StepStatus status = StepStatus::kPending;
  base::Time started_at;
  base::Time finished_at;
  std::string observed_summary;  // what actually happened, from observation
  VerificationRecord verification;
  ExecutionRoute route = ExecutionRoute::kDeterministic;
  int64_t cost_microdollars = 0;
  int model_calls = 0;
  int navigations = 0;

  friend bool operator==(const ActionReceipt&, const ActionReceipt&) = default;
};

inline constexpr size_t kMaxReceiptsPerTask = 256;
inline constexpr size_t kMaxTasksInDeck = 500;
inline constexpr size_t kMaxTaskTitleLength = 200;
inline constexpr size_t kMaxGoalLength = 4096;

}  // namespace seoul

#endif  // SEOUL_BROWSER_TASKS_TASK_TYPES_H_
