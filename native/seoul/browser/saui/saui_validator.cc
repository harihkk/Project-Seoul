// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_validator.h"

#include <set>
#include <string>

#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_limits.h"

namespace seoul {

namespace {

bool NonEmptyStringProp(const base::Value::Dict& props, std::string_view key) {
  const std::string* value = props.FindString(key);
  return value && !value->empty();
}

SauiStatusResult ValidateChartComponent(const ComponentNode& node,
                                        const DataEntry& entry) {
  // Chart-eligible data: enough real points, attributed and timed.
  if (!EntryChartEligible(entry)) {
    return SauiErr(SauiError::kChartRequirementMissing);
  }
  if (node.type == ComponentType::kPieChart &&
      entry.kind == DataEntryKind::kTable &&
      entry.table.rows.size() > kMaxPieSlices) {
    return SauiErr(SauiError::kChartRequirementMissing);
  }
  // Axis honesty. A bar chart must either start at zero or say that it does
  // not. Other charts declaring an explicit nonzero y_min must carry the
  // truncation indicator.
  if (node.type == ComponentType::kBarChart ||
      node.type == ComponentType::kStackedBarChart) {
    std::optional<bool> baseline_zero = node.props.FindBool("baseline_zero");
    if (!baseline_zero.has_value()) {
      return SauiErr(SauiError::kMissingRequiredProperty);
    }
    if (!*baseline_zero &&
        !node.props.FindBool("axis_truncation_indicated").value_or(false)) {
      return SauiErr(SauiError::kTruncatedAxisNotIndicated);
    }
  } else {
    std::optional<double> y_min = node.props.FindDouble("y_min");
    if (y_min.has_value() && *y_min != 0.0 &&
        !node.props.FindBool("axis_truncation_indicated").value_or(false)) {
      return SauiErr(SauiError::kTruncatedAxisNotIndicated);
    }
  }
  return SauiOk();
}

SauiStatusResult ValidateComponent(const ComponentNode& node,
                                   const AdaptiveSurface& surface,
                                   const std::set<std::string>& action_ids,
                                   std::set<std::string>* seen_component_ids) {
  if (!seen_component_ids->insert(node.id).second) {
    return SauiErr(SauiError::kDuplicateComponentId);
  }
  const ComponentTypeInfo& info = GetComponentTypeInfo(node.type);

  for (size_t i = 0; i < info.required_prop_count; ++i) {
    if (!NonEmptyStringProp(node.props, info.required_props[i])) {
      // Required props may also be non-string (booleans such as
      // baseline_zero); accept any present non-none value.
      const base::Value* value = node.props.Find(info.required_props[i]);
      if (!value || value->is_none()) {
        return SauiErr(SauiError::kMissingRequiredProperty);
      }
    }
  }

  if (info.requires_accessible_name && node.accessible_name.empty()) {
    return SauiErr(SauiError::kMissingAccessibleName);
  }

  // Bindings: the primary "data" slot follows the catalog contract; every
  // bound slot must resolve to an existing entry.
  const auto data_binding = node.bindings.find("data");
  if (info.binding_required && data_binding == node.bindings.end()) {
    return SauiErr(SauiError::kMissingRequiredBinding);
  }
  for (const auto& [slot, entry_name] : node.bindings) {
    const auto entry_it = surface.data.find(entry_name);
    if (entry_it == surface.data.end()) {
      return SauiErr(SauiError::kUnknownDataEntry);
    }
    if (slot == "data") {
      if (info.accepted_binding_kinds == kBindNone) {
        return SauiErr(SauiError::kBindingKindMismatch);
      }
      if (!(info.accepted_binding_kinds &
            DataEntryKindBit(entry_it->second.kind))) {
        return SauiErr(SauiError::kBindingKindMismatch);
      }
    }
  }

  if (info.chart) {
    // The parser guarantees data_binding exists here only if provided;
    // binding_required is true for every chart type in the catalog.
    if (data_binding == node.bindings.end()) {
      return SauiErr(SauiError::kMissingRequiredBinding);
    }
    const DataEntry& entry = surface.data.find(data_binding->second)->second;
    if (auto result = ValidateChartComponent(node, entry);
        !result.has_value()) {
      return result;
    }
  }

  for (const std::string& action_id : node.action_ids) {
    if (action_ids.find(action_id) == action_ids.end()) {
      return SauiErr(SauiError::kUnknownActionReference);
    }
  }

  for (const ComponentNode& child : node.children) {
    if (auto result =
            ValidateComponent(child, surface, action_ids, seen_component_ids);
        !result.has_value()) {
      return result;
    }
  }
  return SauiOk();
}

}  // namespace

bool EntryChartEligible(const DataEntry& entry) {
  if (!entry.has_provenance || entry.provenance.source_name.empty() ||
      entry.provenance.retrieved_at.is_null() ||
      entry.provenance.effective_at.is_null()) {
    return false;
  }
  switch (entry.kind) {
    case DataEntryKind::kSeries:
      return entry.series.points.size() >= 2;
    case DataEntryKind::kTable:
      return entry.table.rows.size() >= 2 ||
             (entry.table.rows.size() == 1 && entry.table.columns.size() >= 2);
    case DataEntryKind::kScalar:
    case DataEntryKind::kRecord:
      return false;  // a single value is never a chart
  }
  return false;
}

SauiStatusResult ValidateSurface(const AdaptiveSurface& surface) {
  if (surface.schema_version != kSauiSchemaVersion) {
    return SauiErr(SauiError::kUnsupportedSchemaVersion);
  }
  if (surface.components.empty()) {
    return SauiErr(SauiError::kEmptySurface);
  }
  if (surface.kind != SurfaceKind::kResponse && surface.title.empty()) {
    return SauiErr(SauiError::kInvalidTitle);
  }

  std::set<std::string> action_ids;
  for (const SurfaceAction& action : surface.actions) {
    if (!action_ids.insert(action.id).second) {
      return SauiErr(SauiError::kDuplicateActionId);
    }
  }

  std::set<std::string> component_ids;
  for (const ComponentNode& node : surface.components) {
    if (auto result =
            ValidateComponent(node, surface, action_ids, &component_ids);
        !result.has_value()) {
      return result;
    }
  }
  return SauiOk();
}

}  // namespace seoul
