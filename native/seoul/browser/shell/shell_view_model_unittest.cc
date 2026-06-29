// Project Seoul native browser shell V0.

#include "seoul/browser/shell/shell_view_model.h"

#include "seoul/browser/organization/organization_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(ShellViewModelTest, EmptyWorkspaceShowsEmptyState) {
  OrganizationModel model;
  const WorkspaceId ws = model.EnsureDefaultWorkspace();
  ShellBuildContext context;
  context.window = LiveWindowKey::FromSessionId(1);
  model.SetActiveWorkspaceForWindow(context.window.value(), ws);
  WindowProjection projection;
  projection.window = context.window;
  projection.active_workspace = ws;
  projection.empty_workspace = true;
  LiveWindowSnapshot live;
  live.window = context.window;
  ShellSnapshot snapshot =
      ShellViewModel::Build(model, context, projection, live, 1);
  EXPECT_TRUE(snapshot.show_empty_workspace);
  EXPECT_EQ(snapshot.status, ShellStatus::kEmptyWorkspace);
}

TEST(ShellViewModelTest, EssentialsComeFromOrganizationModel) {
  OrganizationModel model;
  model.EnsureDefaultWorkspace();
  model.CreateOrUpdateEssential(EssentialId(), "Mail", "https://mail.example/");
  ShellBuildContext context;
  context.window = LiveWindowKey::FromSessionId(1);
  WindowProjection projection;
  projection.window = context.window;
  LiveWindowSnapshot live;
  ShellSnapshot snapshot =
      ShellViewModel::Build(model, context, projection, live, 1);
  ASSERT_EQ(snapshot.essentials.size(), 1u);
  EXPECT_EQ(snapshot.essentials[0].name, "Mail");
}

}  // namespace
}  // namespace seoul
