// Project Seoul native browser shell V0.

#include "seoul/browser/shell/command_launcher_catalog.h"

#include <algorithm>

namespace seoul {
namespace {

CommandLauncherEntry MakeEntry(const std::string& id,
                               const std::string& label,
                               std::initializer_list<std::string> tokens,
                               bool enabled,
                               const std::string& disabled_reason) {
  CommandLauncherEntry entry;
  entry.id = id;
  entry.label = label;
  entry.tokens.assign(tokens);
  entry.enabled = enabled;
  entry.disabled_reason = disabled_reason;
  return entry;
}

bool MatchesQuery(const CommandLauncherEntry& entry, std::string_view query) {
  if (query.empty()) {
    return true;
  }
  std::string lower_query(query);
  std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                 ::tolower);
  auto contains = [&](const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find(lower_query) != std::string::npos;
  };
  if (contains(entry.label) || contains(entry.id)) {
    return true;
  }
  for (const std::string& token : entry.tokens) {
    if (contains(token)) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<CommandLauncherEntry> CommandLauncherCatalog::BuildEntries(
    const ShellSnapshot& snapshot) {
  auto action_enabled = [&](ShellUtilityAction action) {
    for (const ShellActionEnablement& entry : snapshot.actions) {
      if (entry.action == action) {
        return entry;
      }
    }
    return ShellActionEnablement();
  };

  const ShellActionEnablement new_tab =
      action_enabled(ShellUtilityAction::kNewTemporaryTab);
  const ShellActionEnablement split =
      action_enabled(ShellUtilityAction::kCreateSplit);
  const ShellActionEnablement reconcile =
      action_enabled(ShellUtilityAction::kReconcile);

  std::vector<CommandLauncherEntry> entries;
  entries.push_back(MakeEntry("new_tab", "Open New Tab",
                              {"open", "tab", "temporary"}, new_tab.enabled,
                              new_tab.disabled_reason));
  entries.back().action = ShellUtilityAction::kNewTemporaryTab;
  entries.push_back(MakeEntry("create_split", "Create Split", {"split", "pane"},
                              split.enabled, split.disabled_reason));
  entries.back().action = ShellUtilityAction::kCreateSplit;
  entries.push_back(MakeEntry("command_launcher", "Command Launcher",
                              {"command", "palette"}, true, ""));
  entries.push_back(MakeEntry("reconcile", "Run Reconciliation",
                              {"reconcile", "recovery", "degraded"},
                              reconcile.enabled, reconcile.disabled_reason));
  entries.back().action = ShellUtilityAction::kReconcile;
  return entries;
}

std::vector<CommandLauncherEntry> CommandLauncherCatalog::Filter(
    const std::vector<CommandLauncherEntry>& entries,
    std::string_view query) {
  std::vector<CommandLauncherEntry> filtered;
  for (const CommandLauncherEntry& entry : entries) {
    if (MatchesQuery(entry, query)) {
      filtered.push_back(entry);
    }
  }
  return filtered;
}

}  // namespace seoul
