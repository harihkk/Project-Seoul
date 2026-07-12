// Project Seoul native browser shell V0.

#include "seoul/browser/shell/command_launcher_catalog.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(CommandLauncherCatalogTest, DeterministicRanking) {
  ShellSnapshot snapshot;
  auto entries = CommandLauncherCatalog::BuildEntries(snapshot);
  ASSERT_FALSE(entries.empty());
  auto filtered = CommandLauncherCatalog::Filter(entries, "tab");
  ASSERT_FALSE(filtered.empty());
  EXPECT_EQ(filtered.front().id, "new_tab");
}

TEST(CommandLauncherCatalogTest, UnknownQueryReturnsEmpty) {
  ShellSnapshot snapshot;
  auto entries = CommandLauncherCatalog::BuildEntries(snapshot);
  auto filtered = CommandLauncherCatalog::Filter(entries, "zzzz-not-a-command");
  EXPECT_TRUE(filtered.empty());
}

TEST(CommandLauncherCatalogTest, EveryEntryIsExecutableAndSearchable) {
  ShellSnapshot snapshot;
  for (ShellUtilityAction action : {
           ShellUtilityAction::kNewTemporaryTab,
           ShellUtilityAction::kCreateSplit,
           ShellUtilityAction::kOpenCanvas,
           ShellUtilityAction::kOpenTaskDeck,
           ShellUtilityAction::kReconcile,
       }) {
    ShellActionEnablement enabled;
    enabled.action = action;
    enabled.enabled = true;
    snapshot.actions.push_back(enabled);
  }
  const auto entries = CommandLauncherCatalog::BuildEntries(snapshot);
  ASSERT_EQ(entries.size(), 5u);
  for (const CommandLauncherEntry& entry : entries) {
    EXPECT_NE(entry.action, ShellUtilityAction::kCommandLauncher);
    const auto found = CommandLauncherCatalog::Filter(entries, entry.label);
    ASSERT_FALSE(found.empty());
    EXPECT_EQ(found.front().id, entry.id);
  }
}

}  // namespace
}  // namespace seoul
