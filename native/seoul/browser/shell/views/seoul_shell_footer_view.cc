// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_shell_footer_view.h"

#include "base/strings/utf_string_conversions.h"
#include "seoul/browser/shell/shell_controller.h"
#include "seoul/browser/shell/shell_view_model.h"
#include "seoul/browser/shell/views/seoul_command_launcher_view.h"
#include "seoul/browser/shell/views/seoul_split_chooser_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace seoul {

SeoulShellFooterView::SeoulShellFooterView(ShellController* controller) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  // Window-scoped command-palette shortcut. View accelerator registration is
  // attached/detached with this footer's FocusManager, so it cannot leak into
  // another browser window or outlive the shell region.
  AddAccelerator(ui::Accelerator(
      ui::VKEY_K, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN));
  BindController(controller);
}

SeoulShellFooterView::~SeoulShellFooterView() {
  if (controller_) {
    controller_->RemoveObserver(this);
  }
}

void SeoulShellFooterView::BindController(ShellController* controller) {
  if (controller_ == controller) {
    return;
  }
  if (controller_) {
    controller_->RemoveObserver(this);
  }
  controller_ = controller;
  if (controller_) {
    controller_->AddObserver(this);
    RebuildFromSnapshot(controller_->snapshot());
  }
}

void SeoulShellFooterView::OnShellSnapshotChanged(
    const ShellChange& change,
    const ShellSnapshot& snapshot) {
  (void)change;
  RebuildFromSnapshot(snapshot);
}

bool SeoulShellFooterView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  const ui::Accelerator launcher(
      ui::VKEY_K, ui::EF_PLATFORM_ACCELERATOR | ui::EF_SHIFT_DOWN);
  if (accelerator != launcher || !controller_ || !GetWidget()) {
    return false;
  }
  OnCommandLauncherPressed();
  return true;
}

