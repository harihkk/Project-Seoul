// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_events.h"

namespace seoul {

// Move-only; base::Value carries no copy constructor, so ComponentEvent has
// none either.
ComponentEvent::ComponentEvent() = default;
ComponentEvent::ComponentEvent(ComponentEvent&&) = default;
ComponentEvent& ComponentEvent::operator=(ComponentEvent&&) = default;
ComponentEvent::~ComponentEvent() = default;

}  // namespace seoul
