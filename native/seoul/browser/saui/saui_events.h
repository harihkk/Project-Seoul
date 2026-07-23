// Project Seoul Adaptive UI (SAUI).
// Typed events emitted by the Canvas when the user interacts with a rendered
// component. Events carry stable component/action ids and a typed value; the
// Canvas never emits raw DOM state or click coordinates, and a model never
// receives them.

#ifndef SEOUL_BROWSER_SAUI_SAUI_EVENTS_H_
#define SEOUL_BROWSER_SAUI_SAUI_EVENTS_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "seoul/browser/saui/saui_types.h"

namespace seoul {

enum class ComponentEventKind {
  kActivate,      // button/link/control activated
  kValueChanged,  // input value changed (value carries the new value)
  kSubmit,        // form submitted (value carries collected field values)
  kSelect,        // row/point/item selected (value carries the selection key)
  kDismiss,       // drawer/section dismissed
};

// Move-only: the event value is a base::Value, which has no copy constructor.
struct ComponentEvent {
  ComponentEvent();
  ComponentEvent(ComponentEvent&&);
  ComponentEvent& operator=(ComponentEvent&&);
  ~ComponentEvent();

  SurfaceId surface_id;
  std::string component_id;
  ComponentEventKind kind = ComponentEventKind::kActivate;
  // The surface action triggered by this event, when the component declared
  // one. Resolution to a tool call / approval / navigation happens in the
  // Canvas session layer against the surface's validated action list.
  std::optional<std::string> action_id;
  base::Value value;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_EVENTS_H_
