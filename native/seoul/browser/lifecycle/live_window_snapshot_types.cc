// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/live_window_snapshot_types.h"

namespace seoul {

LiveTabDescriptor::LiveTabDescriptor() = default;
LiveTabDescriptor::LiveTabDescriptor(const LiveTabDescriptor&) = default;
LiveTabDescriptor::LiveTabDescriptor(LiveTabDescriptor&&) = default;
LiveTabDescriptor& LiveTabDescriptor::operator=(const LiveTabDescriptor&) = default;
LiveTabDescriptor& LiveTabDescriptor::operator=(LiveTabDescriptor&&) = default;
LiveTabDescriptor::~LiveTabDescriptor() = default;

LiveWindowSnapshot::LiveWindowSnapshot() = default;
LiveWindowSnapshot::LiveWindowSnapshot(const LiveWindowSnapshot&) = default;
LiveWindowSnapshot::LiveWindowSnapshot(LiveWindowSnapshot&&) = default;
LiveWindowSnapshot& LiveWindowSnapshot::operator=(const LiveWindowSnapshot&) = default;
LiveWindowSnapshot& LiveWindowSnapshot::operator=(LiveWindowSnapshot&&) = default;
LiveWindowSnapshot::~LiveWindowSnapshot() = default;

}  // namespace seoul
