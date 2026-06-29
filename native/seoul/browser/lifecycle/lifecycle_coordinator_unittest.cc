// Project Seoul native lifecycle bridge.
// Authored for later compilation on a capable host. Not run on the dev machine.

#include "seoul/browser/lifecycle/lifecycle_coordinator.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/organization_observer.h"
#include "seoul/browser/organization/organization_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

NormalizedEvent Event(NormalizedEventType type) {
  NormalizedEvent e;
  e.type = type;
  return e;
}

NormalizedEvent WindowDiscovered(int window) {
  NormalizedEvent e = Event(NormalizedEventType::kWindowDiscovered);
  e.window = LiveWindowKey::FromSessionId(window);
  return e;
}

NormalizedEvent WindowDestroyed(int window) {
  NormalizedEvent e = Event(NormalizedEventType::kWindowDestroyed);
  e.window = LiveWindowKey::FromSessionId(window);
  return e;
}

NormalizedEvent TabInserted(int window, int tab, int order = -1) {
  NormalizedEvent e = Event(NormalizedEventType::kTabInserted);
  e.window = LiveWindowKey::FromSessionId(window);
  e.tab = LiveTabKey::FromSessionId(tab);
  e.order_index = order;
  return e;
}

NormalizedEvent TabRemoved(int window, int tab, TabRemovalKind kind) {
  NormalizedEvent e = Event(NormalizedEventType::kTabRemoved);
  e.window = LiveWindowKey::FromSessionId(window);
  e.tab = LiveTabKey::FromSessionId(tab);
  e.removal_kind = kind;
  return e;
}

NormalizedEvent TabMoved(int window, int tab, int order) {
  NormalizedEvent e = Event(NormalizedEventType::kTabMoved);
  e.window = LiveWindowKey::FromSessionId(window);
  e.tab = LiveTabKey::FromSessionId(tab);
  e.order_index = order;
  return e;
}

NormalizedEvent ActiveTab(int window, int tab) {
  NormalizedEvent e = Event(NormalizedEventType::kActiveTabChanged);
  e.window = LiveWindowKey::FromSessionId(window);
  e.tab = LiveTabKey::FromSessionId(tab);
  return e;
}

class LifecycleCoordinatorTest : public testing::Test {
 protected:
  LifecycleCoordinatorTest()
      : model_(base::BindLambdaForTesting([this]() { return now_; })),
        coordinator_(&model_) {}

  // Helpers reading the model.
  size_t MembershipCount() const {
    return model_.ToSnapshot().memberships.size();
  }
  size_t ArchiveCount() const {
    return model_.ToSnapshot().archived_tabs.size();
  }
  const TabMembershipRecord* MembershipForTab(int tab) {
    const TabMembershipId id =
        model_.FindMembershipIdByTabKey(LiveTabKey::FromSessionId(tab).value());
    return id.is_valid() ? model_.FindMembership(id) : nullptr;
  }
  std::string ActiveWorkspaceKey(int window) {
    return model_
        .ActiveWorkspaceForWindow(LiveWindowKey::FromSessionId(window).value())
        .value();
  }

  base::Time now_ = base::Time::UnixEpoch() + base::Seconds(1000);
  OrganizationModel model_;
  LifecycleCoordinator coordinator_;
};

TEST_F(LifecycleCoordinatorTest, RepeatedWindowDiscoveryIsIdempotent) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  EXPECT_EQ(1u, coordinator_.known_window_count());
  EXPECT_TRUE(model_.default_workspace().is_valid());
  EXPECT_EQ(model_.default_workspace().value(), ActiveWorkspaceKey(1));
}

TEST_F(LifecycleCoordinatorTest, InvalidWindowIgnored) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(0));  // invalid session id
  EXPECT_EQ(0u, coordinator_.known_window_count());
}

TEST_F(LifecycleCoordinatorTest, NewTabAssignedToActiveWorkspace) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  const TabMembershipRecord* m = MembershipForTab(10);
  ASSERT_TRUE(m);
  EXPECT_EQ(model_.default_workspace().value(), m->workspace_id.value());
  EXPECT_EQ(TabRole::kTemporary, m->role);
  EXPECT_EQ(1u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, DuplicateInsertionCreatesNoSecondMembership) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  EXPECT_EQ(1u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest,
       InsertionInUndiscoveredWindowFallsBackToDefault) {
  coordinator_.OnNormalizedEvent(
      WindowDiscovered(1));  // ensures a default exists
  coordinator_.OnNormalizedEvent(
      TabInserted(2, 10));  // window 2 not discovered
  const TabMembershipRecord* m = MembershipForTab(10);
  ASSERT_TRUE(m);
  EXPECT_EQ(model_.default_workspace().value(), m->workspace_id.value());
}

TEST_F(LifecycleCoordinatorTest, ActivationUpdatesLastActive) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  now_ += base::Seconds(60);
  coordinator_.OnNormalizedEvent(ActiveTab(1, 10));
  const TabMembershipRecord* m = MembershipForTab(10);
  ASSERT_TRUE(m);
  EXPECT_EQ(now_, m->last_active_at);
}