void SeoulShellFooterView::RebuildFromSnapshot(const ShellSnapshot& snapshot) {
  if (!new_tab_button_) {
    auto* row = AddChildView(std::make_unique<views::View>());
    button_row_layout_ =
        row->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    new_tab_button_ = row->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SeoulShellFooterView::OnNewTabPressed,
                            base::Unretained(this)),
        u"+ Tab"));
    launcher_button_ = row->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SeoulShellFooterView::OnCommandLauncherPressed,
                            base::Unretained(this)),
        u"⌘"));
    task_button_ = row->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SeoulShellFooterView::OnTaskDeckPressed,
                            base::Unretained(this)),
        u"Tasks"));
    split_button_ = row->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SeoulShellFooterView::OnSplitPressed,
                            base::Unretained(this)),
        u"Split"));
    reconcile_button_ = row->AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SeoulShellFooterView::OnReconcilePressed,
                            base::Unretained(this)),
        u"Recover"));
    new_tab_button_->GetViewAccessibility().SetName(u"New tab");
    launcher_button_->GetViewAccessibility().SetName(u"Command launcher");
    task_button_->GetViewAccessibility().SetName(u"Task Deck, no tasks");
    split_button_->GetViewAccessibility().SetName(u"Create split");
    reconcile_button_->GetViewAccessibility().SetName(u"Run reconciliation");
    status_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    status_label_->GetViewAccessibility().SetLiveRegionContainer(
        views::ViewAccessibility::LiveRegionStatus::kPolite);
    empty_state_view_ = AddChildView(std::make_unique<views::View>());
    auto* empty_layout =
        empty_state_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kVertical));
    empty_layout->set_main_axis_alignment(
        views::BoxLayout::MainAxisAlignment::kCenter);
    auto* empty_label = empty_state_view_->AddChildView(
        std::make_unique<views::Label>(u"No tabs in this workspace"));
    empty_label->GetViewAccessibility().SetName(u"Empty workspace");
    auto* empty_action =
        empty_state_view_->AddChildView(std::make_unique<views::LabelButton>(
            base::BindRepeating(&SeoulShellFooterView::OnNewTabPressed,
                                base::Unretained(this)),
            u"Open New Tab"));
    empty_action->GetViewAccessibility().SetName(u"Open new tab");
  }

  auto action_enabled = [&](ShellUtilityAction action) {
    for (const ShellActionEnablement& entry : snapshot.actions) {
      if (entry.action == action) {
        return entry;
      }
    }
    return ShellActionEnablement();
  };

  const ShellActionEnablement new_tab =
      action_enabled(ShellUtilityAction::kNewTemporaryTab);
  const ShellActionEnablement split =
      action_enabled(ShellUtilityAction::kCreateSplit);
  const ShellActionEnablement reconcile =
      action_enabled(ShellUtilityAction::kReconcile);
  const ShellActionEnablement task_deck =
      action_enabled(ShellUtilityAction::kOpenTaskDeck);
  const bool collapsed = snapshot.mode == ShellMode::kCollapsed;
  button_row_layout_->SetOrientation(
      collapsed ? views::BoxLayout::Orientation::kVertical
                : views::BoxLayout::Orientation::kHorizontal);

  new_tab_button_->SetText(collapsed ? u"+" : u"+ Tab");
  new_tab_button_->SetTooltipText(
      new_tab.enabled ? u"Open new temporary tab"
                      : base::UTF8ToUTF16(new_tab.disabled_reason));
  launcher_button_->SetText(collapsed ? u"⌕" : u"Commands");
  launcher_button_->SetTooltipText(u"Search commands");
  task_button_->SetText(base::UTF8ToUTF16(
      ShellViewModel::TaskButtonLabel(snapshot.tasks, snapshot.mode)));
  task_button_->GetViewAccessibility().SetName(base::UTF8ToUTF16(
      ShellViewModel::TaskAccessibleName(snapshot.tasks)));
  task_button_->SetTooltipText(
      task_deck.enabled ? u"Open Task Deck"
                        : base::UTF8ToUTF16(task_deck.disabled_reason));
  task_button_->SetEnabled(task_deck.enabled);
  split_button_->SetText(collapsed ? u"↔" : u"Split");
  split_button_->SetTooltipText(
      split.enabled ? u"Create split" : base::UTF8ToUTF16(split.disabled_reason));

  new_tab_button_->SetEnabled(new_tab.enabled);
  split_button_->SetEnabled(split.enabled);
  if (snapshot.status == ShellStatus::kRecoveryRequired) {
    reconcile_button_->SetVisible(true);
    reconcile_button_->SetText(collapsed ? u"!" : u"Acknowledge Recovery");
    reconcile_button_->GetViewAccessibility().SetName(u"Acknowledge recovery");
    reconcile_button_->SetEnabled(true);
  } else {
    reconcile_button_->SetText(collapsed ? u"!" : u"Recover");
    reconcile_button_->GetViewAccessibility().SetName(u"Run reconciliation");
    reconcile_button_->SetVisible(snapshot.show_status_banner);
    reconcile_button_->SetEnabled(reconcile.enabled);
  }
  reconcile_button_->SetTooltipText(
      snapshot.status == ShellStatus::kRecoveryRequired
          ? u"Acknowledge recovery"
          : reconcile.enabled
                ? u"Run reconciliation"
                : base::UTF8ToUTF16(reconcile.disabled_reason));

  if (snapshot.show_status_banner && !collapsed) {
    status_label_->SetText(base::UTF8ToUTF16(snapshot.status_message));
    status_label_->SetVisible(true);
    status_label_->GetViewAccessibility().SetName(
        base::UTF8ToUTF16(snapshot.status_message));
  } else {
    status_label_->SetVisible(false);
  }
  empty_state_view_->SetVisible(snapshot.show_empty_workspace && !collapsed);
}

void SeoulShellFooterView::OnNewTabPressed() {
  if (controller_) {
    controller_->OpenNewTemporaryTab();
  }
}

void SeoulShellFooterView::OnCommandLauncherPressed() {
  if (!controller_ || !GetWidget()) {
    return;
  }
  SeoulCommandLauncherView::Show(GetWidget(), launcher_button_, controller_);
}

void SeoulShellFooterView::OnTaskDeckPressed() {
  if (controller_) {
    controller_->RunUtilityAction(ShellUtilityAction::kOpenTaskDeck);
  }
}

void SeoulShellFooterView::OnSplitPressed() {
  if (controller_ && GetWidget()) {
    SeoulSplitChooserView::Show(GetWidget(), split_button_, controller_);
  }
}

void SeoulShellFooterView::OnReconcilePressed() {
  if (!controller_) {
    return;
  }
  if (controller_->snapshot().status == ShellStatus::kRecoveryRequired) {
    controller_->AcknowledgeRecovery();
    return;
  }
  controller_->RunReconciliation();
}

BEGIN_METADATA(SeoulShellFooterView)
END_METADATA

}  // namespace seoul
