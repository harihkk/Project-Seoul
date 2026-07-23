// Project Seoul general-purpose operator: task layer.
// The Task Deck model: every task Seoul runs is visible here with its state,
// receipts, and cost. There is no hidden automation; background workflows and
// monitors appear alongside interactive tasks.

#ifndef SEOUL_BROWSER_TASKS_TASK_DECK_MODEL_H_
#define SEOUL_BROWSER_TASKS_TASK_DECK_MODEL_H_

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "seoul/browser/tasks/task_types.h"

namespace seoul {

struct TaskRecord {
  TaskRecord();
  TaskRecord(const TaskRecord&);
  TaskRecord(TaskRecord&&);
  TaskRecord& operator=(const TaskRecord&);
  TaskRecord& operator=(TaskRecord&&);
  ~TaskRecord();

  TaskId id;
  std::string title;
  std::string goal;
  TaskState state = TaskState::kDraft;
  TaskFailureReason failure_reason = TaskFailureReason::kNone;
  ExecutionRoute dominant_route = ExecutionRoute::kDeterministic;
  int64_t total_cost_microdollars = 0;
  base::Time created_at;
  base::Time updated_at;
  std::string workflow_id;  // set when the task runs a saved workflow
  std::string scene_id;     // originating Scene, when any
  std::vector<ActionReceipt> receipts;
};

class TaskDeckObserver : public base::CheckedObserver {
 public:
  virtual void OnTaskDeckChanged(const TaskId& task) = 0;
};

class TaskDeckModel {
 public:
  explicit TaskDeckModel(base::RepeatingCallback<base::Time()> clock);
  TaskDeckModel(const TaskDeckModel&) = delete;
  TaskDeckModel& operator=(const TaskDeckModel&) = delete;
  ~TaskDeckModel();

  void AddObserver(TaskDeckObserver* observer);
  void RemoveObserver(TaskDeckObserver* observer);

  // Adds a task in kDraft/kPlanning. When the deck is full, the oldest
  // completed/failed/cancelled task is evicted; active tasks are never
  // evicted, and adding fails (false) if all slots are active.
  bool Add(TaskRecord record);
  const TaskRecord* Find(const TaskId& id) const;

  // Legal transitions only (for example kExecuting -> kPaused); anything
  // else returns false and changes nothing.
  bool SetState(const TaskId& id,
                TaskState next,
                TaskFailureReason reason = TaskFailureReason::kNone);
  bool AppendReceipt(const TaskId& id, ActionReceipt receipt);

  std::vector<const TaskRecord*> Active() const;  // executing/paused/...
  std::vector<const TaskRecord*> Monitoring() const;
  std::vector<const TaskRecord*> Finished() const;  // completed/failed/...
  size_t size() const { return tasks_.size(); }

 private:
  void NotifyChanged(const TaskId& id);

  base::RepeatingCallback<base::Time()> clock_;
  std::map<TaskId, TaskRecord> tasks_;
  base::ObserverList<TaskDeckObserver> observers_;
};

// True when `from` -> `to` is a legal task state transition.
bool IsLegalTaskTransition(TaskState from, TaskState to);

}  // namespace seoul

#endif  // SEOUL_BROWSER_TASKS_TASK_DECK_MODEL_H_