TEST_F(LifecycleCoordinatorTest, ActivationSwitchesWindowWorkspace) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  const WorkspaceId other = model_.CreateWorkspace("Other").value();
  // A tab that lives in `other`, activated in window 1, switches the window.
  model_.AddTabMembership(other, LiveTabKey::FromSessionId(20).value(),
                          TabRole::kRetained);
  coordinator_.OnNormalizedEvent(ActiveTab(1, 20));
  EXPECT_EQ(other.value(), ActiveWorkspaceKey(1));
}

TEST_F(LifecycleCoordinatorTest, ActivationOfUntrackedTabCreatesNoMembership) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(ActiveTab(1, 99));  // never inserted
  EXPECT_EQ(0u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest,
       GenuineCloseRemovesMembershipWithoutArchiving) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(
      TabRemoved(1, 10, TabRemovalKind::kGenuineClose));
  EXPECT_EQ(0u, MembershipCount());
  EXPECT_EQ(0u,
            ArchiveCount());  // Chromium owns recently-closed; Seoul doesn't.
}

TEST_F(LifecycleCoordinatorTest, CloseCancellationPreservesMembership) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  NormalizedEvent cancel = Event(NormalizedEventType::kTabCloseCancelled);
  cancel.window = LiveWindowKey::FromSessionId(1);
  cancel.tab = LiveTabKey::FromSessionId(10);
  coordinator_.OnNormalizedEvent(cancel);
  EXPECT_EQ(1u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, DetachPreservesMembershipAndRecordsTransfer) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(
      TabRemoved(1, 10, TabRemovalKind::kTransferOut));
  EXPECT_EQ(1u, MembershipCount());  // preserved
  EXPECT_EQ(1u, coordinator_.pending_transfer_count());
}

TEST_F(LifecycleCoordinatorTest, TransferAcrossTwoWindowsKeepsOneMembership) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(WindowDiscovered(2));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  const std::string ws = MembershipForTab(10)->workspace_id.value();
  coordinator_.OnNormalizedEvent(
      TabRemoved(1, 10, TabRemovalKind::kTransferOut));
  coordinator_.OnNormalizedEvent(TabInserted(2, 10));  // lands in window 2
  EXPECT_EQ(0u, coordinator_.pending_transfer_count());
  EXPECT_EQ(1u, MembershipCount());
  EXPECT_EQ(ws, MembershipForTab(10)->workspace_id.value());  // unchanged
}

TEST_F(LifecycleCoordinatorTest,
       TransferThatNeverArrivesStaysPendingAndBounded) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(
      TabRemoved(1, 10, TabRemovalKind::kTransferOut));
  EXPECT_EQ(1u, coordinator_.pending_transfer_count());
  EXPECT_EQ(1u, MembershipCount());  // no ghost removal
}

TEST_F(LifecycleCoordinatorTest, PendingTransfersAreCappedOldestEvicted) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  const size_t over = LifecycleCoordinator::kMaxPendingTransfers + 1;
  for (size_t i = 0; i < over; ++i) {
    const int tab = static_cast<int>(i) + 1;
    coordinator_.OnNormalizedEvent(TabInserted(1, tab));
    coordinator_.OnNormalizedEvent(
        TabRemoved(1, tab, TabRemovalKind::kTransferOut));
  }
  EXPECT_EQ(LifecycleCoordinator::kMaxPendingTransfers,
            coordinator_.pending_transfer_count());
}

TEST_F(LifecycleCoordinatorTest, MoveWithinWindowUpdatesOrder) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10, /*order=*/0));
  coordinator_.OnNormalizedEvent(TabMoved(1, 10, /*order=*/3));
  EXPECT_EQ(3, MembershipForTab(10)->order);
}

TEST_F(LifecycleCoordinatorTest, ReplacementPreservesLogicalMembership) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  const TabMembershipId before =
      model_.FindMembershipIdByTabKey(LiveTabKey::FromSessionId(10).value());
  NormalizedEvent replace = Event(NormalizedEventType::kTabReplaced);
  replace.window = LiveWindowKey::FromSessionId(1);
  replace.tab = LiveTabKey::FromSessionId(10);
  coordinator_.OnNormalizedEvent(replace);
  const TabMembershipId after =
      model_.FindMembershipIdByTabKey(LiveTabKey::FromSessionId(10).value());
  EXPECT_EQ(1u, MembershipCount());
  EXPECT_TRUE(before == after);
}

TEST_F(LifecycleCoordinatorTest, TabStripDestructionForgetsWindowKeepsTabs) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  NormalizedEvent destroyed = Event(NormalizedEventType::kTabStripDestroyed);
  destroyed.window = LiveWindowKey::FromSessionId(1);
  coordinator_.OnNormalizedEvent(destroyed);
  EXPECT_EQ(0u, coordinator_.known_window_count());
  EXPECT_EQ(1u, MembershipCount());  // tabs preserved for restoration
}

