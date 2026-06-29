// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_shell_footer_view.h"

#include "base/strings/utf_string_conversions.h"
#include "seoul/browser/shell/shell_controller.h"
#include "seoul/browser/shell/views/seoul_command_launcher_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

void SeoulShellFooterView::RebuildFromSnapshot(const ShellSnapshot& snapshot) {
  if (!new_tab_button_) {
    auto* row = AddChildView(std::make_unique<views::View>());
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
    split_button_->GetViewAccessibility().SetName(u"Create split");
    reconcile_button_->GetViewAccessibility().SetName(u"Run reconciliation");
    status_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
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

  new_tab_button_->SetEnabled(new_tab.enabled);
  split_button_->SetEnabled(split.enabled);
  if (snapshot.status == ShellStatus::kRecoveryRequired) {
    reconcile_button_->SetVisible(true);
    reconcile_button_->SetText(u"Acknowledge Recovery");
    reconcile_button_->SetEnabled(true);
  } else {
    reconcile_button_->SetText(u"Recover");
    reconcile_button_->SetVisible(snapshot.show_status_banner);
    reconcile_button_->SetEnabled(reconcile.enabled);
  }

  if (snapshot.show_status_banner) {
    status_label_->SetText(base::UTF8ToUTF16(snapshot.status_message));
    status_label_->SetVisible(true);
    status_label_->GetViewAccessibility().SetName(
        base::UTF8ToUTF16(snapshot.status_message));
  } else {
    status_label_->SetVisible(false);
  }
  empty_state_view_->SetVisible(snapshot.show_empty_workspace);
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

void SeoulShellFooterView::OnSplitPressed() {
  if (controller_) {
    controller_->CreateSplitFromActive();
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
