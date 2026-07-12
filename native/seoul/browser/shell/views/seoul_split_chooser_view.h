// Project Seoul native browser shell: explicit split-partner chooser.

#ifndef SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SPLIT_CHOOSER_VIEW_H_
#define SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SPLIT_CHOOSER_VIEW_H_

#include "ui/gfx/native_ui_types.h"
#include "ui/views/view.h"

namespace seoul {

class ShellController;

class SeoulSplitChooserView {
 public:
  static void Show(gfx::NativeWindow parent,
                   views::View* anchor,
                   ShellController* controller);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SPLIT_CHOOSER_VIEW_H_
