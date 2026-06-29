// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_FOOTER_VIEW_H_
#define SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_FOOTER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "seoul/browser/shell/shell_observer.h"
#include "seoul/browser/shell/shell_types.h"
#include "ui/views/view.h"

namespace seoul {

class ShellController;

class SeoulShellFooterView : public views::View, public ShellObserver {
 public:
  METADATA_HEADER(SeoulShellFooterView, views::View)
  explicit SeoulShellFooterView(ShellController* controller);
  SeoulShellFooterView(const SeoulShellFooterView&) = delete;
  SeoulShellFooterView& operator=(const SeoulShellFooterView&) = delete;
  ~SeoulShellFooterView() override;

  void BindController(ShellController* controller);
  void OnShellSnapshotChanged(const ShellChange& change,
                              const ShellSnapshot& snapshot) override;

 private:
  void RebuildFromSnapshot(const ShellSnapshot& snapshot);
  void OnNewTabPressed();
  void OnCommandLauncherPressed();
  void OnSplitPressed();
  void OnReconcilePressed();

  raw_ptr<ShellController> controller_ = nullptr;
  raw_ptr<views::LabelButton> new_tab_button_ = nullptr;
  raw_ptr<views::LabelButton> launcher_button_ = nullptr;
  raw_ptr<views::LabelButton> split_button_ = nullptr;
  raw_ptr<views::LabelButton> reconcile_button_ = nullptr;
  raw_ptr<views::Label> status_label_ = nullptr;
  raw_ptr<views::View> empty_state_view_ = nullptr;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_FOOTER_VIEW_H_
