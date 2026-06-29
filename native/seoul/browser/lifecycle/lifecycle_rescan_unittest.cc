// Project Seoul native lifecycle bridge.

#include "base/functional/bind.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/organization/organization_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

NormalizedEvent ExistingTab(int window, int tab) {
  NormalizedEvent e;
  e.type = NormalizedEventType::kTabInserted;
  e.window = LiveWindowKey::FromSessionId(window);
  e.tab = LiveTabKey::FromSessionId(tab);
  e.insert_kind = TabInsertKind::kExisting;
  return e;
}

NormalizedEvent GenuineClose(int window, int tab) {
  NormalizedEvent e;
  e.type = NormalizedEventType::kTabRemoved;
  e.window = LiveWindowKey::FromSessionId(window);
  e.tab = LiveTabKey::FromSessionId(tab);
  e.removal_kind = TabRemovalKind::kGenuineClose;
  return e;
}

class RescanTest : public testing::Test {
 protected:
  RescanTest()
      : model_(base::BindRepeating([]() { return base::Time(); })),
        coordinator_(&model_) {}

  size_t MembershipCount() const {
    return model_.ToSnapshot().memberships.size();
  }

  OrganizationModel model_;
  LifecycleCoordinator coordinator_;
};

TEST_F(RescanTest, PostEnumerationInsertDetected) {
  NormalizedEvent window;
  window.type = NormalizedEventType::kWindowDiscovered;
  window.window = LiveWindowKey::FromSessionId(1);
  coordinator_.OnNormalizedEvent(window);
  coordinator_.OnNormalizedEvent(ExistingTab(1, 10));
  EXPECT_EQ(1u, MembershipCount());
  coordinator_.OnNormalizedEvent(ExistingTab(1, 11));
  EXPECT_EQ(2u, MembershipCount());
}

TEST_F(RescanTest, PostEnumerationRemovalDetected) {
  NormalizedEvent window;
  window.type = NormalizedEventType::kWindowDiscovered;
  window.window = LiveWindowKey::FromSessionId(1);
  coordinator_.OnNormalizedEvent(window);
  coordinator_.OnNormalizedEvent(ExistingTab(1, 10));
  coordinator_.OnNormalizedEvent(GenuineClose(1, 10));
  EXPECT_EQ(0u, MembershipCount());
}

TEST_F(RescanTest, RepeatedRescanIsIdempotent) {
  NormalizedEvent window;
  window.type = NormalizedEventType::kWindowDiscovered;
  window.window = LiveWindowKey::FromSessionId(1);
  coordinator_.OnNormalizedEvent(window);
  for (int i = 0; i < 2; ++i) {
    coordinator_.OnNormalizedEvent(ExistingTab(1, 10));
  }
  EXPECT_EQ(1u, MembershipCount());
}

}  // namespace
}  // namespace seoul
