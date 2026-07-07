// Project Seoul product runtime: task-to-surface bridge.
// The production connection between the task engine and the adaptive surface
// layer. It observes the task service and, whenever a task carries a verified
// semantic result, compiles that result into an adaptive surface - creating one
// the first time and patching the same surface in place for every subsequent
// (streaming or final) update. This is the only production caller of
// SurfaceService::CreateFromSemantic; without it a completed task would produce
// no artifact. One surface per task; repeated terminal callbacks never
// duplicate.
//
// STATE OWNERSHIP
//   owner:        one TaskSurfaceBridge per profile runtime.
//   lifetime:     the profile runtime (constructed after the task and surface
//                 services, destroyed before them; removes its observer in the
//                 destructor).
//   persistence:  none; the surface service owns durable (pinned) surfaces.
//   isolation:    per profile.

#ifndef SEOUL_BROWSER_PRODUCT_TASK_SURFACE_BRIDGE_H_
#define SEOUL_BROWSER_PRODUCT_TASK_SURFACE_BRIDGE_H_

#include <optional>

#include <map>

#include "base/memory/raw_ptr.h"
#include "seoul/browser/product/surface_service.h"
#include "seoul/browser/product/task_service.h"
#include "seoul/browser/saui/saui_types.h"
#include "seoul/browser/tasks/task_types.h"

namespace seoul {

class TaskSurfaceBridge : public TaskServiceObserver {
 public:
  // Both services must outlive the bridge (the runtime owns all three).
  TaskSurfaceBridge(TaskService* tasks, SurfaceService* surfaces);
  TaskSurfaceBridge(const TaskSurfaceBridge&) = delete;
  TaskSurfaceBridge& operator=(const TaskSurfaceBridge&) = delete;
  ~TaskSurfaceBridge() override;

  // TaskServiceObserver:
  void OnTaskUpdated(const TaskId& task_id) override;
  void OnTaskFinished(const TaskId& task_id) override;

  // The surface a task projected to, if any (test/inspection helper).
  const SurfaceId* SurfaceForTask(const TaskId& task_id) const;

 private:
  // Creates the task's surface on the first verified semantic result and
  // refreshes the same surface in place on later results (streaming patches).
  // A failed/cancelled task with no prior surface projects nothing.
  void Project(const TaskId& task_id);

  raw_ptr<TaskService> tasks_;
  raw_ptr<SurfaceService> surfaces_;
  std::map<TaskId, SurfaceId> task_surface_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_TASK_SURFACE_BRIDGE_H_
