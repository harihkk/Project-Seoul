// Project Seoul native browser shell V0.

#include "seoul/browser/shell/shell_view_model.h"

#include <algorithm>

namespace seoul {
namespace {

ShellItemState EssentialState(const OrganizationModel& model,
                              const EssentialRecord& essential,
                              ShellWindowKey window,
                              const WindowProjection& projection) {
  (void)model;
  (void)window;
  (void)projection;
  if (essential.root_url.empty()) {
    return ShellItemState::kUnavailable;
  }
  return ShellItemState::kNormal;
}

void AddAction(ShellSnapshot* snapshot,
               ShellUtilityAction action,
               bool enabled,
               const std::string& disabled_reason) {
  ShellActionEnablement entry;
  entry.action = action;
  entry.enabled = enabled;
  entry.disabled_reason = disabled_reason;
  snapshot->actions.push_back(std::move(entry));
}

}  // namespace

ShellSnapshot ShellViewModel::Build(const OrganizationModel& model,
                                    const ShellBuildContext& context,
                                    const WindowProjection& projection,
                                    const LiveWindowSnapshot& live,
                                    uint64_t revision) {
  ShellSnapshot snapshot;
  snapshot.window = context.window;
  snapshot.mode = context.mode;
  snapshot.revision = revision;
  snapshot.switch_phase = context.switch_phase;

  const WorkspaceId active =
      model.ActiveWorkspaceForWindow(context.window.value());
  const WorkspaceRecord* ws = model.FindWorkspace(active);
  if (ws) {
    snapshot.workspace.workspace_id = ws->id;
    snapshot.workspace.name = ws->name;
    snapshot.workspace.icon = ws->icon;
    snapshot.workspace.archived = ws->archived;
  }
  snapshot.workspace.switching =
      context.switch_phase == WorkspaceSwitchPhase::kValidating ||
      context.switch_phase == WorkspaceSwitchPhase::kCalculating ||
      context.switch_phase == WorkspaceSwitchPhase::kAwaitingActivation ||
      context.switch_phase == WorkspaceSwitchPhase::kCommitting;

  if (context.recovery_required) {
    snapshot.status = ShellStatus::kRecoveryRequired;
    snapshot.show_status_banner = true;
    snapshot.status_message = "Recovery required.";
    AddAction(&snapshot, ShellUtilityAction::kAcknowledgeRecovery, true, "");
    AddAction(&snapshot, ShellUtilityAction::kNewTemporaryTab, false,
              "Recovery required.");
    AddAction(&snapshot, ShellUtilityAction::kCreateSplit, false,
              "Recovery required.");
    return snapshot;
  }

  if (context.lifecycle_degraded ||
      projection.status == ProjectionStatus::kReconciliationRequired) {
    snapshot.status = ShellStatus::kReconciliationRequired;
    snapshot.show_status_banner = true;
    snapshot.status_message = "Reconciliation required.";
    AddAction(&snapshot, ShellUtilityAction::kReconcile, true, "");
  } else if (projection.status == ProjectionStatus::kFailOpen) {
    snapshot.status = ShellStatus::kFailOpen;
    snapshot.show_status_banner = true;
    snapshot.status_message = "Showing all tabs while the layout recovers.";
  } else if (context.switch_phase ==
                 WorkspaceSwitchPhase::kAwaitingActivation ||
             context.switch_phase == WorkspaceSwitchPhase::kCommitting) {
    snapshot.status = ShellStatus::kSwitchingWorkspace;
  } else if (projection.empty_workspace) {
    snapshot.status = ShellStatus::kEmptyWorkspace;
    snapshot.show_empty_workspace = true;
  } else {
    snapshot.status = ShellStatus::kCoherent;
  }

  const OrganizationSnapshot org = model.ToSnapshot();
  for (const EssentialRecord& essential : org.essentials) {
    ShellEssentialItem item;
    item.id = essential.id;
    item.name = essential.name;
    item.icon = essential.icon;
    item.root_url = essential.root_url;
    item.state = EssentialState(model, essential, context.window, projection);
    snapshot.essentials.push_back(std::move(item));
  }

  int retained_count = 0;
  int temporary_count = 0;
  int pinned_count = 0;
  for (const ProjectedTab& tab : projection.tabs) {
    if (tab.role == TabRole::kPinned) {
      ShellPinnedItem pinned;
      pinned.membership_id = tab.membership_id;
      pinned.tab = tab.tab;
      pinned.is_active = tab.tab == projection.active_tab;
      pinned.state = ShellItemState::kNormal;
      const TabMembershipRecord* m = model.FindMembership(tab.membership_id);
      if (m) {
        pinned.saved_root_url = m->saved_root_url;
      }
      snapshot.pinned_items.push_back(std::move(pinned));
      pinned_count++;
    } else if (tab.role == TabRole::kRetained) {
      retained_count++;
    } else {
      temporary_count++;
    }
  }

  ShellSectionInfo pinned_section;
  pinned_section.section = ShellSection::kWorkspacePinned;
  pinned_section.label = "Pinned";
  pinned_section.visible =
      pinned_count > 0 || snapshot.mode == ShellMode::kExpanded;
  pinned_section.projected_count = pinned_count;
  snapshot.sections.push_back(pinned_section);

  ShellSectionInfo retained_section;
  retained_section.section = ShellSection::kRetainedTabs;
  retained_section.label = "Retained";
  retained_section.visible = retained_count > 0;
  retained_section.projected_count = retained_count;
  snapshot.sections.push_back(retained_section);

  ShellSectionInfo temporary_section;
  temporary_section.section = ShellSection::kTemporaryTabs;
  temporary_section.label = "Temporary";
  temporary_section.visible = temporary_count > 0;
  temporary_section.projected_count = temporary_count;
  snapshot.sections.push_back(temporary_section);

  const bool mutations_ok =
      snapshot.status != ShellStatus::kReconciliationRequired &&
      snapshot.status != ShellStatus::kRecoveryRequired;
  AddAction(&snapshot, ShellUtilityAction::kNewTemporaryTab, mutations_ok,
            mutations_ok ? "" : "Reconciliation required.");
  AddAction(&snapshot, ShellUtilityAction::kCommandLauncher, true, "");

  bool split_ok = false;
  if (mutations_ok && projection.active_tab.is_valid()) {
    LiveTabKey partner;
    for (const ProjectedTab& tab : projection.tabs) {
      if (tab.tab != projection.active_tab) {
        partner = tab.tab;
        break;
      }
    }
    if (partner.is_valid()) {
      bool active_in_split = false;
      for (const LiveTabDescriptor& descriptor : live.tabs) {
        if (descriptor.tab == projection.active_tab &&
            !descriptor.upstream_split_token.empty()) {
          active_in_split = true;
          break;
        }
      }
      split_ok = !active_in_split;
    }
  }
  AddAction(&snapshot, ShellUtilityAction::kCreateSplit,
            split_ok && mutations_ok,
            split_ok ? "" : "Select another tab to split with.");

  return snapshot;
}

}  // namespace seoul
