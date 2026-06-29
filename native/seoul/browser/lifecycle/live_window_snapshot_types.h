// Project Seoul native lifecycle bridge.
// Ephemeral live-window snapshot types (Chromium-free).

#ifndef SEOUL_BROWSER_LIFECYCLE_LIVE_WINDOW_SNAPSHOT_TYPES_H_
#define SEOUL_BROWSER_LIFECYCLE_LIVE_WINDOW_SNAPSHOT_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "seoul/browser/lifecycle/lifecycle_identity.h"

namespace seoul {

struct LiveTabDescriptor {
  LiveTabKey tab;
  int strip_order = -1;
  bool chromium_pinned = false;
  std::string upstream_split_token;
  std::string upstream_group_token;
  bool is_active = false;

  friend bool operator==(const LiveTabDescriptor&,
                         const LiveTabDescriptor&) = default;
};

struct LiveWindowSnapshot {
  LiveWindowKey window;
  bool eligible = true;
  bool lifecycle_degraded = false;
  std::vector<LiveTabDescriptor> tabs;
  LiveTabKey active_tab;
  uint64_t revision = 0;
};

// Alias used by projection calculator.
using LiveWindowTabState = LiveWindowSnapshot;

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIVE_WINDOW_SNAPSHOT_TYPES_H_
