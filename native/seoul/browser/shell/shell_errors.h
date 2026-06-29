// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_SHELL_ERRORS_H_
#define SEOUL_BROWSER_SHELL_SHELL_ERRORS_H_

#include "base/types/expected.h"
#include "seoul/browser/shell/shell_types.h"

namespace seoul {

const char* ShellErrorToString(ShellError error);

template <typename T>
using ShellResult = base::expected<T, ShellError>;

using ShellStatusResult = base::expected<void, ShellError>;

inline ShellStatusResult ShellOk() {
  return base::ok();
}

inline base::unexpected<ShellError> ShellErr(ShellError error) {
  return base::unexpected(error);
}

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_SHELL_ERRORS_H_
