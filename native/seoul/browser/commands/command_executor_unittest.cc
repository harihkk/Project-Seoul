// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/command_executor.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/organization/organization_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class FakeResolver : public TargetResolver {
 public:
  CommandResult<ResolvedWindowTarget> ResolveWindow(
      Profile* profile,
      LiveWindowKey window) override {
    return ResolvedWindowTarget{window};
  }
  CommandResult<ResolvedTabTarget> ResolveTab(Profile* profile,
                                              LiveWindowKey window,
                                              LiveTabKey tab) override {
    return base::unexpected(CommandError::kTabNotFound);
  }
  CommandResult<ResolvedSplitTarget> ResolveSplitPanes(
      Profile* profile,
      LiveWindowKey window,
      LiveTabKey pane_a,
      LiveTabKey pane_b) override {
    return base::unexpected(CommandError::kSplitPreconditionFailure);
  }
};

class FakeAdapter : public ChromiumMutationAdapter {
 public:
  CommandStatusResult OpenNewTab(Profile* profile,
                                 const ResolvedWindowTarget& window,
                                 CommandForegroundDisposition disposition,
                                 LiveTabKey* out_tab) override {
    return CommandOk();
  }
  CommandStatusResult OpenTab(Profile* profile,
                              const ResolvedWindowTarget& window,
                              const GURL& url,
                              CommandForegroundDisposition disposition,
                              LiveTabKey* out_tab) override {
    return CommandOk();
  }
  CommandStatusResult ActivateTab(Profile* profile,
                                  const ResolvedTabTarget& tab) override {
    return CommandOk();
  }
  CommandStatusResult CloseTab(Profile* profile,
                               const ResolvedTabTarget& tab) override {
    return CommandOk();
  }
  CommandStatusResult SetPinned(Profile* profile,
                                const ResolvedTabTarget& tab,
                                bool pinned) override {
    return CommandOk();
  }
  CommandStatusResult MoveTab(Profile* profile,
                              const ResolvedTabTarget& tab,
                              int destination_index) override {
    return CommandOk();
  }
  CommandStatusResult CreateSplit(Profile* profile,
                                  const ResolvedSplitTarget& split,
                                  double ratio,
                                  std::string* upstream_token) override {
    return CommandOk();
  }
  CommandStatusResult DissolveSplit(
      Profile* profile,
      LiveWindowKey window,
      const std::string& upstream_token) override {
    return CommandOk();
  }
};

class CommandExecutorTest : public testing::Test {
 protected:
  CommandExecutorTest()
      : model_(base::BindLambdaForTesting([]() { return base::Time(); })),
        coordinator_(&model_),
        executor_(nullptr, &model_, &coordinator_, &resolver_, &adapter_) {
    model_.EnsureDefaultWorkspace();
  }

  OrganizationModel model_;
  LifecycleCoordinator coordinator_;
  FakeResolver resolver_;
  FakeAdapter adapter_;
  CommandExecutor executor_;
};

TEST_F(CommandExecutorTest, RejectsReconciliationRequiredBrowserCommand) {
  bool requested = false;
  coordinator_.SetReconciliationRequestCallback(
      base::BindLambdaForTesting([&requested]() { requested = true; }));
  class OverflowObserver : public OrganizationModelObserver {
   public:
    explicit OverflowObserver(LifecycleCoordinator* c) : c_(c) {}
    void OnOrganizationChanged(const OrganizationChange& change) override {
      if (fired_) {
        return;
      }
      fired_ = true;
      for (size_t i = 0; i < LifecycleCoordinator::kMaxQueuedEvents + 1; ++i) {
        NormalizedEvent e;
        e.type = NormalizedEventType::kTabInserted;
        e.window = LiveWindowKey::FromSessionId(1);
        e.tab = LiveTabKey::FromSessionId(static_cast<int>(300 + i));
        c_->OnNormalizedEvent(e);
      }
    }
    bool fired_ = false;
    raw_ptr<LifecycleCoordinator> c_;
  } overflow(&coordinator_);
  model_.AddObserver(&overflow);
  NormalizedEvent window;
  window.type = NormalizedEventType::kWindowDiscovered;
  window.window = LiveWindowKey::FromSessionId(1);
  coordinator_.OnNormalizedEvent(window);
  coordinator_.OnNormalizedEvent(
      NormalizedEvent{NormalizedEventType::kTabInserted});
  model_.RemoveObserver(&overflow);
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = CommandKind::kActivateTab;
  command.window = LiveWindowKey::FromSessionId(1);
  command.tab = LiveTabKey::FromSessionId(10);
  EXPECT_TRUE(coordinator_.reconciliation_required());
  EXPECT_EQ(executor_.Submit(command).error(),
            CommandError::kReconciliationRequired);
}

TEST_F(CommandExecutorTest, ModelOnlyCommandAppliedImmediately) {
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = CommandKind::kCreateWorkspace;
  command.name = "Another";
  auto result = executor_.Submit(command);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), CommandStatus::kApplied);
}

}  // namespace
}  // namespace seoul
