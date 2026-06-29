// Project Seoul outbound browser command layer.
// Future browser tests for ChromiumMutationAdapterImpl. Not executed in V0.

#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {

class ChromiumMutationAdapterBrowserTest : public InProcessBrowserTest {};

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       OpenTabThroughNavigationAdapter) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       ActivateTabObserved) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       UnloadDelayedCloseAndCancellation) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       PinInducedIndexMovement) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       MoveWithAdjustedDestination) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       SplitCreationUsesExplicitTwoIndexVector) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest, SplitRemoval) {}

IN_PROC_BROWSER_TEST_F(ChromiumMutationAdapterBrowserTest,
                       AdapterWhenWindowOrTabDisappears) {}

}  // namespace seoul
