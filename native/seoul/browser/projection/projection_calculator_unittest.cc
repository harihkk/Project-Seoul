// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/projection_calculator.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "seoul/browser/organization/organization_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

LiveTabKey Tab(int id) {
  return LiveTabKey::FromSessionId(id);
}

LiveWindowTabState MakeLive(const std::vector<std::pair<int, int>>& tabs) {
  LiveWindowTabState live;
  live.window = LiveWindowKey::FromSessionId(1);
  for (const auto& [session, order] : tabs) {
    LiveTabDescriptor d;
    d.tab = Tab(session);
    d.strip_order = order;
    live.tabs.push_back(d);
  }
  return live;
}

TEST(ProjectionCalculatorTest, ProjectsSingleWorkspaceTabs) {
  OrganizationModel model(base::BindLambdaForTesting(
      []() { return base::Time::FromSecondsSinceUnixEpoch(100); }));
  model.EnsureDefaultWorkspace();
  const WorkspaceId ws = model.default_workspace();
  model.AddTabMembership(ws, Tab(10).value(), TabRole::kRetained);
  model.AddTabMembership(ws, Tab(20).value(), TabRole::kTemporary);

  LiveWindowTabState live = MakeLive({{10, 0}, {20, 1}, {30, 2}});
  model.AddTabMembership(ws, Tab(30).value(), TabRole::kRetained);
  const WorkspaceId other = model.CreateWorkspace("other").value();
  model.MoveTabToWorkspace(model.FindMembershipIdByTabKey(Tab(30).value()),
                           other);

  WindowProjection projection = ProjectionCalculator::Compute(
      model, live, ws, ProjectionGeneration(1), false);
  EXPECT_EQ(projection.tabs.size(), 2u);
  EXPECT_FALSE(projection.empty_workspace);
}

TEST(ProjectionCalculatorTest, EmptyWorkspaceState) {
  OrganizationModel model;
  model.EnsureDefaultWorkspace();
  LiveWindowTabState live = MakeLive({});
  WindowProjection projection = ProjectionCalculator::Compute(
      model, live, model.default_workspace(), ProjectionGeneration(1), false);
  EXPECT_TRUE(projection.empty_workspace);
  EXPECT_EQ(projection.status, ProjectionStatus::kEmptyWorkspace);
}

TEST(ProjectionCalculatorTest, FailOpenShowsAllTabs) {
  OrganizationModel model;
  model.EnsureDefaultWorkspace();
  LiveWindowTabState live = MakeLive({{10, 0}, {20, 1}});
  WindowProjection projection = ProjectionCalculator::Compute(
      model, live, model.default_workspace(), ProjectionGeneration(1), true);
  EXPECT_EQ(projection.status, ProjectionStatus::kFailOpen);
  EXPECT_EQ(projection.tabs.size(), 2u);
}

}  // namespace
}  // namespace seoul
