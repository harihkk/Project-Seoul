// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_TARGET_RESOLVER_H_
#define SEOUL_BROWSER_COMMANDS_TARGET_RESOLVER_H_

#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_errors.h"

class Profile;

namespace seoul {

struct ResolvedWindowTarget {
  LiveWindowKey window;
};

struct ResolvedTabTarget {
  LiveWindowKey window;
  LiveTabKey tab;
  int current_index = -1;
  bool already_active = false;
  bool already_pinned = false;
};

struct ResolvedSplitTarget {
  ResolvedSplitTarget();
  ResolvedSplitTarget(const ResolvedSplitTarget&);
  ResolvedSplitTarget(ResolvedSplitTarget&&);
  ResolvedSplitTarget& operator=(const ResolvedSplitTarget&);
  ResolvedSplitTarget& operator=(ResolvedSplitTarget&&);
  ~ResolvedSplitTarget();

  LiveWindowKey window;
  LiveTabKey pane_a;
  LiveTabKey pane_b;
  std::string upstream_split_token;
  int pane_a_index = -1;
  int pane_b_index = -1;
};

class TargetResolver {
 public:
  virtual ~TargetResolver() = default;

  virtual CommandResult<ResolvedWindowTarget> ResolveWindow(
      Profile* profile,
      LiveWindowKey window) = 0;

  virtual CommandResult<ResolvedTabTarget> ResolveTab(Profile* profile,
                                                      LiveWindowKey window,
                                                      LiveTabKey tab) = 0;

  virtual CommandResult<ResolvedSplitTarget> ResolveSplitPanes(
      Profile* profile,
      LiveWindowKey window,
      LiveTabKey pane_a,
      LiveTabKey pane_b) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_TARGET_RESOLVER_H_
