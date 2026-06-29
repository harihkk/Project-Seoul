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

}  // namespace
}  // namespace seoul
