// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_shell_header_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "seoul/browser/shell/shell_controller.h"
#include "seoul/browser/shell/views/seoul_workspace_menu.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace seoul {

SeoulShellHeaderView::SeoulShellHeaderView(ShellController* controller) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  BindController(controller);
}

SeoulShellHeaderView::~SeoulShellHeaderView() {
  if (controller_) {
    controller_->RemoveObserver(this);
  }
}

void SeoulShellHeaderView::BindController(ShellController* controller) {
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

void SeoulShellHeaderView::OnShellSnapshotChanged(
    const ShellChange& change,
    const ShellSnapshot& snapshot) {
  (void)change;
  RebuildFromSnapshot(snapshot);
}

void SeoulShellHeaderView::RebuildFromSnapshot(const ShellSnapshot& snapshot) {
  if (!workspace_button_) {
    workspace_button_ = AddChildView(std::make_unique<views::LabelButton>(
        base::BindRepeating(&SeoulShellHeaderView::OnWorkspaceButtonPressed,
                            base::Unretained(this)),
        u"Workspace"));
    workspace_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    workspace_button_->GetViewAccessibility().SetName(u"Workspace switcher");
    essentials_container_ = AddChildView(std::make_unique<views::View>());
    essentials_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    pinned_section_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    retained_section_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    temporary_section_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  }

  std::u16string workspace_label = base::UTF8ToUTF16(snapshot.workspace.name);
  if (snapshot.workspace.switching) {
    workspace_label = u"… " + workspace_label;
  }
  workspace_button_->SetText(workspace_label);
  workspace_button_->SetTooltipText(workspace_label);
  workspace_button_->GetViewAccessibility().SetName(workspace_label);

  essentials_container_->RemoveAllChildViews();
  const bool collapsed = snapshot.mode == ShellMode::kCollapsed;
  size_t shown = 0;
  constexpr size_t kMaxVisibleEssentials = 6;
  for (const ShellEssentialItem& essential : snapshot.essentials) {
    if (shown >= kMaxVisibleEssentials) {
      break;
    }
    auto* button = essentials_container_->AddChildView(
        std::make_unique<views::LabelButton>(
            base::BindRepeating(
                [](ShellController* controller, EssentialId id) {
                  if (controller) {
                    controller->OpenEssential(id);
                  }
                },
                controller_, essential.id),
            collapsed ? u"★" : base::UTF8ToUTF16(essential.name)));
    button->GetViewAccessibility().SetName(base::UTF8ToUTF16(
        essential.name.empty() ? essential.root_url : essential.name));
    button->SetTooltipText(base::UTF8ToUTF16(essential.name));
    button->SetEnabled(essential.state != ShellItemState::kUnavailable);
    shown++;
  }

  auto section_text = [&](ShellSection section, const char* fallback) {
    for (const ShellSectionInfo& info : snapshot.sections) {
      if (info.section == section && info.visible) {
        return base::UTF8ToUTF16(fallback);
      }
    }
    return std::u16string();
  };
  pinned_section_label_->SetText(
      section_text(ShellSection::kWorkspacePinned, "Pinned"));
  retained_section_label_->SetText(
      section_text(ShellSection::kRetainedTabs, "Retained"));
  temporary_section_label_->SetText(
      section_text(ShellSection::kTemporaryTabs, "Temporary"));
  pinned_section_label_->SetVisible(!pinned_section_label_->GetText().empty());
  retained_section_label_->SetVisible(
      !retained_section_label_->GetText().empty());
  temporary_section_label_->SetVisible(
      !temporary_section_label_->GetText().empty());
}

void SeoulShellHeaderView::OnWorkspaceButtonPressed() {
  if (!controller_ || !GetWidget()) {
    return;
  }
  SeoulWorkspaceMenu::Show(GetWidget(), workspace_button_, controller_);
}

BEGIN_METADATA(SeoulShellHeaderView)
END_METADATA

}  // namespace seoul
