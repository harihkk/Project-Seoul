// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/workspace_switcher.h"

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "seoul/browser/commands/command_completion_observer.h"
#include "seoul/browser/commands/command_executor.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/lifecycle/live_window_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class FakeAdapter : public ChromiumMutationAdapter {
 public:
  CommandStatusResult OpenNewTab(Profile*,
                                 const ResolvedWindowTarget&,
                                 CommandForegroundDisposition,
                                 LiveTabKey*) override {
    return CommandOk();
  }
  CommandStatusResult OpenTab(Profile*,
                              const ResolvedWindowTarget&,
                              const GURL&,
                              CommandForegroundDisposition,
                              LiveTabKey*) override {
    return CommandOk();
  }
  CommandStatusResult ActivateTab(Profile*, const ResolvedTabTarget&) override {
    activate_calls_++;
    return CommandOk();
  }
  CommandStatusResult CloseTab(Profile*, const ResolvedTabTarget&) override {
    return CommandOk();
  }
  CommandStatusResult SetPinned(Profile*,
                                const ResolvedTabTarget&,
                                bool) override {
    return CommandOk();
  }
  CommandStatusResult MoveTab(Profile*,
                              const ResolvedTabTarget&,
                              int) override {
    return CommandOk();
  }
  CommandStatusResult CreateSplit(Profile*,
                                  const ResolvedSplitTarget&,
                                  double,
                                  std::string*) override {
    return CommandOk();
  }
  CommandStatusResult DissolveSplit(Profile*,
                                    LiveWindowKey,
                                    const std::string&) override {
    return CommandOk();
  }
  int activate_calls() const { return activate_calls_; }

 private:
  int activate_calls_ = 0;
};

class FakeResolver : public TargetResolver {
 public:
  CommandResult<ResolvedWindowTarget> ResolveWindow(
      Profile*,
      LiveWindowKey window) override {
    return ResolvedWindowTarget{window};
  }
  CommandResult<ResolvedTabTarget> ResolveTab(Profile*,
                                              LiveWindowKey,
                                              LiveTabKey tab) override {
    ResolvedTabTarget t;
    t.tab = tab;
    t.window = LiveWindowKey::FromSessionId(1);
    t.current_index = 0;
    return t;
  }
  CommandResult<ResolvedSplitTarget> ResolveSplitPanes(Profile*,
                                                       LiveWindowKey,
                                                       LiveTabKey,
                                                       LiveTabKey) override {
    return base::unexpected(CommandError::kSplitPreconditionFailure);
  }
};

void SeedLiveState(LiveWindowStateProvider* provider,
                   LiveWindowKey window,
                   LiveTabKey active) {
  LiveWindowSnapshot snapshot;
  snapshot.window = window;
  snapshot.active_tab = active;
  LiveTabDescriptor tab;
  tab.tab = active;
  tab.strip_order = 0;
  snapshot.tabs.push_back(tab);
  provider->SetSnapshotForTesting(window, snapshot);
}

TEST(WorkspaceSwitcherTest, RejectsArchivedWorkspace) {
  OrganizationModel model;
  model.EnsureDefaultWorkspace();
  const WorkspaceId archived = model.CreateWorkspace("arch").value();
  model.ArchiveWorkspace(archived);
  LifecycleCoordinator coordinator(&model);
  LiveWindowStateProvider live_state;
  FakeResolver resolver;
  FakeAdapter adapter;
  CommandExecutor executor(nullptr, &model, &coordinator, &resolver, &adapter);
  WindowProjectionController controller(LiveWindowKey::FromSessionId(1), &model,
                                        &coordinator);
  WorkspaceSwitcher switcher(nullptr, &model, &executor, &controller,
                             &live_state);
  auto result = switcher.SwitchWorkspaceForWindow(archived);
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ProjectionError::kArchivedWorkspace);
}

TEST(WorkspaceSwitcherTest, CommitsWhenTargetTabAlreadyActive) {
  OrganizationModel model;
  ASSERT_TRUE(model.EnsureDefaultWorkspace().has_value());
  const WorkspaceId ws_a = model.default_workspace();
  const WorkspaceId ws_b = model.CreateWorkspace("b").value();
  const LiveWindowKey window = LiveWindowKey::FromSessionId(1);
  const LiveTabKey tab = LiveTabKey::FromSessionId(10);
  model.SetActiveWorkspaceForWindow(window.value(), ws_a);
  ASSERT_TRUE(model.AddTabMembership(ws_b, tab.value(), TabRole::kTemporary)
                  .has_value());
  LifecycleCoordinator coordinator(&model);
  LiveWindowStateProvider live_state;
  SeedLiveState(&live_state, window, tab);
  FakeResolver resolver;
  FakeAdapter adapter;
  CommandExecutor executor(nullptr, &model, &coordinator, &resolver, &adapter);
  WindowProjectionController controller(window, &model, &coordinator);
  controller.UpdateLiveState(live_state.GetSnapshot(window).value());
  WorkspaceSwitcher switcher(nullptr, &model, &executor, &controller,
                             &live_state);
  auto result = switcher.SwitchWorkspaceForWindow(ws_b);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->phase, WorkspaceSwitchPhase::kApplied);
  EXPECT_EQ(model.ActiveWorkspaceForWindow(window.value()), ws_b);
  EXPECT_EQ(adapter.activate_calls(), 0);
}

TEST(WorkspaceSwitcherTest, ExternalActivationDoesNotDispatchCommand) {
  OrganizationModel model;
  ASSERT_TRUE(model.EnsureDefaultWorkspace().has_value());
  const WorkspaceId ws_a = model.default_workspace();
  const WorkspaceId ws_b = model.CreateWorkspace("b").value();
  const LiveWindowKey window = LiveWindowKey::FromSessionId(1);
  const LiveTabKey tab = LiveTabKey::FromSessionId(11);
  model.SetActiveWorkspaceForWindow(window.value(), ws_a);
  ASSERT_TRUE(model.AddTabMembership(ws_b, tab.value(), TabRole::kTemporary)
                  .has_value());
  LifecycleCoordinator coordinator(&model);
  LiveWindowStateProvider live_state;
  SeedLiveState(&live_state, window, tab);
  FakeResolver resolver;
  FakeAdapter adapter;
  CommandExecutor executor(nullptr, &model, &coordinator, &resolver, &adapter);
  WindowProjectionController controller(window, &model, &coordinator);
  WorkspaceSwitcher switcher(nullptr, &model, &executor, &controller,
                             &live_state);
  auto result = switcher.SwitchWorkspaceForWindowExternalActivation(ws_b, tab);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(model.ActiveWorkspaceForWindow(window.value()), ws_b);
  EXPECT_EQ(adapter.activate_calls(), 0);
}

}  // namespace
}  // namespace seoul
