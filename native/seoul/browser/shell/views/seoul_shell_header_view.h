// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_HEADER_VIEW_H_
#define SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_HEADER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "seoul/browser/shell/shell_observer.h"
#include "seoul/browser/shell/shell_types.h"
#include "ui/views/view.h"

namespace views {
class BoxLayout;
class Label;
class LabelButton;
}  // namespace views

namespace seoul {

class ShellController;

class SeoulShellHeaderView : public views::View, public ShellObserver {
  METADATA_HEADER(SeoulShellHeaderView, views::View)

 public:
  explicit SeoulShellHeaderView(ShellController* controller);
  SeoulShellHeaderView(const SeoulShellHeaderView&) = delete;
  SeoulShellHeaderView& operator=(const SeoulShellHeaderView&) = delete;
  ~SeoulShellHeaderView() override;

  void BindController(ShellController* controller);
  void OnShellSnapshotChanged(const ShellChange& change,
                              const ShellSnapshot& snapshot) override;

 private:
  void RebuildFromSnapshot(const ShellSnapshot& snapshot);
  void OnWorkspaceButtonPressed();
  void OnEssentialsOverflowPressed();

  raw_ptr<ShellController> controller_ = nullptr;
  raw_ptr<views::LabelButton> workspace_button_ = nullptr;
  raw_ptr<views::View> essentials_container_ = nullptr;
  raw_ptr<views::BoxLayout> essentials_layout_ = nullptr;
  raw_ptr<views::Label> pinned_section_label_ = nullptr;
  raw_ptr<views::Label> retained_section_label_ = nullptr;
  raw_ptr<views::Label> temporary_section_label_ = nullptr;
  raw_ptr<views::LabelButton> essentials_overflow_button_ = nullptr;
  std::vector<ShellEssentialItem> rendered_essentials_;
  std::vector<ShellEssentialItem> overflow_essentials_;
  bool essentials_initialized_ = false;
  bool essentials_collapsed_ = false;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_HEADER_VIEW_H_
