// Project Seoul native organization engine.
// Unit tests for OrganizationModel (workspaces, membership, essentials,
// archive, observers). Authored for a capable compile host; not run on the
// authoring machine (8 GiB, no GN/build).

#include "seoul/browser/organization/organization_model.h"

#include <map>
#include <vector>

#include "base/test/bind.h"
#include "base/time/time.h"
#include "seoul/browser/organization/organization_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class RecordingObserver : public OrganizationModelObserver {
 public:
  void OnOrganizationChanged(const OrganizationChange& change) override {
    changes.push_back(change.type);
  }
  std::vector<OrganizationChangeType> changes;
};

class OrganizationModelTest : public testing::Test {
 protected:
  OrganizationModelTest()
      : model_(base::BindLambdaForTesting([this]() { return clock_; })) {}

  void Advance(base::TimeDelta d) { clock_ += d; }

  WorkspaceId InitDefault() {
    EXPECT_TRUE(model_.EnsureDefaultWorkspace().has_value());
    return model_.default_workspace();
  }

  base::Time clock_ = base::Time::UnixEpoch() + base::Days(100);
  OrganizationModel model_;
};

TEST_F(OrganizationModelTest, FirstRunCreatesExactlyOneDefault) {
  EXPECT_EQ(model_.workspace_count(), 0u);
  ASSERT_TRUE(model_.EnsureDefaultWorkspace().has_value());
  EXPECT_EQ(model_.workspace_count(), 1u);
  WorkspaceId def = model_.default_workspace();
  ASSERT_TRUE(def.is_valid());
  const WorkspaceRecord* w = model_.FindWorkspace(def);
  ASSERT_TRUE(w);
  EXPECT_TRUE(w->is_default);
  EXPECT_FALSE(w->archived);
}

TEST_F(OrganizationModelTest, EnsureDefaultIsIdempotent) {
  ASSERT_TRUE(model_.EnsureDefaultWorkspace().has_value());
  WorkspaceId first = model_.default_workspace();
  ASSERT_TRUE(model_.EnsureDefaultWorkspace().has_value());
  ASSERT_TRUE(model_.EnsureDefaultWorkspace().has_value());
  EXPECT_EQ(model_.workspace_count(), 1u);
  EXPECT_TRUE(model_.default_workspace() == first);
}

TEST_F(OrganizationModelTest, CreateRenameReorder) {
  InitDefault();
  auto created = model_.CreateWorkspace("Work");
  ASSERT_TRUE(created.has_value());
  WorkspaceId id = created.value();
  EXPECT_EQ(model_.workspace_count(), 2u);

  EXPECT_TRUE(model_.RenameWorkspace(id, "Research").has_value());
  EXPECT_EQ(model_.FindWorkspace(id)->name, "Research");

  EXPECT_TRUE(model_.ReorderWorkspace(id, 5).has_value());
  EXPECT_EQ(model_.FindWorkspace(id)->order, 5);

  auto bad_name = model_.CreateWorkspace("   ");
  ASSERT_FALSE(bad_name.has_value());
  EXPECT_EQ(bad_name.error(), OrganizationError::kInvalidName);

  EXPECT_EQ(model_.ReorderWorkspace(id, -1).error(),
            OrganizationError::kInvalidOrder);
  EXPECT_EQ(model_.RenameWorkspace(WorkspaceId::GenerateNew(), "x").error(),
            OrganizationError::kWorkspaceNotFound);
}

TEST_F(OrganizationModelTest, ArchiveRestoreAndDefaultProtection) {
  WorkspaceId def = InitDefault();
  WorkspaceId work = model_.CreateWorkspace("Work").value();

  EXPECT_EQ(model_.ArchiveWorkspace(def).error(),
            OrganizationError::kDefaultWorkspaceProtected);
  EXPECT_EQ(model_.DeleteWorkspace(def).error(),
            OrganizationError::kDefaultWorkspaceProtected);

  ASSERT_TRUE(model_.ArchiveWorkspace(work).has_value());
  EXPECT_TRUE(model_.FindWorkspace(work)->archived);
  // An archived workspace cannot be activated.
  EXPECT_EQ(model_.SetActiveWorkspaceForWindow("win-1", work).error(),
            OrganizationError::kArchivedWorkspaceCannotActivate);

  ASSERT_TRUE(model_.RestoreWorkspace(work).has_value());
  EXPECT_FALSE(model_.FindWorkspace(work)->archived);
}

