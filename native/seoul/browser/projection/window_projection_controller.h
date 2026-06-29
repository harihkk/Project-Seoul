// Project Seoul workspace projection engine V0.
// One projection controller per eligible browser window.

#ifndef SEOUL_BROWSER_PROJECTION_WINDOW_PROJECTION_CONTROLLER_H_
#define SEOUL_BROWSER_PROJECTION_WINDOW_PROJECTION_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/projection/projection_observer.h"
#include "seoul/browser/projection/projection_types.h"

namespace seoul {

class LifecycleCoordinator;

class WindowProjectionController {
 public:
  WindowProjectionController(LiveWindowKey window,
                             OrganizationModel* model,
                             LifecycleCoordinator* lifecycle);
  WindowProjectionController(const WindowProjectionController&) = delete;
  WindowProjectionController& operator=(const WindowProjectionController&) =
      delete;
  ~WindowProjectionController();

  LiveWindowKey window() const { return window_; }
  const WindowProjection& projection() const { return projection_; }
  const LiveWindowTabState& live_state() const { return live_; }
  bool fail_open() const { return fail_open_; }
  bool switch_in_progress() const { return switch_in_progress_; }

  void SetSwitchInProgress(bool in_progress) {
    switch_in_progress_ = in_progress;
  }

  void UpdateLiveState(const LiveWindowTabState& live);
  void OnOrganizationChanged();
  void OnLifecycleDegraded(bool degraded);
  void EnterFailOpen();
  void ClearFailOpen();
  void Shutdown();

  void AddObserver(ProjectionObserver* observer);
  void RemoveObserver(ProjectionObserver* observer);

 private:
  void Recompute(bool publish);
  void Publish(ProjectionChangeType type);

  LiveWindowKey window_;
  raw_ptr<OrganizationModel> model_;
  raw_ptr<LifecycleCoordinator> lifecycle_;
  LiveWindowTabState live_;
  WindowProjection projection_;
  ProjectionGeneration generation_;
  bool fail_open_ = false;
  bool switch_in_progress_ = false;
  bool shutting_down_ = false;
  base::ObserverList<ProjectionObserver> observers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_WINDOW_PROJECTION_CONTROLLER_H_
