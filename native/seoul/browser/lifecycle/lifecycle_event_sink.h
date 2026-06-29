// Project Seoul native lifecycle bridge.
// The seam between the thin Chromium adapters and the coordinator. The
// per-window adapter and the window watcher emit NormalizedEvents into a sink;
// the coordinator implements it. Tests feed events directly into the
// coordinator through this interface, with no Chromium browser constructed.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_EVENT_SINK_H_
#define SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_EVENT_SINK_H_

#include "seoul/browser/lifecycle/lifecycle_events.h"

namespace seoul {

class LifecycleEventSink {
 public:
  virtual ~LifecycleEventSink() = default;
  virtual void OnNormalizedEvent(const NormalizedEvent& event) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_EVENT_SINK_H_