TEST_F(OrganizationModelTest,
       ArchivingActiveWorkspacePicksDeterministicFallback) {
  WorkspaceId def = InitDefault();
  WorkspaceId work = model_.CreateWorkspace("Work").value();
  ASSERT_TRUE(model_.SetActiveWorkspaceForWindow("win-1", work).has_value());
  EXPECT_TRUE(model_.ActiveWorkspaceForWindow("win-1") == work);

  ASSERT_TRUE(model_.ArchiveWorkspace(work).has_value());
  // Fallback prefers the default workspace.
  EXPECT_TRUE(model_.ActiveWorkspaceForWindow("win-1") == def);
}

TEST_F(OrganizationModelTest, DeleteCascadesMembershipsAndReassignsWindows) {
  WorkspaceId def = InitDefault();
  WorkspaceId work = model_.CreateWorkspace("Work").value();
  ASSERT_TRUE(
      model_.AddTabMembership(work, "tab-a", TabRole::kTemporary).has_value());
  ASSERT_TRUE(model_.SetActiveWorkspaceForWindow("win-1", work).has_value());

  ASSERT_TRUE(model_.DeleteWorkspace(work).has_value());
  EXPECT_EQ(model_.workspace_count(), 1u);
  EXPECT_EQ(model_.membership_count(), 0u);
  EXPECT_TRUE(model_.ActiveWorkspaceForWindow("win-1") == def);
}

TEST_F(OrganizationModelTest, MembershipAddDuplicateRemoveMove) {
  WorkspaceId def = InitDefault();
  WorkspaceId work = model_.CreateWorkspace("Work").value();

  auto m = model_.AddTabMembership(def, "tab-a", TabRole::kTemporary);
  ASSERT_TRUE(m.has_value());
  // A tab_key belongs to at most one workspace: duplicate rejected.
  EXPECT_EQ(model_.AddTabMembership(work, "tab-a", TabRole::kTemporary).error(),
            OrganizationError::kDuplicateMembership);

  ASSERT_TRUE(model_.MoveTabToWorkspace(m.value(), work).has_value());
  EXPECT_TRUE(model_.FindMembership(m.value())->workspace_id == work);
  EXPECT_EQ(model_.MoveTabToWorkspace(m.value(), work).error(),
            OrganizationError::kNoOpRejected);

  ASSERT_TRUE(model_.RemoveTabMembership(m.value()).has_value());
  EXPECT_EQ(model_.membership_count(), 0u);
  EXPECT_EQ(model_.RemoveTabMembership(m.value()).error(),
            OrganizationError::kTabMembershipNotFound);
}

TEST_F(OrganizationModelTest, TabRoleTransitions) {
  WorkspaceId def = InitDefault();
  auto m = model_.AddTabMembership(def, "tab-a", TabRole::kTemporary).value();

  ASSERT_TRUE(model_.RetainTab(m).has_value());
  EXPECT_EQ(model_.FindMembership(m)->role, TabRole::kRetained);

  ASSERT_TRUE(model_.PinTab(m, "https://example.test/root").has_value());
  EXPECT_EQ(model_.FindMembership(m)->role, TabRole::kPinned);
  EXPECT_EQ(model_.FindMembership(m)->saved_root_url,
            "https://example.test/root");

  // Unpinning keeps the tab (becomes retained), it does not close it.
  ASSERT_TRUE(model_.UnpinTab(m).has_value());
  EXPECT_EQ(model_.FindMembership(m)->role, TabRole::kRetained);
  EXPECT_TRUE(model_.FindMembership(m)->saved_root_url.empty());

  ASSERT_TRUE(model_.MarkTabTemporary(m).has_value());
  EXPECT_EQ(model_.FindMembership(m)->role, TabRole::kTemporary);
}

TEST_F(OrganizationModelTest, EssentialsAreGlobalAndSingleIdentity) {
  InitDefault();
  auto e = model_.CreateOrUpdateEssential(EssentialId(), "Mail",
                                          "https://mail.test/");
  ASSERT_TRUE(e.has_value());
  EXPECT_EQ(model_.essential_count(), 1u);

  // Update in place keeps the same identity (no duplicate live tab created).
  auto e2 = model_.CreateOrUpdateEssential(e.value(), "Mailbox",
                                           "https://mail.test/inbox");
  ASSERT_TRUE(e2.has_value());
  EXPECT_TRUE(e2.value() == e.value());
  EXPECT_EQ(model_.essential_count(), 1u);
  EXPECT_EQ(model_.FindEssential(e.value())->name, "Mailbox");

  EXPECT_EQ(model_
                .CreateOrUpdateEssential(EssentialId::GenerateNew(), "x",
                                         "https://x.test/")
                .error(),
            OrganizationError::kEssentialNotFound);

  ASSERT_TRUE(model_.RemoveEssential(e.value()).has_value());
  EXPECT_EQ(model_.essential_count(), 0u);
}

