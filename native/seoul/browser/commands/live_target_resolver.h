// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_LIVE_TARGET_RESOLVER_H_
#define SEOUL_BROWSER_COMMANDS_LIVE_TARGET_RESOLVER_H_

#include "seoul/browser/commands/target_resolver.h"

namespace seoul {

class LiveTargetResolver : public TargetResolver {
 public:
  LiveTargetResolver();
  LiveTargetResolver(const LiveTargetResolver&) = delete;
  LiveTargetResolver& operator=(const LiveTargetResolver&) = delete;
  ~LiveTargetResolver() override;

  CommandResult<ResolvedWindowTarget> ResolveWindow(
      Profile* profile,
      LiveWindowKey window) override;

  CommandResult<ResolvedTabTarget> ResolveTab(Profile* profile,
                                              LiveWindowKey window,
                                              LiveTabKey tab) override;

  CommandResult<ResolvedSplitTarget> ResolveSplitPanes(
      Profile* profile,
      LiveWindowKey window,
      LiveTabKey pane_a,
      LiveTabKey pane_b) override;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_LIVE_TARGET_RESOLVER_H_
