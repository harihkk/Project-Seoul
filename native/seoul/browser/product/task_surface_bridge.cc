// Project Seoul product runtime: task-to-surface bridge.

#include "seoul/browser/product/task_surface_bridge.h"

#include <utility>

namespace seoul {

TaskSurfaceBridge::TaskSurfaceBridge(TaskService* tasks,
                                     SurfaceService* surfaces)
    : tasks_(tasks), surfaces_(surfaces) {
  tasks_->AddObserver(this);
}

TaskSurfaceBridge::~TaskSurfaceBridge() {
  tasks_->RemoveObserver(this);
}

void TaskSurfaceBridge::OnTaskUpdated(const TaskId& task_id) {
  Project(task_id);
}

void TaskSurfaceBridge::OnTaskFinished(const TaskId& task_id) {
  Project(task_id);
}

const SurfaceId* TaskSurfaceBridge::SurfaceForTask(
    const TaskId& task_id) const {
  auto it = task_surface_.find(task_id);
  return it != task_surface_.end() ? &it->second : nullptr;
}

void TaskSurfaceBridge::Project(const TaskId& task_id) {
  const std::optional<TaskSnapshot> snapshot = tasks_->Snapshot(task_id);
  if (!snapshot.has_value()) {
    return;
  }
  // Only real, verified data becomes an artifact. A task that produced no
  // semantic result (a pure browser mutation, a failed plan) projects nothing;
  // its progress is shown through task state, not a fabricated surface.
  const SemanticResult* semantic = tasks_->FinalSemanticResult(task_id);
  if (!semantic) {
    return;
  }
  // Never show a data artifact for a task that failed or was cancelled before
  // it ever produced a surface (a partial result mid-run is fine; a first
  // projection at a failed terminal state is not).
  auto existing = task_surface_.find(task_id);
  const bool has_surface = existing != task_surface_.end() &&
                           surfaces_->FindSurface(existing->second);
  if (!has_surface && (snapshot->state == TaskState::kFailed ||
                       snapshot->state == TaskState::kCancelled)) {
    return;
  }

  // Update the same surface in place for streaming/terminal updates; create it
  // once. If the mapped surface was removed (e.g. evicted), fall through to a
  // fresh creation so the mapping self-heals.
  if (has_surface && surfaces_->RefreshSemantic(existing->second, *semantic)) {
    return;
  }

  InterfaceIntent intent;
  intent.title = snapshot->goal;
  const SurfaceId id =
      surfaces_->CreateFromSemantic(*semantic, intent, task_id);
  if (id.is_valid()) {
    task_surface_[task_id] = id;
  }
}

}  // namespace seoul
