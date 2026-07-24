// Project Seoul native lifecycle bridge.
// Authored for later compilation on a capable host. Not run on the dev machine.

#include <string>

#include "base/functional/bind.h"
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

NormalizedEvent Insert(int w, int t) {
  NormalizedEvent e = Make(NormalizedEventType::kTabInserted);
  e.window = LiveWindowKey::FromSessionId(w);
  e.tab = LiveTabKey::FromSessionId(t);
  return e;
}

NormalizedEvent SplitAdded(const std::string& token,
                           int a,
                           int b,
                           double ratio) {
  NormalizedEvent e = Make(NormalizedEventType::kSplitAdded);
  e.window = LiveWindowKey::FromSessionId(1);
  e.upstream_split_token = token;
  e.split_pane_a = LiveTabKey::FromSessionId(a);
  e.split_pane_b = LiveTabKey::FromSessionId(b);
  e.divider_ratio = ratio;
  return e;
}

NormalizedEvent SplitVisuals(const std::string& token,
                             double ratio,
                             bool intermediate) {
  NormalizedEvent e = Make(NormalizedEventType::kSplitVisualsChanged);
  e.upstream_split_token = token;
  e.divider_ratio = ratio;
  e.split_visuals_intermediate = intermediate;
  return e;
}

class SplitsTest : public testing::Test {
 protected:
  SplitsTest()
      : model_(base::BindLambdaForTesting([this]() { return now_; })),
        coordinator_(&model_) {}

  const SplitGroupRecord* FindSplit(const std::string& token) {
    const SplitGroupId id = model_.FindSplitIdByUpstreamToken(token);
    return id.is_valid() ? model_.FindSplit(id) : nullptr;
  }
  size_t SplitCount() const { return model_.ToSnapshot().splits.size(); }

  void DiscoverAndInsert(int a, int b) {
    coordinator_.OnNormalizedEvent(Window(1));
    coordinator_.OnNormalizedEvent(Insert(1, a));
    coordinator_.OnNormalizedEvent(Insert(1, b));
  }

  base::Time now_ = base::Time::UnixEpoch() + base::Seconds(1000);
  OrganizationModel model_;
  LifecycleCoordinator coordinator_;
};

TEST_F(SplitsTest, SplitAddedCreatesRecord) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.4));
  const SplitGroupRecord* s = FindSplit("split-abc");
  ASSERT_TRUE(s);
  EXPECT_EQ(model_.default_workspace().value(), s->workspace_id.value());
  EXPECT_EQ(2u, s->pane_tab_keys.size());
  EXPECT_EQ(1u, SplitCount());
}

TEST_F(SplitsTest, DuplicateSplitEventIgnored) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  EXPECT_EQ(1u, SplitCount());
}

TEST_F(SplitsTest, SplitRemovedDissolves) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  NormalizedEvent removed = Make(NormalizedEventType::kSplitRemoved);
  removed.upstream_split_token = "split-abc";
  coordinator_.OnNormalizedEvent(removed);
  EXPECT_EQ(0u, SplitCount());
}

TEST_F(SplitsTest, AtomicContentsReplacementPreservesSplitId) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  const SplitGroupId before = model_.FindSplitIdByUpstreamToken("split-abc");
  coordinator_.OnNormalizedEvent(Insert(1, 12));
  NormalizedEvent changed = Make(NormalizedEventType::kSplitContentsChanged);
  changed.window = LiveWindowKey::FromSessionId(1);
  changed.upstream_split_token = "split-abc";
  changed.split_pane_a = LiveTabKey::FromSessionId(10);
  changed.split_pane_b = LiveTabKey::FromSessionId(12);
  coordinator_.OnNormalizedEvent(changed);
  const SplitGroupId after = model_.FindSplitIdByUpstreamToken("split-abc");
  EXPECT_TRUE(before == after);
  EXPECT_EQ(LiveTabKey::FromSessionId(12).value(),
            FindSplit("split-abc")->pane_tab_keys[1]);
}

TEST_F(SplitsTest, ContentsReplacementPreservesRatioWhenOmitted) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.4));
  coordinator_.OnNormalizedEvent(Insert(1, 12));
  NormalizedEvent changed = Make(NormalizedEventType::kSplitContentsChanged);
  changed.upstream_split_token = "split-abc";
  changed.split_pane_a = LiveTabKey::FromSessionId(10);
  changed.split_pane_b = LiveTabKey::FromSessionId(12);
  changed.has_divider_ratio = false;
  coordinator_.OnNormalizedEvent(changed);
  EXPECT_DOUBLE_EQ(0.4, FindSplit("split-abc")->divider_ratio);
}

TEST_F(SplitsTest, ContentsReplacementRejectsExplicitInvalidRatio) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.4));
  NormalizedEvent changed = Make(NormalizedEventType::kSplitContentsChanged);
  changed.upstream_split_token = "split-abc";
  changed.split_pane_a = LiveTabKey::FromSessionId(10);
  changed.split_pane_b = LiveTabKey::FromSessionId(11);
  changed.has_divider_ratio = true;
  changed.divider_ratio = 99.0;
  coordinator_.OnNormalizedEvent(changed);
  EXPECT_DOUBLE_EQ(0.4, FindSplit("split-abc")->divider_ratio);
}

TEST_F(SplitsTest, IntermediateRatioNotPersistedAndUnchanged) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  coordinator_.OnNormalizedEvent(
      SplitVisuals("split-abc", 0.8, /*intermediate=*/true));
  EXPECT_DOUBLE_EQ(0.5, FindSplit("split-abc")->divider_ratio);
}

TEST_F(SplitsTest, FinalRatioCommitted) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  coordinator_.OnNormalizedEvent(
      SplitVisuals("split-abc", 0.7, /*intermediate=*/false));
  EXPECT_DOUBLE_EQ(0.7, FindSplit("split-abc")->divider_ratio);
}

TEST_F(SplitsTest, CrossWorkspaceSplitRejected) {
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(Insert(1, 10));  // default workspace
  const WorkspaceId other = model_.CreateWorkspace("Other").value();
  ASSERT_TRUE(model_
                  .AddTabMembership(other,
                                    LiveTabKey::FromSessionId(11).value(),
                                    TabRole::kRetained)
                  .has_value());
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  EXPECT_EQ(0u, SplitCount());
}

TEST_F(SplitsTest, UntrackedPaneFailsSafely) {
  coordinator_.OnNormalizedEvent(Window(1));
  coordinator_.OnNormalizedEvent(Insert(1, 10));
  // Pane 99 was never inserted.
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 99, 0.5));
  EXPECT_EQ(0u, SplitCount());
}

TEST_F(SplitsTest, ClosingOnePaneDissolvesSplit) {
  DiscoverAndInsert(10, 11);
  coordinator_.OnNormalizedEvent(SplitAdded("split-abc", 10, 11, 0.5));
  ASSERT_EQ(1u, SplitCount());
  NormalizedEvent close = Make(NormalizedEventType::kTabRemoved);
  close.window = LiveWindowKey::FromSessionId(1);
  close.tab = LiveTabKey::FromSessionId(10);
  close.removal_kind = TabRemovalKind::kGenuineClose;
  coordinator_.OnNormalizedEvent(close);
  EXPECT_EQ(0u, SplitCount());  // model drops the pane and dissolves below two
}

}  // namespace
}  // namespace seoul
