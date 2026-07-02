// Project Seoul native browser shell V0.
//
// Real in-process browser tests for the Seoul shell integration. They run
// against a real Browser with the Seoul organization service attached to the
// regular profile and assert the load-bearing integration invariants: the
// profile-scoped services exist and are wired, the Chromium tab strip remains
// the owner of tabs (Seoul projects; it does not replace), and the model is
// reachable through the service. Wired into //chrome/test:browser_tests via
// the single integration patch.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/seoul_organization_service.h"
#include "seoul/browser/organization/seoul_organization_service_factory.h"
#include "seoul/browser/projection/projection_service.h"
#include "seoul/browser/shell/shell_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace seoul {

class SeoulShellBrowserTest : public InProcessBrowserTest {
 protected:
  SeoulOrganizationService* service() {
    return SeoulOrganizationServiceFactory::GetForProfile(browser()->profile());
  }
};

// The profile-scoped Seoul runtime services are constructed and wired for a
// regular profile: the real Seoul runtime is linked into Chrome, not a dead
// library.
IN_PROC_BROWSER_TEST_F(SeoulShellBrowserTest, ServicesWiredForRegularProfile) {
  SeoulOrganizationService* svc = service();
  ASSERT_TRUE(svc);
  EXPECT_TRUE(svc->projection_service());
  EXPECT_TRUE(svc->shell_service());
  EXPECT_TRUE(svc->command_executor());
  EXPECT_TRUE(svc->lifecycle_coordinator());
}

// Seoul projects the tab strip; it never replaces it. Adding a tab through the
// normal Chromium path changes the Chromium tab strip's own count, and the
// service remains attached and unbroken.
IN_PROC_BROWSER_TEST_F(SeoulShellBrowserTest, TabStripRemainsChromiumOwned) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  const int before = tab_strip->count();
  ASSERT_TRUE(
      AddTabAtIndex(before, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(tab_strip->count(), before + 1);
  EXPECT_TRUE(service());
}

// The organization model is reachable through the service and bounded; the
// shell reads this model rather than owning tab state. (Default-workspace
// bootstrapping and mutations are covered by the organization unit tests; this
// asserts the browser-level wiring holds.)
IN_PROC_BROWSER_TEST_F(SeoulShellBrowserTest, OrganizationModelIsReachable) {
  SeoulOrganizationService* svc = service();
  ASSERT_TRUE(svc);
  // EnsureDefaultWorkspace() runs during service construction, so a default
  // workspace always exists.
  EXPECT_GE(svc->model().workspace_count(), 1u);
}

}  // namespace seoul
