// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_SHELL_SERVICE_H_
#define SEOUL_BROWSER_SHELL_SHELL_SERVICE_H_

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "seoul/browser/organization/organization_observer.h"
#include "seoul/browser/organization/organization_types.h"
#include "seoul/browser/shell/shell_types.h"

class Profile;
class VerticalTabStripRegionView;

namespace seoul {

class CommandExecutor;
class LifecycleCoordinator;
class LiveWindowStateProvider;
class OrganizationModel;
class ProjectionService;
class ShellController;
class SeoulShellRegionHost;

class ShellService : public OrganizationModelObserver {
 public:
  using AcknowledgeRecoveryCallback = base::RepeatingCallback<MutationStatus()>;

  ShellService(Profile* profile,
               OrganizationModel* model,
               ProjectionService* projection_service,
               LiveWindowStateProvider* live_state,
               CommandExecutor* executor,
               LifecycleCoordinator* lifecycle,
               bool recovery_required,
               AcknowledgeRecoveryCallback acknowledge_recovery);
  ShellService(const ShellService&) = delete;
  ShellService& operator=(const ShellService&) = delete;
  ~ShellService() override;

  ShellController* GetController(ShellWindowKey window);
  void RegisterVerticalRegion(ShellWindowKey window,
                              VerticalTabStripRegionView* region);
  void UnregisterVerticalRegion(ShellWindowKey window);
  void OnCollapseStateChanged(ShellWindowKey window, bool collapsed);
  void Shutdown();

  void OnOrganizationChanged(const OrganizationChange& change) override;

 private:
  ShellController& EnsureController(ShellWindowKey window);

  raw_ptr<Profile> profile_;
  raw_ptr<OrganizationModel> model_;
  raw_ptr<ProjectionService> projection_service_;
  raw_ptr<LiveWindowStateProvider> live_state_;
  raw_ptr<CommandExecutor> executor_;
  raw_ptr<LifecycleCoordinator> lifecycle_;
  bool recovery_required_ = false;
  bool shutting_down_ = false;
  AcknowledgeRecoveryCallback acknowledge_recovery_;
  std::map<ShellWindowKey, std::unique_ptr<ShellController>> controllers_;
  // One owned host per initialized vertical region (scoped to this service's
  // window bindings; replaces the former process-global host map). The host
  // destructor detaches the shell child views.
  std::map<ShellWindowKey, std::unique_ptr<SeoulShellRegionHost>> hosts_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_SERVICE_H_
