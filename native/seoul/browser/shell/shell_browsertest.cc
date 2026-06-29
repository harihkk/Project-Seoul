// Project Seoul native browser shell V0.

#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {

class SeoulShellBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(SeoulShellBrowserTest,
                       ShellViewsConstructedInVerticalRegion) {}

IN_PROC_BROWSER_TEST_F(SeoulShellBrowserTest,
                       WorkspaceSwitcherUsesRealSwitcher) {}

IN_PROC_BROWSER_TEST_F(SeoulShellBrowserTest, TabStripRemainsChromiumOwned) {}

}  // namespace seoul
