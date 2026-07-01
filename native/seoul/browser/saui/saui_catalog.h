// Project Seoul Adaptive UI (SAUI).
// The trusted component catalog: per-type metadata that drives parsing and
// validation. A document may only use types listed here; the catalog is owned
// by Seoul and never extended by a model at runtime.

#ifndef SEOUL_BROWSER_SAUI_SAUI_CATALOG_H_
#define SEOUL_BROWSER_SAUI_SAUI_CATALOG_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "seoul/browser/saui/saui_types.h"

namespace seoul {

// Bitmask of DataEntryKind values a component's primary "data" binding may
// reference. Zero means the component takes no binding.
inline constexpr uint8_t kBindNone = 0;
inline constexpr uint8_t kBindScalar = 1 << 0;
inline constexpr uint8_t kBindRecord = 1 << 1;
inline constexpr uint8_t kBindSeries = 1 << 2;
inline constexpr uint8_t kBindTable = 1 << 3;

uint8_t DataEntryKindBit(DataEntryKind kind);

struct ComponentTypeInfo {
  ComponentType type;
  const char* name;  // stable wire name, snake_case
  ComponentCategory category;
  bool container;                  // may have children
  bool input;                      // user-editable / activatable control
  bool chart;                      // chart validation rules apply
  bool requires_accessible_name;   // non-empty accessible_name enforced
  uint8_t accepted_binding_kinds;  // kBind* mask for the "data" slot
  bool binding_required;           // "data" slot must be bound
  const char* const* required_props;
  size_t required_prop_count;
};

// Returns the catalog row for `type`. Never null: every enum value has a row
// (enforced by a static_assert on the table and by unit tests).
const ComponentTypeInfo& GetComponentTypeInfo(ComponentType type);

// Returns the row whose wire name is `name`, or null for unknown names.
const ComponentTypeInfo* FindComponentTypeByName(std::string_view name);

const char* ComponentTypeName(ComponentType type);

size_t ComponentTypeCount();

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_CATALOG_H_
