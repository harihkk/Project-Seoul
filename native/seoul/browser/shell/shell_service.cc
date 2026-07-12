// Project Seoul native browser shell V0.

#include "seoul/browser/shell/shell_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "components/sessions/core/session_id.h"
#include "seoul/browser/shell/shell_controller.h"
#include "seoul/browser/shell/views/seoul_shell_region_host.h"
#include "ui/base/base_window.h"

namespace seoul {

ShellService::ShellService(Profile* profile,
                           OrganizationModel* model,
                           ProjectionService* projection_service,
                           LiveWindowStateProvider* live_state,
                           CommandExecutor* executor,
                           LifecycleCoordinator* lifecycle,
                           bool recovery_required,
                           AcknowledgeRecoveryCallback acknowledge_recovery)
    : profile_(profile),
      model_(model),
      projection_service_(projection_service),
      live_state_(live_state),
      executor_(executor),
      lifecycle_(lifecycle),
      recovery_required_(recovery_required),
      acknowledge_recovery_(std::move(acknowledge_recovery)) {
  if (model_) {
    model_->AddObserver(this);
  }
}

ShellService::~ShellService() {
  Shutdown();
}

void ShellService::Shutdown() {
  if (shutting_down_) {
    return;
  }
  shutting_down_ = true;
  if (model_) {
    model_->RemoveObserver(this);
  }
  // Destroy hosts first: each host detaches its shell child views (which
  // unobserve their controller) while the controllers are still alive.
  hosts_.clear();
  for (auto& [window, controller] : controllers_) {
    controller->Shutdown();
    (void)window;
  }
  controllers_.clear();
  task_summaries_.clear();
}

ShellController* ShellService::GetController(ShellWindowKey window) {
  auto it = controllers_.find(window);
  return it != controllers_.end() ? it->second.get() : nullptr;
}

ShellController& ShellService::EnsureController(ShellWindowKey window) {
  auto it = controllers_.find(window);
  if (it == controllers_.end()) {
    auto controller = std::make_unique<ShellController>(
        window, profile_, model_, projection_service_, live_state_, executor_,
        lifecycle_, recovery_required_);
    controller->SetAcknowledgeRecoveryCallback(acknowledge_recovery_);
    controller->SetFocusWindowCallback(base::BindRepeating(
        [](Profile* profile, LiveWindowKey target) {
          if (!profile || !target.is_valid()) {
            return false;
          }
          ProfileBrowserCollection* collection =
              ProfileBrowserCollection::GetForProfile(profile);
          BrowserWindowInterface* browser =
              collection ? collection->FindBrowserWithID(
                               SessionID::FromSerializedValue(
                                   target.session_id()))
                         : nullptr;
          if (!browser || browser->GetProfile() != profile ||
              browser->IsDeleteScheduled() ||
              browser->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
              !browser->GetWindow()) {
            return false;
          }
          browser->GetWindow()->Activate();
          return true;
        },
        profile_.get()));
    if (auto summary = task_summaries_.find(window);
        summary != task_summaries_.end()) {
      controller->SetTaskSummary(summary->second);
    }
    it = controllers_.emplace(window, std::move(controller)).first;
  }
  return *it->second;
}

void ShellService::RegisterVerticalRegion(ShellWindowKey window,
                                          VerticalTabStripRegionView* region,
                                          BrowserWindowInterface* browser_window) {
  if (shutting_down_ || !window.is_valid() || !region || !browser_window) {
    return;
  }
  const SessionID& browser_id = browser_window->GetSessionID();
  if (browser_window->GetProfile() != profile_ ||
      browser_window->IsDeleteScheduled() ||
      browser_window->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      !browser_id.is_valid() || browser_id.id() != window.session_id()) {
    return;
  }
  ShellController& controller = EnsureController(window);
  controller.SetOpenCanvasCallback(base::BindRepeating(
      [](Profile* profile, LiveWindowKey bound_window) {
        if (!profile || !bound_window.is_valid()) {
          return false;
        }
        ProfileBrowserCollection* collection =
            ProfileBrowserCollection::GetForProfile(profile);
        BrowserWindowInterface* browser =
            collection ? collection->FindBrowserWithID(
                             SessionID::FromSerializedValue(
                                 bound_window.session_id()))
                       : nullptr;
        if (!browser || browser->GetProfile() != profile ||
            browser->IsDeleteScheduled() ||
            browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
          return false;
        }
        SidePanelUI* side_panel =
            browser->GetFeatures().side_panel_ui();
        if (!side_panel) {
          return false;
        }
        side_panel->Show(SidePanelEntryId::kSeoulCanvas);
        return true;
      },
      profile_.get(), window));
  // Deterministic duplicate handling: replacing the entry destroys any prior
  // host (detaching its child views) before the new one attaches. One host per
  // initialized region; no process-global state.
  std::unique_ptr<SeoulShellRegionHost>& host = hosts_[window];
  host = std::make_unique<SeoulShellRegionHost>();
  host->Attach(region, &controller);
}

void ShellService::UnregisterVerticalRegion(ShellWindowKey window) {
  // Destroy the host first (detaches shell child views while the controller is
  // still alive), then tear down the per-window controller binding.
  hosts_.erase(window);
  if (auto it = controllers_.find(window); it != controllers_.end()) {
    it->second->Shutdown();
    controllers_.erase(it);
  }
}

void ShellService::OnCollapseStateChanged(ShellWindowKey window,
                                          bool collapsed) {
  if (ShellController* controller = GetController(window)) {
    controller->SetCollapsed(collapsed);
  }
}

void ShellService::UpdateTaskSummary(ShellWindowKey window,
                                     ShellTaskSummary summary) {
  if (shutting_down_ || !window.is_valid()) {
    return;
  }
  task_summaries_[window] = summary;
  if (ShellController* controller = GetController(window)) {
    controller->SetTaskSummary(summary);
  }
}

void ShellService::ClearTaskSummaries() {
  task_summaries_.clear();
  for (auto& [window, controller] : controllers_) {
    controller->SetTaskSummary(ShellTaskSummary());
    (void)window;
  }
}

void ShellService::OnOrganizationChanged(const OrganizationChange& change) {
  (void)change;
}

}  // namespace seoul
