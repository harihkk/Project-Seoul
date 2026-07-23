// Project Seoul general-purpose operator: task layer.

#include "seoul/browser/tasks/task_deck_model.h"

#include <utility>

namespace seoul {

TaskRecord::TaskRecord() = default;
TaskRecord::TaskRecord(const TaskRecord&) = default;
TaskRecord::TaskRecord(TaskRecord&&) = default;
TaskRecord& TaskRecord::operator=(const TaskRecord&) = default;
TaskRecord& TaskRecord::operator=(TaskRecord&&) = default;
TaskRecord::~TaskRecord() = default;

namespace {

bool IsFinishedState(TaskState state) {
  return state == TaskState::kCompleted || state == TaskState::kFailed ||
         state == TaskState::kCancelled;
}

}  // namespace

bool IsLegalTaskTransition(TaskState from, TaskState to) {
  if (from == to) {
    return false;
  }
  switch (from) {
    case TaskState::kDraft:
      return to == TaskState::kPlanning || to == TaskState::kCancelled;
    case TaskState::kPlanning:
      return to == TaskState::kAwaitingApproval ||
             to == TaskState::kExecuting || to == TaskState::kFailed ||
             to == TaskState::kCancelled;
    case TaskState::kAwaitingApproval:
      return to == TaskState::kExecuting || to == TaskState::kCancelled;
    case TaskState::kExecuting:
      return to == TaskState::kPaused || to == TaskState::kMonitoring ||
             to == TaskState::kCompleted || to == TaskState::kFailed ||
             to == TaskState::kCancelled;
    case TaskState::kPaused:
      return to == TaskState::kExecuting || to == TaskState::kFailed ||
             to == TaskState::kCancelled;
    case TaskState::kMonitoring:
      return to == TaskState::kExecuting || to == TaskState::kPaused ||
             to == TaskState::kCompleted || to == TaskState::kFailed ||
             to == TaskState::kCancelled;
    case TaskState::kCompleted:
    case TaskState::kFailed:
    case TaskState::kCancelled:
      return false;  // terminal
  }
  return false;
}

TaskDeckModel::TaskDeckModel(base::RepeatingCallback<base::Time()> clock)
    : clock_(std::move(clock)) {}

TaskDeckModel::~TaskDeckModel() {
  observers_.Clear();
}

void TaskDeckModel::AddObserver(TaskDeckObserver* observer) {
  observers_.AddObserver(observer);
}

void TaskDeckModel::RemoveObserver(TaskDeckObserver* observer) {
  observers_.RemoveObserver(observer);
}

void TaskDeckModel::NotifyChanged(const TaskId& id) {
  for (TaskDeckObserver& observer : observers_) {
    observer.OnTaskDeckChanged(id);
  }
}

bool TaskDeckModel::Add(TaskRecord record) {
  if (!record.id.is_valid() || record.title.empty() ||
      record.title.size() > kMaxTaskTitleLength ||
      record.goal.size() > kMaxGoalLength ||
      tasks_.find(record.id) != tasks_.end()) {
    return false;
  }
  if (tasks_.size() >= kMaxTasksInDeck) {
    // Evict the oldest finished task; never an active one.
    auto oldest = tasks_.end();
    for (auto it = tasks_.begin(); it != tasks_.end(); ++it) {
      if (!IsFinishedState(it->second.state)) {
        continue;
      }
      if (oldest == tasks_.end() ||
          it->second.updated_at < oldest->second.updated_at) {
        oldest = it;
      }
    }
    if (oldest == tasks_.end()) {
      return false;
    }
    tasks_.erase(oldest);
  }
  record.created_at = clock_.Run();
  record.updated_at = record.created_at;
  const TaskId id = record.id;
  tasks_.emplace(id, std::move(record));
  NotifyChanged(id);
  return true;
}

const TaskRecord* TaskDeckModel::Find(const TaskId& id) const {
  auto it = tasks_.find(id);
  return it == tasks_.end() ? nullptr : &it->second;
}

bool TaskDeckModel::SetState(const TaskId& id,
                             TaskState next,
                             TaskFailureReason reason) {
  auto it = tasks_.find(id);
  if (it == tasks_.end() || !IsLegalTaskTransition(it->second.state, next)) {
    return false;
  }
  it->second.state = next;
  it->second.failure_reason = reason;
  it->second.updated_at = clock_.Run();
  NotifyChanged(id);
  return true;
}

bool TaskDeckModel::AppendReceipt(const TaskId& id, ActionReceipt receipt) {
  auto it = tasks_.find(id);
  if (it == tasks_.end() || it->second.receipts.size() >= kMaxReceiptsPerTask) {
    return false;
  }
  it->second.total_cost_microdollars += receipt.cost_microdollars;
  if (receipt.route == ExecutionRoute::kCloudModel) {
    it->second.dominant_route = ExecutionRoute::kCloudModel;
  } else if (receipt.route == ExecutionRoute::kLocalModel &&
             it->second.dominant_route == ExecutionRoute::kDeterministic) {
    it->second.dominant_route = ExecutionRoute::kLocalModel;
  }
  it->second.receipts.push_back(std::move(receipt));
  it->second.updated_at = clock_.Run();
  NotifyChanged(id);
  return true;
}

std::vector<const TaskRecord*> TaskDeckModel::Active() const {
  std::vector<const TaskRecord*> result;
  for (const auto& [id, record] : tasks_) {
    if (record.state == TaskState::kExecuting ||
        record.state == TaskState::kPaused ||
        record.state == TaskState::kPlanning ||
        record.state == TaskState::kAwaitingApproval ||
        record.state == TaskState::kDraft) {
      result.push_back(&record);
    }
  }
  return result;
}

std::vector<const TaskRecord*> TaskDeckModel::Monitoring() const {
  std::vector<const TaskRecord*> result;
  for (const auto& [id, record] : tasks_) {
    if (record.state == TaskState::kMonitoring) {
      result.push_back(&record);
    }
  }
  return result;
}

std::vector<const TaskRecord*> TaskDeckModel::Finished() const {
  std::vector<const TaskRecord*> result;
  for (const auto& [id, record] : tasks_) {
    if (IsFinishedState(record.state)) {
      result.push_back(&record);
    }
  }
  return result;
}

}  // namespace seoul
