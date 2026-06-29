// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_workspace_menu.h"

#include "base/strings/utf_string_conversions.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/organization_types.h"
#include "seoul/browser/shell/shell_controller.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace seoul {
namespace {

constexpr int kCreateWorkspace = 1000;
constexpr int kRenameWorkspace = 1001;
constexpr int kArchiveWorkspace = 1002;
constexpr int kWorkspaceBase = 2000;

class WorkspaceMenuModel : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate {
 public:
  WorkspaceMenuModel(ShellController* controller, std::vector<WorkspaceId> ids)
      : ui::SimpleMenuModel(this),
        controller_(controller),
        workspace_ids_(std::move(ids)) {
    if (!controller_ || !controller_->model()) {
      return;
    }
    const OrganizationSnapshot snap = controller_->model()->ToSnapshot();
    for (const WorkspaceRecord& workspace : snap.workspaces) {
      if (workspace.archived) {
        continue;
      }
      workspace_ids_.push_back(workspace.id);
      AddItem(kWorkspaceBase + static_cast<int>(workspace_ids_.size()) - 1,
              base::UTF8ToUTF16(workspace.name));
    }
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItem(kCreateWorkspace, u"Create Workspace");
    AddItem(kRenameWorkspace, u"Rename Workspace");
    AddItem(kArchiveWorkspace, u"Archive Workspace");
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    (void)event_flags;
    if (!controller_) {
      return;
    }
    if (command_id >= kWorkspaceBase) {
      const size_t index = static_cast<size_t>(command_id - kWorkspaceBase);
      if (index < workspace_ids_.size()) {
        controller_->SwitchWorkspace(workspace_ids_[index]);
      }
      return;
    }
    BrowserCommand command;
    command.id = CommandId::Next();
    switch (command_id) {
      case kCreateWorkspace:
        command.kind = CommandKind::kCreateWorkspace;
        command.name = "Workspace";
        controller_->DispatchModelCommand(std::move(command));
        return;
      case kRenameWorkspace:
        command.kind = CommandKind::kRenameWorkspace;
        command.workspace_id = controller_->snapshot().workspace.workspace_id;
        command.name = controller_->snapshot().workspace.name + " Renamed";
        controller_->DispatchModelCommand(std::move(command));
        return;
      case kArchiveWorkspace:
        command.kind = CommandKind::kArchiveWorkspace;
        command.workspace_id = controller_->snapshot().workspace.workspace_id;
        controller_->DispatchModelCommand(std::move(command));
        return;
      default:
        return;
    }
  }

 private:
  raw_ptr<ShellController> controller_;
  std::vector<WorkspaceId> workspace_ids_;
};

}  // namespace

void SeoulWorkspaceMenu::Show(gfx::NativeWindow parent,
                              views::View* anchor,
                              ShellController* controller) {
  (void)parent;
  if (!anchor || !controller) {
    return;
  }
  WorkspaceMenuModel model(controller, {});
  views::MenuRunner menu_runner(&model, views::MenuRunner::HAS_MNEMONICS);
  menu_runner.RunMenuAt(
      anchor->GetWidget(), nullptr, anchor->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_MOUSE);
}

}  // namespace seoul
