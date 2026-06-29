// Project Seoul outbound browser command layer.

#include "base/test/bind.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_executor.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/commands/url_policy.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(CommandValidationTest, RejectsInvalidCommandId) {
  OrganizationModel model;
  LifecycleCoordinator coordinator(&model);
  CommandExecutor executor(nullptr, &model, &coordinator, nullptr, nullptr);
  BrowserCommand command;
  command.kind = CommandKind::kCreateWorkspace;
  command.name = "X";
  EXPECT_EQ(executor.Submit(command).error(), CommandError::kInvalidCommand);
}

TEST(CommandValidationTest, RejectsUnsupportedUrlScheme) {
  OrganizationModel model;
  LifecycleCoordinator coordinator(&model);
  CommandExecutor executor(nullptr, &model, &coordinator, nullptr, nullptr);
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = CommandKind::kOpenTemporaryTab;
  command.url = GURL("chrome://settings");
  EXPECT_EQ(executor.Submit(command).error(),
            CommandError::kUnsupportedUrlScheme);
}

}  // namespace
}  // namespace seoul
