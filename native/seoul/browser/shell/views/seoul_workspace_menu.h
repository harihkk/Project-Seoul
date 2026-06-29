// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_VIEWS_SEOUL_WORKSPACE_MENU_H_
#define SEOUL_BROWSER_SHELL_VIEWS_SEOUL_WORKSPACE_MENU_H_

#include "ui/gfx/native_ui_types.h"
#include "ui/views/view.h"

namespace seoul {

class ShellController;

class SeoulWorkspaceMenu {
 public:
  static void Show(gfx::NativeWindow parent,
                   views::View* anchor,
                   ShellController* controller);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_VIEWS_SEOUL_WORKSPACE_MENU_H_
