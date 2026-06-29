// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_SHELL_TYPES_H_
#define SEOUL_BROWSER_SHELL_SHELL_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/organization/organization_ids.h"
#include "seoul/browser/organization/organization_types.h"
#include "seoul/browser/projection/projection_types.h"
#include "seoul/browser/projection/workspace_switcher.h"

namespace seoul {

using ShellWindowKey = LiveWindowKey;

enum class ShellMode {
  kExpanded,
  kCollapsed,
};

enum class ShellSection {
  kContextHeader,
  kEssentialsDeck,
  kWorkspacePinned,
  kRetainedTabs,
  kTemporaryTabs,
  kProjectedTabs,
  kUtilityBar,
  kStatus,
  kEmptyWorkspace,
};

enum class ShellItemKind {
  kWorkspace,
  kEssential,
  kPinnedMembership,
  kSectionLabel,
  kUtilityAction,
  kStatusBanner,
};

enum class ShellItemState {
  kNormal,
  kActive,
  kLoading,
  kUnavailable,
  kDisabled,
  kBusy,
};

enum class ShellStatus {
  kCoherent,
  kEmptyWorkspace,
  kSwitchingWorkspace,
  kDegradedProjection,
  kReconciliationRequired,
  kRecoveryRequired,
  kFailOpen,
};

enum class ShellError {
  kInvalidWindow,
  kMutationRejected,
  kCommandRejected,
  kSwitchFailed,
  kConcurrentSwitch,
  kShutdown,
};

enum class ShellUtilityAction {
  kNewTemporaryTab,
  kCommandLauncher,
  kCreateSplit,
  kReconcile,
  kAcknowledgeRecovery,
};

struct ShellEssentialItem {
  EssentialId id;
  std::string name;
  std::string icon;
  std::string root_url;
  ShellItemState state = ShellItemState::kNormal;
  bool has_live_tab = false;
  bool live_in_current_window = false;
  bool is_active = false;
};

struct ShellPinnedItem {
  TabMembershipId membership_id;
  LiveTabKey tab;
  std::string saved_root_url;
  ShellItemState state = ShellItemState::kNormal;
  bool is_active = false;
};

struct ShellWorkspaceHeader {
  WorkspaceId workspace_id;
  std::string name;
  std::string icon;
  bool archived = false;
  bool switching = false;
  ShellItemState state = ShellItemState::kNormal;
};

struct ShellSectionInfo {
  ShellSection section = ShellSection::kProjectedTabs;
  std::string label;
  bool visible = false;
  int projected_count = 0;
};

struct ShellActionEnablement {
  ShellUtilityAction action = ShellUtilityAction::kNewTemporaryTab;
  bool enabled = false;
  std::string disabled_reason;
};

struct ShellSnapshot {
  ShellWindowKey window;
  ShellMode mode = ShellMode::kExpanded;
  ShellStatus status = ShellStatus::kCoherent;
  uint64_t revision = 0;
  ShellWorkspaceHeader workspace;
  std::vector<ShellEssentialItem> essentials;
  std::vector<ShellPinnedItem> pinned_items;
  std::vector<ShellSectionInfo> sections;
  std::vector<ShellActionEnablement> actions;
  bool show_empty_workspace = false;
  bool show_status_banner = false;
  std::string status_message;
  WorkspaceSwitchPhase switch_phase = WorkspaceSwitchPhase::kIdle;
};

struct ShellChange {
  ShellWindowKey window;
  uint64_t revision = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_TYPES_H_
