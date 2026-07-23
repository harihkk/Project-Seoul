// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/lifecycle_events.h"

namespace seoul {

NormalizedEvent::NormalizedEvent() = default;
NormalizedEvent::NormalizedEvent(NormalizedEventType type) : type(type) {}
NormalizedEvent::NormalizedEvent(const NormalizedEvent&) = default;
NormalizedEvent::NormalizedEvent(NormalizedEvent&&) = default;
NormalizedEvent& NormalizedEvent::operator=(const NormalizedEvent&) = default;
NormalizedEvent& NormalizedEvent::operator=(NormalizedEvent&&) = default;
NormalizedEvent::~NormalizedEvent() = default;

}  // namespace seoul
