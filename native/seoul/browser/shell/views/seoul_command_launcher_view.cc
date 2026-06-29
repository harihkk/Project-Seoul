// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_command_launcher_view.h"

#include "base/strings/utf_string_conversions.h"
#include "seoul/browser/shell/command_launcher_catalog.h"
#include "seoul/browser/shell/shell_controller.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace seoul {
namespace {

class LauncherMenuModel : public ui::SimpleMenuModel,
                          public ui::SimpleMenuModel::Delegate {
 public:
  LauncherMenuModel(ShellController* controller,
                    std::vector<CommandLauncherEntry> entries)
      : ui::SimpleMenuModel(this),
        controller_(controller),
        entries_(std::move(entries)) {
    int id = 1;
    for (const CommandLauncherEntry& entry : entries_) {
      AddItem(id++, base::UTF8ToUTF16(entry.label));
    }
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    (void)event_flags;
    if (!controller_ || command_id <= 0 ||
        static_cast<size_t>(command_id) > entries_.size()) {
      return;
    }
    const CommandLauncherEntry& entry = entries_[command_id - 1];
    if (!entry.enabled) {
      return;
    }
    if (entry.id == "new_tab") {
      controller_->OpenNewTemporaryTab();
      return;
    }
    if (entry.id == "create_split") {
      controller_->CreateSplitFromActive();
      return;
    }
    if (entry.id == "reconcile") {
      controller_->RunReconciliation();
      return;
    }
  }

 private:
  raw_ptr<ShellController> controller_;
  std::vector<CommandLauncherEntry> entries_;
};

}  // namespace

void SeoulCommandLauncherView::Show(gfx::NativeWindow parent,
                                    views::View* anchor,
                                    ShellController* controller) {
  (void)parent;
  if (!anchor || !controller) {
    return;
  }
  auto entries = CommandLauncherCatalog::BuildEntries(controller->snapshot());
  LauncherMenuModel model(controller, std::move(entries));
  views::MenuRunner menu_runner(&model, views::MenuRunner::HAS_MNEMONICS);
  menu_runner.RunMenuAt(
      anchor->GetWidget(), nullptr, anchor->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_KEYBOARD);
}

}  // namespace seoul
