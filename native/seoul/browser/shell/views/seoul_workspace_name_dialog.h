// Project Seoul native browser shell.
// A real modal text-input dialog for creating or renaming a workspace. It
// collects a name from the user and invokes a callback with the entered value;
// there are no fixed or synthesized names. Built on ui::DialogModel and shown
// browser-modal, so it uses the platform dialog with validation and focus
// handling rather than a placeholder.

#ifndef SEOUL_BROWSER_SHELL_VIEWS_SEOUL_WORKSPACE_NAME_DIALOG_H_
#define SEOUL_BROWSER_SHELL_VIEWS_SEOUL_WORKSPACE_NAME_DIALOG_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/gfx/native_ui_types.h"

namespace seoul {

// Shows a modal "name a workspace" dialog anchored to `parent`. On accept with
// a non-empty, bounded name, `on_accept` runs with that name; cancel runs
// nothing. `initial_name` prefills the field (empty for create).
void ShowWorkspaceNameDialog(gfx::NativeWindow parent,
                             const std::u16string& title,
                             const std::u16string& initial_name,
                             base::OnceCallback<void(std::string)> on_accept);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_VIEWS_SEOUL_WORKSPACE_NAME_DIALOG_H_