TEST_F(OrganizationModelTest, AutoArchiveProtectionRules) {
  WorkspaceId def = InitDefault();
  auto temp =
      model_.AddTabMembership(def, "tab-temp", TabRole::kTemporary).value();
  auto retained =
      model_.AddTabMembership(def, "tab-keep", TabRole::kRetained).value();
  auto pinned =
      model_.AddTabMembership(def, "tab-pin", TabRole::kPinned).value();

  const base::TimeDelta threshold = base::Hours(12);
  // Not yet inactive: nothing eligible.
  EXPECT_TRUE(model_.EligibleForAutoArchive({}, clock_, threshold).empty());

  Advance(base::Hours(13));
  base::Time now = clock_;

  // Retained and pinned are never eligible; only the unprotected temporary is.
  std::vector<TabMembershipId> eligible =
      model_.EligibleForAutoArchive({}, now, threshold);
  ASSERT_EQ(eligible.size(), 1u);
  EXPECT_TRUE(eligible[0] == temp);

  // A live condition protects the temporary tab.
  std::map<std::string, TabLiveActivity> activity;
  TabLiveActivity playing;
  playing.playing_media = true;
  activity["tab-temp"] = playing;
  EXPECT_TRUE(model_.EligibleForAutoArchive(activity, now, threshold).empty());

  (void)retained;
  (void)pinned;
}

TEST_F(OrganizationModelTest, ArchiveAndRestoreTab) {
  WorkspaceId def = InitDefault();
  auto m = model_.AddTabMembership(def, "tab-a", TabRole::kTemporary).value();

  ASSERT_TRUE(model_.ArchiveTab(m).has_value());
  EXPECT_EQ(model_.membership_count(), 0u);  // not live anymore
  EXPECT_EQ(model_.archived_count(), 1u);

  auto restored = model_.RestoreArchivedTab(m, "tab-a-restored");
  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(model_.membership_count(), 1u);
  EXPECT_EQ(model_.archived_count(), 0u);
  EXPECT_EQ(model_.FindMembership(restored.value())->role, TabRole::kTemporary);
}

TEST_F(OrganizationModelTest, ObserversOrderedOncePerCommitNoneOnFailure) {
  RecordingObserver obs;
  model_.AddObserver(&obs);

  ASSERT_TRUE(model_.EnsureDefaultWorkspace().has_value());
  ASSERT_TRUE(model_.CreateWorkspace("Work").has_value());
  // Failed mutation emits no notification.
  EXPECT_FALSE(
      model_.RenameWorkspace(WorkspaceId::GenerateNew(), "x").has_value());

  ASSERT_EQ(obs.changes.size(), 2u);
  EXPECT_EQ(obs.changes[0], OrganizationChangeType::kInitialized);
  EXPECT_EQ(obs.changes[1], OrganizationChangeType::kWorkspaceCreated);

  model_.RemoveObserver(&obs);
  ASSERT_TRUE(model_.CreateWorkspace("Another").has_value());
  EXPECT_EQ(obs.changes.size(), 2u);  // no more after removal
}

TEST_F(OrganizationModelTest, SnapshotRoundTripThroughModel) {
  WorkspaceId def = InitDefault();
  WorkspaceId work = model_.CreateWorkspace("Work").value();
  ASSERT_TRUE(
      model_.AddTabMembership(work, "tab-a", TabRole::kPinned).has_value());
  ASSERT_TRUE(
      model_
          .CreateOrUpdateEssential(EssentialId(), "Mail", "https://mail.test/")
          .has_value());

  OrganizationSnapshot snap = model_.ToSnapshot();

  OrganizationModel other;
  ASSERT_TRUE(other.LoadSnapshot(snap).has_value());
  EXPECT_EQ(other.workspace_count(), 2u);
  EXPECT_EQ(other.membership_count(), 1u);
  EXPECT_EQ(other.essential_count(), 1u);
  EXPECT_TRUE(other.default_workspace() == def);
  (void)work;
}

}  // namespace
}  // namespace seoul