TEST_F(LifecycleCoordinatorTest, WindowDestroyedPreservesMemberships) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(TabInserted(1, 11));
  coordinator_.OnNormalizedEvent(WindowDestroyed(1));
  EXPECT_EQ(0u, coordinator_.known_window_count());
  EXPECT_EQ(2u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, ShutdownIgnoresLaterEvents) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(Event(NormalizedEventType::kShutdownBegan));
  EXPECT_TRUE(coordinator_.is_shutting_down());
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));  // ignored after shutdown
  EXPECT_EQ(0u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, WindowShutdownRemovalPreservesMembership) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(
      TabRemoved(1, 10, TabRemovalKind::kWindowShutdown));
  EXPECT_EQ(1u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, ReconciliationClearsPendingTransfers) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  coordinator_.OnNormalizedEvent(
      TabRemoved(1, 10, TabRemovalKind::kTransferOut));
  EXPECT_EQ(1u, coordinator_.pending_transfer_count());
  coordinator_.OnNormalizedEvent(
      Event(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(
      Event(NormalizedEventType::kReconciliationCompleted));
  EXPECT_EQ(0u, coordinator_.pending_transfer_count());
}

class ReentrantObserver : public OrganizationModelObserver {
 public:
  explicit ReentrantObserver(LifecycleCoordinator* coordinator)
      : coordinator_(coordinator) {}

  void OnOrganizationChanged(const OrganizationChange& change) override {
    if (change.type == OrganizationChangeType::kMembershipAdded &&
        !reentered_) {
      reentered_ = true;
      coordinator_->OnNormalizedEvent(TabInserted(1, 99));
    }
  }

 private:
  raw_ptr<LifecycleCoordinator> coordinator_;
  bool reentered_ = false;
};

TEST_F(LifecycleCoordinatorTest, ReentrantEventIsQueuedAndApplied) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  ReentrantObserver obs(&coordinator_);
  model_.AddObserver(&obs);
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  model_.RemoveObserver(&obs);
  EXPECT_EQ(2u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, QueueOverflowSurfacesFailure) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  class OverflowObserver : public OrganizationModelObserver {
   public:
    explicit OverflowObserver(LifecycleCoordinator* c) : c_(c) {}
    void OnOrganizationChanged(const OrganizationChange& change) override {
      if (fired_) {
        return;
      }
      fired_ = true;
      for (size_t i = 0; i < LifecycleCoordinator::kMaxQueuedEvents + 1; ++i) {
        NormalizedEvent e = TabInserted(1, static_cast<int>(100 + i));
        c_->OnNormalizedEvent(e);
      }
    }
    bool fired_ = false;
    raw_ptr<LifecycleCoordinator> c_;
  } overflow(&coordinator_);
  model_.AddObserver(&overflow);
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  model_.RemoveObserver(&overflow);
  EXPECT_TRUE(coordinator_.queue_overflow());
}

TEST_F(LifecycleCoordinatorTest, QueueOverflowRequiresReconciliation) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  class OverflowObserver : public OrganizationModelObserver {
   public:
    explicit OverflowObserver(LifecycleCoordinator* c) : c_(c) {}
    void OnOrganizationChanged(const OrganizationChange& change) override {
      if (fired_) {
        return;
      }
      fired_ = true;
      for (size_t i = 0; i < LifecycleCoordinator::kMaxQueuedEvents + 1; ++i) {
        c_->OnNormalizedEvent(TabInserted(1, static_cast<int>(200 + i)));
      }
    }
    bool fired_ = false;
    raw_ptr<LifecycleCoordinator> c_;
  } overflow(&coordinator_);
  model_.AddObserver(&overflow);
  coordinator_.OnNormalizedEvent(TabInserted(1, 10));
  model_.RemoveObserver(&overflow);
  EXPECT_TRUE(coordinator_.reconciliation_required());
}

TEST_F(LifecycleCoordinatorTest, ReconciliationClearsDegradedState) {
  coordinator_.OnNormalizedEvent(
      Event(NormalizedEventType::kReconciliationBegan));
  coordinator_.OnNormalizedEvent(
      Event(NormalizedEventType::kReconciliationCompleted));
  EXPECT_FALSE(coordinator_.reconciliation_required());
  EXPECT_FALSE(coordinator_.queue_overflow());
}

TEST_F(LifecycleCoordinatorTest, RescanSimulationDetectsNewTab) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  NormalizedEvent existing = TabInserted(1, 10, 0);
  existing.insert_kind = TabInsertKind::kExisting;
  coordinator_.OnNormalizedEvent(existing);
  NormalizedEvent discovered = TabInserted(1, 11, 1);
  discovered.insert_kind = TabInsertKind::kExisting;
  coordinator_.OnNormalizedEvent(discovered);
  EXPECT_EQ(2u, MembershipCount());
}

TEST_F(LifecycleCoordinatorTest, ExistingTabInsertKindCreatesMembershipOnce) {
  coordinator_.OnNormalizedEvent(WindowDiscovered(1));
  NormalizedEvent existing = TabInserted(1, 10, 0);
  existing.insert_kind = TabInsertKind::kExisting;
  coordinator_.OnNormalizedEvent(existing);
  coordinator_.OnNormalizedEvent(existing);
  EXPECT_EQ(1u, MembershipCount());
}

}  // namespace
}  // namespace seoul
