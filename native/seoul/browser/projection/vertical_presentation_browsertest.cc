// Project Seoul workspace projection engine V0.
// Future view/browser tests for vertical presentation filtering.

#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {

class VerticalPresentationBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(VerticalPresentationBrowserTest,
                       InactiveWorkspaceTabViewsHidden) {}

IN_PROC_BROWSER_TEST_F(VerticalPresentationBrowserTest, ActiveTabNeverHidden) {}

IN_PROC_BROWSER_TEST_F(VerticalPresentationBrowserTest,
                       SplitParentHiddenAtomically) {}

IN_PROC_BROWSER_TEST_F(VerticalPresentationBrowserTest,
                       RecoveryFailOpenShowsAllTabs) {}

}  // namespace seoul
