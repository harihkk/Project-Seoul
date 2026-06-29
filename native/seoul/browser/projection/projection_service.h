// Project Seoul workspace projection engine V0.

#ifndef SEOUL_BROWSER_PROJECTION_PROJECTION_SERVICE_H_
#define SEOUL_BROWSER_PROJECTION_PROJECTION_SERVICE_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "seoul/browser/lifecycle/live_window_state.h"
#include "seoul/browser/organization/organization_observer.h"
#include "seoul/browser/projection/projection_observer.h"
#include "seoul/browser/projection/vertical_presentation_adapter.h"
#include "seoul/browser/projection/window_projection_controller.h"
#include "seoul/browser/projection/workspace_switcher.h"

class Profile;

namespace views {
class View;
}

class VerticalTabStripRegionView;

namespace seoul {

class CommandExecutor;
class LifecycleCoordinator;
class OrganizationModel;

class ProjectionService : public OrganizationModelObserver,
                          public LiveWindowStateObserver,
                          public ProjectionObserver {
 public:
  ProjectionService(Profile* profile,
                    OrganizationModel* model,
                    LifecycleCoordinator* lifecycle,
                    CommandExecutor* executor,
                    LiveWindowStateProvider* live_state);
  ProjectionService(const ProjectionService&) = delete;
  ProjectionService& operator=(const ProjectionService&) = delete;
  ~ProjectionService() override;

  WindowProjectionController* GetController(LiveWindowKey window);
  WorkspaceSwitcher* GetSwitcher(LiveWindowKey window);

  void RegisterVerticalRegion(LiveWindowKey window,
                              VerticalTabStripRegionView* region);
  void UnregisterVerticalRegion(LiveWindowKey window);

  views::View* GetDefaultFocusableChildForWindow(LiveWindowKey window);

  void OnHiddenTabActivated(LiveWindowKey window, LiveTabKey tab);
  void Shutdown();

  void OnOrganizationChanged(const OrganizationChange& change) override;
  void OnLiveWindowSnapshotChanged(const LiveWindowSnapshot& snapshot) override;
  void OnLiveWindowRemoved(LiveWindowKey window) override;
  void OnProjectionChanged(const ProjectionChange& change,
                           const WindowProjection& projection) override;

 private:
  WindowProjectionController& EnsureController(LiveWindowKey window);
  void ApplyProjectionToViews(LiveWindowKey window,
                              const WindowProjection& projection);
  void HandleLifecycleDegraded();
  void MaybeHandleHiddenActiveTab(LiveWindowKey window,
                                  LiveTabKey tab,
                                  const WindowProjection& projection);

  raw_ptr<Profile> profile_;
  raw_ptr<OrganizationModel> model_;
  raw_ptr<LifecycleCoordinator> lifecycle_;
  raw_ptr<CommandExecutor> executor_;
  raw_ptr<LiveWindowStateProvider> live_state_;
  std::map<LiveWindowKey, std::unique_ptr<WindowProjectionController>>
      controllers_;
  std::map<LiveWindowKey, std::unique_ptr<WorkspaceSwitcher>> switchers_;
  std::map<LiveWindowKey, raw_ptr<VerticalTabStripRegionView>> regions_;
  std::map<LiveWindowKey, VerticalPresentationAdapter> adapters_;
  bool shutting_down_ = false;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_PROJECTION_SERVICE_H_
