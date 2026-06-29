// Project Seoul native organization engine.
// Unit tests for split groups over Chromium's split model. Authored for a
// capable compile host.

#include <vector>

#include "base/test/bind.h"
#include "seoul/browser/organization/organization_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class SplitsTest : public testing::Test {
 protected:
  SplitsTest() {
    EXPECT_TRUE(model_.EnsureDefaultWorkspace().has_value());
    ws_ = model_.default_workspace();
    other_ = model_.CreateWorkspace("Other").value();
    model_.AddTabMembership(ws_, "tab-a", TabRole::kRetained);
    model_.AddTabMembership(ws_, "tab-b", TabRole::kRetained);
    model_.AddTabMembership(other_, "tab-c", TabRole::kRetained);
  }

  OrganizationModel model_;
  WorkspaceId ws_;
  WorkspaceId other_;
};

TEST_F(SplitsTest, CreateTwoPaneSplit) {
  auto s =
      model_.CreateSplitGroup(ws_, {"tab-a", "tab-b"}, 0.5, "split-token-1");
  ASSERT_TRUE(s.has_value());
  const SplitGroupRecord* rec = model_.FindSplit(s.value());
  ASSERT_TRUE(rec);
  EXPECT_EQ(rec->pane_tab_keys.size(), 2u);
  EXPECT_EQ(rec->active_pane_index, 0);
}

TEST_F(SplitsTest, RejectsInvalidArityAndDuplicatePane) {
  EXPECT_EQ(model_.CreateSplitGroup(ws_, {"tab-a"}, 0.5, "t").error(),
            OrganizationError::kInvalidSplitArity);
  EXPECT_EQ(model_.CreateSplitGroup(ws_, {"tab-a", "tab-a"}, 0.5, "t").error(),
            OrganizationError::kInvalidSplitArity);
  // v0 supports exactly two panes.
  EXPECT_EQ(model_.CreateSplitGroup(ws_, {"tab-a", "tab-b", "tab-c"}, 0.5, "t")
                .error(),
            OrganizationError::kInvalidSplitArity);
}

TEST_F(SplitsTest, RejectsCrossWorkspaceReference) {
  EXPECT_EQ(model_.CreateSplitGroup(ws_, {"tab-a", "tab-c"}, 0.5, "t").error(),
            OrganizationError::kCrossWorkspaceSplit);
  EXPECT_EQ(
      model_.CreateSplitGroup(ws_, {"tab-a", "missing"}, 0.5, "t").error(),
      OrganizationError::kTabMembershipNotFound);
}

TEST_F(SplitsTest, DividerAndActivePaneValidation) {
  EXPECT_EQ(model_.CreateSplitGroup(ws_, {"tab-a", "tab-b"}, 0.99, "t").error(),
            OrganizationError::kInvalidDividerRatio);

  auto s = model_.CreateSplitGroup(ws_, {"tab-a", "tab-b"}, 0.5, "t").value();
  EXPECT_EQ(model_.UpdateSplitLayout(s, 0.5, 9).error(),
            OrganizationError::kInvalidActivePane);
  EXPECT_EQ(model_.UpdateSplitLayout(s, 0.001, 1).error(),
            OrganizationError::kInvalidDividerRatio);
  ASSERT_TRUE(model_.UpdateSplitLayout(s, 0.6, 1).has_value());
  EXPECT_EQ(model_.FindSplit(s)->active_pane_index, 1);
  EXPECT_DOUBLE_EQ(model_.FindSplit(s)->divider_ratio, 0.6);
}

TEST_F(SplitsTest, DissolveAndImplicitDissolveOnTabRemoval) {
  auto s = model_.CreateSplitGroup(ws_, {"tab-a", "tab-b"}, 0.5, "t").value();
  EXPECT_EQ(model_.split_count(), 1u);

  // Removing one participating tab drops the split below two panes ->
  // dissolved.
  auto m = model_.AddTabMembership(ws_, "tab-d", TabRole::kRetained).value();
  auto s2 = model_.CreateSplitGroup(ws_, {"tab-a", "tab-d"}, 0.5, "t2");
  // tab-a is already a member of split s; v0 allows a tab in at most the splits
  // referencing it; this creates a second split referencing tab-a and tab-d.
  ASSERT_TRUE(s2.has_value());

  ASSERT_TRUE(model_.RemoveTabMembership(m).has_value());  // removes tab-d
  // s2 lost a pane and dissolved; s is unaffected.
  EXPECT_FALSE(model_.FindSplit(s2.value()));
  EXPECT_TRUE(model_.FindSplit(s));

  ASSERT_TRUE(model_.DissolveSplitGroup(s).has_value());
  EXPECT_EQ(model_.split_count(), 0u);
  EXPECT_EQ(model_.DissolveSplitGroup(s).error(),
            OrganizationError::kSplitGroupNotFound);
}

TEST_F(SplitsTest, MovingParticipatingTabDissolvesSplit) {
  auto s = model_.CreateSplitGroup(ws_, {"tab-a", "tab-b"}, 0.5, "t").value();
  auto* m = model_.FindMembership(model_.ToSnapshot().memberships.front().id);
  (void)m;
  // Find the membership id for tab-a and move it to `other_`.
  TabMembershipId tab_a_id;
  for (const auto& rec : model_.ToSnapshot().memberships) {
    if (rec.tab_key == "tab-a") {
      tab_a_id = rec.id;
    }
  }
  ASSERT_TRUE(tab_a_id.is_valid());
  ASSERT_TRUE(model_.MoveTabToWorkspace(tab_a_id, other_).has_value());
  // The split referenced a tab that left the workspace, so it was dissolved.
  EXPECT_FALSE(model_.FindSplit(s));
}

TEST_F(SplitsTest, SplitSurvivesSnapshotRoundTrip) {
  model_.CreateSplitGroup(ws_, {"tab-a", "tab-b"}, 0.5, "token-keep");
  OrganizationSnapshot snap = model_.ToSnapshot();

  OrganizationModel other;
  ASSERT_TRUE(other.LoadSnapshot(snap).has_value());
  EXPECT_EQ(other.split_count(), 1u);
}

}  // namespace
}  // namespace seoul
