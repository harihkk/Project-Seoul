// Project Seoul native lifecycle bridge.
// Authored for later compilation on a capable host. Not run on the dev machine.
// Startup / session-restore reconciliation: match restored tabs by the identity
// the pinned source guarantees, never fabricate a live tab, stay idempotent.

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/organization_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

NormalizedEvent Make(NormalizedEventType type) {
  NormalizedEvent e;
  e.type = type;
  return e;
}

NormalizedEvent Window(int w) {
  NormalizedEvent e = Make(NormalizedEventType::kWindowDiscovered);
  e.window = LiveWindowKey::FromSessionId(w);
  return e;
}

NormalizedEvent RestoredTab(int w, int t) {
  NormalizedEvent e = Make(NormalizedEventType::kTabInserted);
  e.window = LiveWindowKey::FromSessionId(w);
  e.tab = LiveTabKey::FromSessionId(t);
  e.insert_kind = TabInsertKind::kRestored;
  return e;
}

class ReconciliationTest : public testing::Test {
 protected:
  ReconciliationTest()
      : model_(base::BindLambdaForTesting([this]() { return now_; })),
        coordinator_(&model_) {}

  size_t MembershipCount() const {
    return model_.ToSnapshot().memberships.size();
  }
  bool HasTab(int t) {
    return model_.FindMembershipIdByTabKey(LiveTabKey::FromSessionId(t).value())
        .is_valid();
  }
  // Simulate state loaded from prefs before reconciliation.
  void PreloadMembership(int t) {
    CHECK(model_.EnsureDefaultWorkspace().has_value());
    CHECK(model_
              .AddTabMembership(model_.default_workspace(),
                                LiveTabKey::FromSessionId(t).value(),
                                TabRole::kPinned)
              .has_value());
  }

  base::Time now_ = base::Time::UnixEpoch() + base::Seconds(1000);
  OrganizationModel model_;
  LifecycleCoordinator coordinator_;
};

TEST_F(ReconciliationTest, FreshProfile) {
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationCompleted));
  EXPECT_TRUE(model_.default_workspace().is_valid());
  EXPECT_FALSE(coordinator_.is_reconciling());
}

TEST_F(ReconciliationTest, MatchingRestoredTabIsNotDuplicated) {
  PreloadMembership(50);
  EXPECT_EQ(1u, MembershipCount());
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(RestoredTab(1, 50));  // same key as preloaded
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationCompleted));
  EXPECT_EQ(1u, MembershipCount());  // matched, not duplicated
  EXPECT_TRUE(HasTab(50));
}

TEST_F(ReconciliationTest, UnmatchedRestoredTabDoesNotFabricateMembership) {
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(RestoredTab(1, 99));
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationCompleted));
  EXPECT_FALSE(HasTab(99));
  EXPECT_EQ(0u, MembershipCount());
}

TEST_F(ReconciliationTest, MissingPersistedTabLeftUnresolvedNeverFabricated) {
  PreloadMembership(50);  // a tab that will not reappear
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(Window(1));
  // No insertion for tab 50.
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationCompleted));
  // Left as bounded restorable metadata; not removed, not turned into a live
  // tab.
  EXPECT_TRUE(HasTab(50));
  EXPECT_EQ(1u, MembershipCount());
}

TEST_F(ReconciliationTest, RepeatedReconciliationIsIdempotent) {
  PreloadMembership(50);
  for (int i = 0; i < 2; ++i) {
    coordinator_.OnNormalizedEvent(
        Make(NormalizedEventType::kReconciliationBegan));
    coordinator_.OnNormalizedEvent(Window(1));
    coordinator_.OnNormalizedEvent(RestoredTab(1, 50));
    coordinator_.OnNormalizedEvent(
        Make(NormalizedEventType::kReconciliationCompleted));
  }
  EXPECT_EQ(1u, MembershipCount());
  EXPECT_EQ(1u, coordinator_.known_window_count());
}

TEST_F(ReconciliationTest, InterruptedReconciliationKeepsConsistentState) {
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(RestoredTab(1, 50));
  // No completion event arrives (interrupted).
  EXPECT_TRUE(coordinator_.is_reconciling());
  // An unmatched restored tab is not fabricated into durable organization
  // state, even when reconciliation is interrupted.
  EXPECT_FALSE(HasTab(50));
  EXPECT_EQ(0u, MembershipCount());
  EXPECT_EQ(1u, coordinator_.known_window_count());
}

TEST_F(ReconciliationTest, CrashRestartPathMatchesNormalRestart) {
  // A second begin without an intervening complete (crash-style) is
  // deterministic.
  PreloadMembership(50);
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(RestoredTab(1, 50));
  coordinator_.OnNormalizedEvent(
      Make(NormalizedEventType::kReconciliationCompleted));
  EXPECT_EQ(1u, MembershipCount());
  EXPECT_FALSE(coordinator_.is_reconciling());
}

}  // namespace
}  // namespace seoul
