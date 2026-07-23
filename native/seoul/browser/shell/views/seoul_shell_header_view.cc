// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_shell_header_view.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
// nogncheck: //chrome/browser/ui reaches this target through the side-panel
// Canvas registration, so a declared dep would be a dependency cycle; the
// symbols link through //chrome/browser like the other circular includes.
#include "chrome/browser/ui/views/chrome_layout_provider.h"  // nogncheck
#include "seoul/browser/shell/shell_controller.h"
#include "seoul/browser/shell/views/seoul_workspace_menu.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace seoul {
namespace {

constexpr int kEssentialOverflowCommandBase = 1;

std::u16string EssentialLabel(const ShellEssentialItem& essential) {
  return base::UTF8ToUTF16(essential.name.empty() ? essential.root_url
                                                  : essential.name);
}

class EssentialsOverflowMenuModel final
    : public ui::SimpleMenuModel,
      public ui::SimpleMenuModel::Delegate {
 public:
  EssentialsOverflowMenuModel(
      ShellController* controller,
      std::vector<ShellEssentialItem> essentials)
      : ui::SimpleMenuModel(this),
        controller_(controller),
        essentials_(std::move(essentials)) {
    for (size_t index = 0; index < essentials_.size(); ++index) {
      AddItem(kEssentialOverflowCommandBase + static_cast<int>(index),
              EssentialLabel(essentials_[index]));
    }
  }

  bool IsCommandIdEnabled(int command_id) const override {
    const int index = command_id - kEssentialOverflowCommandBase;
    return index >= 0 && static_cast<size_t>(index) < essentials_.size() &&
           essentials_[index].state != ShellItemState::kUnavailable;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    (void)event_flags;
    const int index = command_id - kEssentialOverflowCommandBase;
    if (!controller_ || index < 0 ||
        static_cast<size_t>(index) >= essentials_.size() ||
        essentials_[index].state == ShellItemState::kUnavailable) {
      return;
    }
    std::ignore = controller_->OpenEssential(essentials_[index].id);
  }

 private:
  raw_ptr<ShellController> controller_;
  std::vector<ShellEssentialItem> essentials_;
};

}  // namespace

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
  essentials_initialized_ = false;
  rendered_essentials_.clear();
  overflow_essentials_.clear();
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
    essentials_layout_ = essentials_container_->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::Orientation::kHorizontal));
    pinned_section_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    retained_section_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
    temporary_section_label_ = AddChildView(std::make_unique<views::Label>(
        u"", views::style::CONTEXT_LABEL, views::style::STYLE_SECONDARY));
  }

  const bool collapsed = snapshot.mode == ShellMode::kCollapsed;
  const std::u16string workspace_name =
      base::UTF8ToUTF16(snapshot.workspace.name);
  std::u16string workspace_label = workspace_name;
  if (collapsed) {
    workspace_label = base::UTF8ToUTF16(snapshot.workspace.icon);
    if (workspace_label.empty() && !workspace_name.empty()) {
      workspace_label = workspace_name.substr(0, 1);
    }
  }
  if (snapshot.workspace.switching) {
    workspace_label = u"… " + workspace_label;
  }
  workspace_button_->SetText(workspace_label);
  workspace_button_->SetTooltipText(
      workspace_name.empty() ? u"Workspace switcher" : workspace_name);
  std::u16string workspace_accessible_name = u"Workspace switcher";
  if (!workspace_name.empty()) {
    workspace_accessible_name += u", " + workspace_name;
  }
  if (snapshot.workspace.switching) {
    workspace_accessible_name += u", switching";
  }
  workspace_button_->GetViewAccessibility().SetName(
      workspace_accessible_name);

  if (!essentials_initialized_ || essentials_collapsed_ != collapsed ||
      rendered_essentials_ != snapshot.essentials) {
    essentials_container_->RemoveAllChildViews();
    essentials_overflow_button_ = nullptr;
    essentials_layout_->SetOrientation(
        collapsed ? views::BoxLayout::Orientation::kVertical
                  : views::BoxLayout::Orientation::kHorizontal);
    constexpr size_t kMaxVisibleEssentials = 6;
    const size_t visible_count =
        std::min(snapshot.essentials.size(), kMaxVisibleEssentials);
    for (size_t index = 0; index < visible_count; ++index) {
      const ShellEssentialItem& essential = snapshot.essentials[index];
      const std::u16string label = EssentialLabel(essential);
      auto* button = essentials_container_->AddChildView(
          std::make_unique<views::LabelButton>(
              base::BindRepeating(
                  [](ShellController* controller, EssentialId id) {
                    if (controller) {
                      std::ignore = controller->OpenEssential(id);
                    }
                  },
                  controller_, essential.id),
              collapsed ? u"★" : label));
      button->GetViewAccessibility().SetName(label);
      if (essential.is_active) {
        button->GetViewAccessibility().SetDescription(u"Current tab");
      }
      button->SetTooltipText(label);
      button->SetEnabled(essential.state != ShellItemState::kUnavailable);
    }
    overflow_essentials_.assign(snapshot.essentials.begin() + visible_count,
                                snapshot.essentials.end());
    if (!overflow_essentials_.empty()) {
      const std::u16string count =
          base::NumberToString16(overflow_essentials_.size());
      essentials_overflow_button_ = essentials_container_->AddChildView(
          std::make_unique<views::LabelButton>(
              base::BindRepeating(
                  &SeoulShellHeaderView::OnEssentialsOverflowPressed,
                  base::Unretained(this)),
              u"+" + count));
      essentials_overflow_button_->GetViewAccessibility().SetName(
          count + u" more Essentials");
      essentials_overflow_button_->SetTooltipText(u"Show more Essentials");
    }
    rendered_essentials_ = snapshot.essentials;
    essentials_collapsed_ = collapsed;
    essentials_initialized_ = true;
  }

  auto section_text = [&](ShellSection section, const char* fallback) {
    for (const ShellSectionInfo& info : snapshot.sections) {
      if (info.section == section && info.visible) {
        return base::UTF8ToUTF16(info.label.empty() ? fallback : info.label);
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
  pinned_section_label_->SetVisible(
      !collapsed && !pinned_section_label_->GetText().empty());
  retained_section_label_->SetVisible(
      !collapsed && !retained_section_label_->GetText().empty());
  temporary_section_label_->SetVisible(
      !collapsed && !temporary_section_label_->GetText().empty());
}

void SeoulShellHeaderView::OnWorkspaceButtonPressed() {
  if (!controller_ || !GetWidget()) {
    return;
  }
  SeoulWorkspaceMenu::Show(GetWidget()->GetNativeWindow(), workspace_button_, controller_);
}

void SeoulShellHeaderView::OnEssentialsOverflowPressed() {
  if (!controller_ || !essentials_overflow_button_ ||
      overflow_essentials_.empty() || !GetWidget()) {
    return;
  }
  EssentialsOverflowMenuModel model(controller_, overflow_essentials_);
  views::MenuRunner runner(&model, views::MenuRunner::HAS_MNEMONICS);
  runner.RunMenuAt(GetWidget(), nullptr,
                   essentials_overflow_button_->GetAnchorBoundsInScreen(),
                   views::MenuAnchorPosition::kTopLeft,
                   ui::mojom::MenuSourceType::kKeyboard);
}

BEGIN_METADATA(SeoulShellHeaderView)
END_METADATA

}  // namespace seoul
