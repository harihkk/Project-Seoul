// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_document.h"

#include <cmath>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_limits.h"
#include "url/gurl.h"

namespace seoul {

// Clone-based copy semantics for the value-holding SAUI structs
// (base::Value/Dict/List are move-only; a copy deep-clones them). Default
// construction, move, and destruction are trivial member-wise.
DataTable::DataTable() = default;
DataTable::DataTable(DataTable&&) = default;
DataTable& DataTable::operator=(DataTable&&) = default;
DataTable::~DataTable() = default;
DataTable::DataTable(const DataTable& other) : columns(other.columns) {
  rows.reserve(other.rows.size());
  for (const base::Value::List& row : other.rows) {
    rows.push_back(row.Clone());
  }
}
DataTable& DataTable::operator=(const DataTable& other) {
  columns = other.columns;
  rows.clear();
  rows.reserve(other.rows.size());
  for (const base::Value::List& row : other.rows) {
    rows.push_back(row.Clone());
  }
  return *this;
}

DataEntry::DataEntry() = default;
DataEntry::DataEntry(DataEntry&&) = default;
DataEntry& DataEntry::operator=(DataEntry&&) = default;
DataEntry::~DataEntry() = default;
DataEntry::DataEntry(const DataEntry& other)
    : kind(other.kind),
      scalar(other.scalar.Clone()),
      record(other.record.Clone()),
      series(other.series),
      table(other.table),
      has_provenance(other.has_provenance),
      provenance(other.provenance) {}
DataEntry& DataEntry::operator=(const DataEntry& other) {
  kind = other.kind;
  scalar = other.scalar.Clone();
  record = other.record.Clone();
  series = other.series;
  table = other.table;
  has_provenance = other.has_provenance;
  provenance = other.provenance;
  return *this;
}

SurfaceAction::SurfaceAction() = default;
SurfaceAction::SurfaceAction(SurfaceAction&&) = default;
SurfaceAction& SurfaceAction::operator=(SurfaceAction&&) = default;
SurfaceAction::~SurfaceAction() = default;
SurfaceAction::SurfaceAction(const SurfaceAction& other)
    : id(other.id),
      label(other.label),
      kind(other.kind),
      target(other.target),
      payload(other.payload.Clone()),
      requires_confirmation(other.requires_confirmation) {}
SurfaceAction& SurfaceAction::operator=(const SurfaceAction& other) {
  id = other.id;
  label = other.label;
  kind = other.kind;
  target = other.target;
  payload = other.payload.Clone();
  requires_confirmation = other.requires_confirmation;
  return *this;
}

ComponentNode::ComponentNode() = default;
ComponentNode::ComponentNode(ComponentNode&&) = default;
ComponentNode& ComponentNode::operator=(ComponentNode&&) = default;
ComponentNode::~ComponentNode() = default;
ComponentNode::ComponentNode(const ComponentNode& other)
    : id(other.id),
      type(other.type),
      props(other.props.Clone()),
      bindings(other.bindings),
      accessible_name(other.accessible_name),
      state(other.state),
      state_message(other.state_message),
      update_policy(other.update_policy),
      action_ids(other.action_ids),
      children(other.children) {}
ComponentNode& ComponentNode::operator=(const ComponentNode& other) {
  id = other.id;
  type = other.type;
  props = other.props.Clone();
  bindings = other.bindings;
  accessible_name = other.accessible_name;
  state = other.state;
  state_message = other.state_message;
  update_policy = other.update_policy;
  action_ids = other.action_ids;
  children = other.children;
  return *this;
}

namespace {

// Property keys whose string values must be http(s) URLs.
constexpr std::string_view kUrlPropKeys[] = {"href", "src", "source_url"};

// Property keys allowed to carry a bounded structured list value.
constexpr std::string_view kStructuredListKeys[] = {
    "options", "columns", "chips", "sources", "segments"};

// Property keys allowed the larger code-length bound.
constexpr std::string_view kCodePropKeys[] = {"code", "diff"};

bool Contains(base::span<const std::string_view> keys, std::string_view key) {
  for (std::string_view candidate : keys) {
    if (candidate == key) {
      return true;
    }
  }
  return false;
}

bool IsHttpUrl(std::string_view text) {
  if (text.size() > kMaxUrlPropLength) {
    return false;
  }
  GURL url{text};
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

bool IsFiniteNumber(const base::Value& value) {
  if (value.is_int()) {
    return true;
  }
  return value.is_double() && std::isfinite(value.GetDouble());
}

bool IsPrimitive(const base::Value& value) {
  return value.is_string() || value.is_bool() || IsFiniteNumber(value);
}

base::Time TimeFromMillis(double ms) {
  return base::Time::UnixEpoch() + base::Milliseconds(ms);
}

double MillisFromTime(base::Time t) {
  return (t - base::Time::UnixEpoch()).InMillisecondsF();
}

SauiStatusResult ParseStructuredListProp(std::string_view key,
                                         const base::Value::List& list) {
  if (list.size() > kMaxStructuredListItems) {
    return SauiErr(SauiError::kLimitExceeded);
  }
  for (const base::Value& item : list) {
    if (item.is_string()) {
      if (item.GetString().size() > kMaxLabelLength) {
        return SauiErr(SauiError::kInvalidPropertyValue);
      }
      continue;
    }
    const base::Value::Dict* dict = item.GetIfDict();
    if (!dict) {
      return SauiErr(SauiError::kInvalidPropertyValue);
    }
    for (const auto [item_key, item_value] : *dict) {
      if (!IsValidPropKey(item_key) || !item_value.is_string() ||
          item_value.GetString().size() > kMaxPropStringLength) {
        return SauiErr(SauiError::kInvalidPropertyValue);
      }
    }
    if (key == "sources") {
      const std::string* href = dict->FindString("href");
      if (!href || !IsHttpUrl(*href)) {
        return SauiErr(SauiError::kInvalidUrlProperty);
      }
    }
    if (key == "columns") {
      const std::string* column_key = dict->FindString("key");
      if (!column_key || !IsValidPropKey(*column_key)) {
        return SauiErr(SauiError::kInvalidPropertyValue);
      }
    }
  }
  return SauiOk();
}

SauiStatusResult ValidatePropsDict(const base::Value::Dict& props) {
  if (props.size() > kMaxPropsPerComponent) {
    return SauiErr(SauiError::kLimitExceeded);
  }
  for (const auto [key, value] : props) {
    if (key.size() > kMaxPropKeyLength) {
      return SauiErr(SauiError::kInvalidPropertyKey);
    }
    if (!IsValidPropKey(key)) {
      // Distinguish handler/markup keys from merely malformed ones so the
      // caller can log the rejection cause precisely.
      const bool handler_shaped =
          key.size() >= 2 && key[0] == 'o' && key[1] == 'n';
      const bool markup_key = key == "html" || key == "script" ||
                              key == "srcdoc" || key == "innerhtml" ||
                              key == "style";
      return SauiErr(handler_shaped || markup_key
                         ? SauiError::kForbiddenPropertyKey
                         : SauiError::kInvalidPropertyKey);
    }
    if (value.is_list()) {
      if (!Contains(kStructuredListKeys, key)) {
        return SauiErr(SauiError::kInvalidPropertyValue);
      }
      if (auto result = ParseStructuredListProp(key, value.GetList());
          !result.has_value()) {
        return result;
      }
      continue;
    }
    if (!IsPrimitive(value)) {
      return SauiErr(SauiError::kInvalidPropertyValue);
    }
    if (value.is_string()) {
      const std::string& text = value.GetString();
      const size_t bound = Contains(kCodePropKeys, key) ? kMaxCodePropLength
                                                        : kMaxPropStringLength;
      if (text.size() > bound) {
        return SauiErr(SauiError::kInvalidPropertyValue);
      }
      if (Contains(kUrlPropKeys, key) && !IsHttpUrl(text)) {
        return SauiErr(SauiError::kInvalidUrlProperty);
      }
    }
  }
  return SauiOk();
}

SauiResult<DataProvenance> ParseProvenance(const base::Value::Dict& dict) {
  DataProvenance provenance;
  const std::string* source_name = dict.FindString("source_name");
  if (!source_name || source_name->empty() ||
      source_name->size() > kMaxLabelLength) {
    return base::unexpected(SauiError::kMissingProvenance);
  }
  provenance.source_name = *source_name;
  if (const std::string* source_url = dict.FindString("source_url")) {
    if (!IsHttpUrl(*source_url)) {
      return base::unexpected(SauiError::kInvalidUrlProperty);
    }
    provenance.source_url = *source_url;
  }
  std::optional<double> retrieved = dict.FindDouble("retrieved_at_ms");
  std::optional<double> effective = dict.FindDouble("effective_at_ms");
  if (!retrieved || !effective || !std::isfinite(*retrieved) ||
      !std::isfinite(*effective)) {
    return base::unexpected(SauiError::kMissingProvenance);
  }
  provenance.retrieved_at = TimeFromMillis(*retrieved);
  provenance.effective_at = TimeFromMillis(*effective);
  if (const std::string* timezone = dict.FindString("timezone")) {
    if (timezone->size() > kMaxLabelLength) {
      return base::unexpected(SauiError::kInvalidDataEntry);
    }
    provenance.timezone = *timezone;
  }
  if (const std::string* units = dict.FindString("units")) {
    if (units->size() > kMaxLabelLength) {
      return base::unexpected(SauiError::kInvalidDataEntry);
    }
    provenance.units = *units;
  }
  if (const std::string* freshness = dict.FindString("freshness")) {
    if (*freshness == "real_time") {
      provenance.freshness = FreshnessState::kRealTime;
    } else if (*freshness == "delayed") {
      provenance.freshness = FreshnessState::kDelayed;
    } else if (*freshness == "cached") {
      provenance.freshness = FreshnessState::kCached;
    } else if (*freshness == "stale") {
      provenance.freshness = FreshnessState::kStale;
    } else {
      return base::unexpected(SauiError::kInvalidDataEntry);
    }
  }
  if (std::optional<double> completeness = dict.FindDouble("completeness")) {
    if (!std::isfinite(*completeness) || *completeness < 0.0 ||
        *completeness > 1.0) {
      return base::unexpected(SauiError::kInvalidDataEntry);
    }
    provenance.completeness = *completeness;
  }
  return provenance;
}

SauiResult<DataEntry> ParseDataEntry(const base::Value::Dict& dict) {
  DataEntry entry;
  const std::string* kind = dict.FindString("kind");
  if (!kind || !DataEntryKindFromString(*kind, &entry.kind)) {
    return base::unexpected(SauiError::kInvalidDataEntry);
  }
  switch (entry.kind) {
    case DataEntryKind::kScalar: {
      const base::Value* value = dict.Find("value");
      if (!value || !IsPrimitive(*value)) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      if (value->is_string() &&
          value->GetString().size() > kMaxPropStringLength) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      entry.scalar = value->Clone();
      break;
    }
    case DataEntryKind::kRecord: {
      const base::Value::Dict* fields = dict.FindDict("fields");
      if (!fields || fields->size() > kMaxRecordFields) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      for (const auto [key, value] : *fields) {
        if (!IsValidPropKey(key) || !IsPrimitive(value)) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        if (value.is_string() &&
            value.GetString().size() > kMaxPropStringLength) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
      }
      entry.record = fields->Clone();
      break;
    }
    case DataEntryKind::kSeries: {
      const base::Value::List* points = dict.FindList("points");
      if (!points || points->size() > kMaxSeriesPoints) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      if (const std::string* x_unit = dict.FindString("x_unit")) {
        if (x_unit->size() > kMaxLabelLength) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        entry.series.x_unit = *x_unit;
      }
      if (const std::string* y_unit = dict.FindString("y_unit")) {
        if (y_unit->size() > kMaxLabelLength) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        entry.series.y_unit = *y_unit;
      }
      for (const base::Value& point_value : *points) {
        const base::Value::Dict* point = point_value.GetIfDict();
        if (!point) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        SeriesPoint parsed;
        std::optional<double> y = point->FindDouble("y");
        if (!y || !std::isfinite(*y)) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        parsed.y = *y;
        std::optional<double> t_ms = point->FindDouble("t_ms");
        std::optional<double> x = point->FindDouble("x");
        if (t_ms.has_value() == x.has_value()) {
          // Exactly one of t_ms/x must be present.
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        if (t_ms) {
          if (!std::isfinite(*t_ms)) {
            return base::unexpected(SauiError::kInvalidDataEntry);
          }
          parsed.has_time = true;
          parsed.time = TimeFromMillis(*t_ms);
        } else {
          if (!std::isfinite(*x)) {
            return base::unexpected(SauiError::kInvalidDataEntry);
          }
          parsed.x = *x;
        }
        entry.series.points.push_back(parsed);
      }
      break;
    }
    case DataEntryKind::kTable: {
      const base::Value::List* columns = dict.FindList("columns");
      const base::Value::List* rows = dict.FindList("rows");
      if (!columns || columns->empty() || columns->size() > kMaxTableColumns ||
          !rows || rows->size() > kMaxTableRows) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      for (const base::Value& column_value : *columns) {
        const base::Value::Dict* column = column_value.GetIfDict();
        if (!column) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        const std::string* key = column->FindString("key");
        const std::string* label = column->FindString("label");
        if (!key || !IsValidPropKey(*key) || !label ||
            label->size() > kMaxLabelLength) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        entry.table.columns.push_back({*key, *label});
      }
      for (const base::Value& row_value : *rows) {
        const base::Value::List* row = row_value.GetIfList();
        if (!row || row->size() != entry.table.columns.size()) {
          return base::unexpected(SauiError::kInvalidDataEntry);
        }
        for (const base::Value& cell : *row) {
          if (!IsPrimitive(cell)) {
            return base::unexpected(SauiError::kInvalidDataEntry);
          }
          if (cell.is_string() &&
              cell.GetString().size() > kMaxPropStringLength) {
            return base::unexpected(SauiError::kInvalidDataEntry);
          }
        }
        entry.table.rows.push_back(row->Clone());
      }
      break;
    }
  }
  if (const base::Value::Dict* provenance = dict.FindDict("provenance")) {
    auto parsed = ParseProvenance(*provenance);
    if (!parsed.has_value()) {
      return base::unexpected(parsed.error());
    }
    entry.has_provenance = true;
    entry.provenance = std::move(parsed.value());
  }
  return entry;
}

SauiResult<SurfaceAction> ParseAction(const base::Value::Dict& dict) {
  SurfaceAction action;
  const std::string* id = dict.FindString("id");
  if (!id || !IsValidSauiIdentifier(*id)) {
    return base::unexpected(SauiError::kInvalidAction);
  }
  action.id = *id;
  const std::string* label = dict.FindString("label");
  if (!label || label->empty() || label->size() > kMaxLabelLength) {
    return base::unexpected(SauiError::kInvalidAction);
  }
  action.label = *label;
  const std::string* kind = dict.FindString("kind");
  if (!kind || !SurfaceActionKindFromString(*kind, &action.kind)) {
    return base::unexpected(SauiError::kInvalidAction);
  }
  const std::string* target = dict.FindString("target");
  if (!target || target->empty() || target->size() > kMaxUrlPropLength) {
    return base::unexpected(SauiError::kInvalidAction);
  }
  if (action.kind == SurfaceActionKind::kNavigate && !IsHttpUrl(*target)) {
    return base::unexpected(SauiError::kInvalidUrlProperty);
  }
  action.target = *target;
  if (const base::Value::Dict* payload = dict.FindDict("payload")) {
    if (payload->size() > kMaxPropsPerComponent) {
      return base::unexpected(SauiError::kLimitExceeded);
    }
    for (const auto [key, value] : *payload) {
      if (!IsValidPropKey(key) || !IsPrimitive(value)) {
        return base::unexpected(SauiError::kInvalidAction);
      }
      if (value.is_string() &&
          value.GetString().size() > kMaxPropStringLength) {
        return base::unexpected(SauiError::kInvalidAction);
      }
    }
    action.payload = payload->Clone();
  }
  action.requires_confirmation =
      dict.FindBool("requires_confirmation").value_or(false);
  return action;
}

SauiResult<ComponentNode> ParseComponent(const base::Value::Dict& dict,
                                         size_t depth,
                                         size_t* total_components) {
  if (depth > kMaxComponentDepth) {
    return base::unexpected(SauiError::kDepthExceeded);
  }
  if (++(*total_components) > kMaxSurfaceComponents) {
    return base::unexpected(SauiError::kLimitExceeded);
  }
  ComponentNode node;
  const std::string* id = dict.FindString("id");
  if (!id || !IsValidSauiIdentifier(*id)) {
    return base::unexpected(SauiError::kInvalidComponentId);
  }
  node.id = *id;
  const std::string* type_name = dict.FindString("type");
  const ComponentTypeInfo* info =
      type_name ? FindComponentTypeByName(*type_name) : nullptr;
  if (!info) {
    return base::unexpected(SauiError::kUnknownComponentType);
  }
  node.type = info->type;
  if (const base::Value::Dict* props = dict.FindDict("props")) {
    if (auto result = ValidatePropsDict(*props); !result.has_value()) {
      return base::unexpected(result.error());
    }
    node.props = props->Clone();
  }
  if (const base::Value::Dict* bindings = dict.FindDict("bindings")) {
    if (bindings->size() > kMaxBindingsPerComponent) {
      return base::unexpected(SauiError::kLimitExceeded);
    }
    for (const auto [slot, entry_name] : *bindings) {
      if (!IsValidPropKey(slot) || !entry_name.is_string() ||
          !IsValidSauiIdentifier(entry_name.GetString())) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      node.bindings[slot] = entry_name.GetString();
    }
  }
  if (const std::string* accessible_name = dict.FindString("accessible_name")) {
    if (accessible_name->size() > kMaxAccessibleNameLength) {
      return base::unexpected(SauiError::kInvalidPropertyValue);
    }
    node.accessible_name = *accessible_name;
  }
  if (const std::string* state = dict.FindString("state")) {
    if (!ComponentStateFromString(*state, &node.state)) {
      return base::unexpected(SauiError::kInvalidState);
    }
  }
  if (const std::string* message = dict.FindString("state_message")) {
    if (message->size() > kMaxLabelLength * 2) {
      return base::unexpected(SauiError::kInvalidState);
    }
    node.state_message = *message;
  }
  if (const std::string* policy = dict.FindString("update_policy")) {
    if (*policy == "static") {
      node.update_policy = UpdatePolicy::kStatic;
    } else if (*policy == "live") {
      node.update_policy = UpdatePolicy::kLive;
    } else {
      return base::unexpected(SauiError::kInvalidDocument);
    }
  }
  if (const base::Value::List* actions = dict.FindList("actions")) {
    if (actions->size() > kMaxActionsPerComponent) {
      return base::unexpected(SauiError::kLimitExceeded);
    }
    for (const base::Value& action_id : *actions) {
      if (!action_id.is_string() ||
          !IsValidSauiIdentifier(action_id.GetString())) {
        return base::unexpected(SauiError::kUnknownActionReference);
      }
      node.action_ids.push_back(action_id.GetString());
    }
  }
  if (const base::Value::List* children = dict.FindList("children")) {
    if (!children->empty() && !info->container) {
      return base::unexpected(SauiError::kChildrenNotAllowed);
    }
    if (children->size() > kMaxChildrenPerComponent) {
      return base::unexpected(SauiError::kLimitExceeded);
    }
    for (const base::Value& child_value : *children) {
      const base::Value::Dict* child = child_value.GetIfDict();
      if (!child) {
        return base::unexpected(SauiError::kInvalidDocument);
      }
      auto parsed = ParseComponent(*child, depth + 1, total_components);
      if (!parsed.has_value()) {
        return base::unexpected(parsed.error());
      }
      node.children.push_back(std::move(parsed.value()));
    }
  }
  return node;
}

base::Value::Dict ProvenanceToValue(const DataProvenance& provenance) {
  base::Value::Dict dict;
  dict.Set("source_name", provenance.source_name);
  if (!provenance.source_url.empty()) {
    dict.Set("source_url", provenance.source_url);
  }
  dict.Set("retrieved_at_ms", MillisFromTime(provenance.retrieved_at));
  dict.Set("effective_at_ms", MillisFromTime(provenance.effective_at));
  if (!provenance.timezone.empty()) {
    dict.Set("timezone", provenance.timezone);
  }
  if (!provenance.units.empty()) {
    dict.Set("units", provenance.units);
  }
  dict.Set("freshness", FreshnessStateToString(provenance.freshness));
  dict.Set("completeness", provenance.completeness);
  return dict;
}

base::Value::Dict DataEntryToValue(const DataEntry& entry) {
  base::Value::Dict dict;
  dict.Set("kind", DataEntryKindToString(entry.kind));
  switch (entry.kind) {
    case DataEntryKind::kScalar:
      dict.Set("value", entry.scalar.Clone());
      break;
    case DataEntryKind::kRecord:
      dict.Set("fields", entry.record.Clone());
      break;
    case DataEntryKind::kSeries: {
      if (!entry.series.x_unit.empty()) {
        dict.Set("x_unit", entry.series.x_unit);
      }
      if (!entry.series.y_unit.empty()) {
        dict.Set("y_unit", entry.series.y_unit);
      }
      base::Value::List points;
      for (const SeriesPoint& point : entry.series.points) {
        base::Value::Dict point_dict;
        if (point.has_time) {
          point_dict.Set("t_ms", MillisFromTime(point.time));
        } else {
          point_dict.Set("x", point.x);
        }
        point_dict.Set("y", point.y);
        points.Append(std::move(point_dict));
      }
      dict.Set("points", std::move(points));
      break;
    }
    case DataEntryKind::kTable: {
      base::Value::List columns;
      for (const TableColumn& column : entry.table.columns) {
        base::Value::Dict column_dict;
        column_dict.Set("key", column.key);
        column_dict.Set("label", column.label);
        columns.Append(std::move(column_dict));
      }
      dict.Set("columns", std::move(columns));
      base::Value::List rows;
      for (const base::Value::List& row : entry.table.rows) {
        rows.Append(row.Clone());
      }
      dict.Set("rows", std::move(rows));
      break;
    }
  }
  if (entry.has_provenance) {
    dict.Set("provenance", ProvenanceToValue(entry.provenance));
  }
  return dict;
}

base::Value::Dict ComponentToValue(const ComponentNode& node) {
  base::Value::Dict dict;
  dict.Set("id", node.id);
  dict.Set("type", ComponentTypeName(node.type));
  if (!node.props.empty()) {
    dict.Set("props", node.props.Clone());
  }
  if (!node.bindings.empty()) {
    base::Value::Dict bindings;
    for (const auto& [slot, entry_name] : node.bindings) {
      bindings.Set(slot, entry_name);
    }
    dict.Set("bindings", std::move(bindings));
  }
  if (!node.accessible_name.empty()) {
    dict.Set("accessible_name", node.accessible_name);
  }
  dict.Set("state", ComponentStateToString(node.state));
  if (!node.state_message.empty()) {
    dict.Set("state_message", node.state_message);
  }
  dict.Set("update_policy",
           node.update_policy == UpdatePolicy::kLive ? "live" : "static");
  if (!node.action_ids.empty()) {
    base::Value::List actions;
    for (const std::string& action_id : node.action_ids) {
      actions.Append(action_id);
    }
    dict.Set("actions", std::move(actions));
  }
  if (!node.children.empty()) {
    base::Value::List children;
    for (const ComponentNode& child : node.children) {
      children.Append(ComponentToValue(child));
    }
    dict.Set("children", std::move(children));
  }
  return dict;
}

}  // namespace

SauiStatusResult ValidateSurfacePropsDict(const base::Value::Dict& props) {
  return ValidatePropsDict(props);
}

SauiResult<ComponentNode> ParseComponentValue(const base::Value::Dict& dict) {
  size_t total_components = 0;
  return ParseComponent(dict, /*depth=*/1, &total_components);
}

SauiResult<DataEntry> ParseDataEntryValue(const base::Value::Dict& dict) {
  return ParseDataEntry(dict);
}

SauiResult<SurfaceAction> ParseActionValue(const base::Value::Dict& dict) {
  return ParseAction(dict);
}

bool IsValidSauiIdentifier(std::string_view id) {
  if (id.empty() || id.size() > kMaxComponentIdLength) {
    return false;
  }
  auto is_alpha = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  };
  if (!is_alpha(id[0])) {
    return false;
  }
  for (char c : id) {
    if (!is_alpha(c) && !(c >= '0' && c <= '9') && c != '_' && c != '-') {
      return false;
    }
  }
  return true;
}

bool IsValidPropKey(std::string_view key) {
  if (key.empty() || key.size() > kMaxPropKeyLength) {
    return false;
  }
  if (key[0] < 'a' || key[0] > 'z') {
    return false;
  }
  for (char c : key) {
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
      return false;
    }
  }
  // Event-handler-shaped keys can never become handlers (the renderer executes
  // nothing), but they are rejected outright so a generator cannot even
  // express the intent.
  if (key.size() >= 2 && key[0] == 'o' && key[1] == 'n') {
    return false;
  }
  if (key == "html" || key == "script" || key == "srcdoc" ||
      key == "innerhtml" || key == "style") {
    return false;
  }
  return true;
}

SauiResult<AdaptiveSurface> ParseSurface(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(SauiError::kInvalidDocument);
  }
  AdaptiveSurface surface;
  std::optional<int> schema_version = dict->FindInt("schema_version");
  if (!schema_version || *schema_version != kSauiSchemaVersion) {
    return base::unexpected(SauiError::kUnsupportedSchemaVersion);
  }
  surface.schema_version = *schema_version;
  if (const std::string* id = dict->FindString("id")) {
    surface.id = SurfaceId::FromString(*id);
  }
  if (!surface.id.is_valid()) {
    surface.id = SurfaceId::GenerateNew();
  }
  const std::string* kind = dict->FindString("kind");
  if (!kind || !SurfaceKindFromString(*kind, &surface.kind)) {
    return base::unexpected(SauiError::kUnknownSurfaceKind);
  }
  if (const std::string* title = dict->FindString("title")) {
    if (title->size() > kMaxTitleLength) {
      return base::unexpected(SauiError::kInvalidTitle);
    }
    surface.title = *title;
  }
  const base::Value::List* components = dict->FindList("components");
  if (!components || components->empty()) {
    return base::unexpected(SauiError::kEmptySurface);
  }
  size_t total_components = 0;
  for (const base::Value& component_value : *components) {
    const base::Value::Dict* component = component_value.GetIfDict();
    if (!component) {
      return base::unexpected(SauiError::kInvalidDocument);
    }
    auto parsed = ParseComponent(*component, /*depth=*/1, &total_components);
    if (!parsed.has_value()) {
      return base::unexpected(parsed.error());
    }
    surface.components.push_back(std::move(parsed.value()));
  }
  if (const base::Value::Dict* data = dict->FindDict("data")) {
    if (data->size() > kMaxDataEntries) {
      return base::unexpected(SauiError::kLimitExceeded);
    }
    for (const auto [name, entry_value] : *data) {
      if (!IsValidSauiIdentifier(name)) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      const base::Value::Dict* entry_dict = entry_value.GetIfDict();
      if (!entry_dict) {
        return base::unexpected(SauiError::kInvalidDataEntry);
      }
      auto entry = ParseDataEntry(*entry_dict);
      if (!entry.has_value()) {
        return base::unexpected(entry.error());
      }
      surface.data[name] = std::move(entry.value());
    }
  }
  if (const base::Value::List* actions = dict->FindList("actions")) {
    if (actions->size() > kMaxSurfaceActions) {
      return base::unexpected(SauiError::kLimitExceeded);
    }
    for (const base::Value& action_value : *actions) {
      const base::Value::Dict* action_dict = action_value.GetIfDict();
      if (!action_dict) {
        return base::unexpected(SauiError::kInvalidAction);
      }
      auto action = ParseAction(*action_dict);
      if (!action.has_value()) {
        return base::unexpected(action.error());
      }
      surface.actions.push_back(std::move(action.value()));
    }
  }
  if (const base::Value::Dict* provenance = dict->FindDict("provenance")) {
    if (const std::string* generator = provenance->FindString("generator")) {
      if (generator->size() > kMaxLabelLength) {
        return base::unexpected(SauiError::kInvalidDocument);
      }
      surface.provenance.generator = *generator;
    }
    if (std::optional<double> created =
            provenance->FindDouble("created_at_ms")) {
      if (!std::isfinite(*created)) {
        return base::unexpected(SauiError::kInvalidDocument);
      }
      surface.provenance.created_at = TimeFromMillis(*created);
    }
    if (const base::Value::List* sources =
            provenance->FindList("source_urls")) {
      if (sources->size() > kMaxStructuredListItems) {
        return base::unexpected(SauiError::kLimitExceeded);
      }
      for (const base::Value& source : *sources) {
        if (!source.is_string() || !IsHttpUrl(source.GetString())) {
          return base::unexpected(SauiError::kInvalidUrlProperty);
        }
        surface.provenance.source_urls.push_back(source.GetString());
      }
    }
  }
  surface.pinned = dict->FindBool("pinned").value_or(false);
  return surface;
}

base::Value::Dict SurfaceToValue(const AdaptiveSurface& surface) {
  base::Value::Dict dict;
  dict.Set("schema_version", surface.schema_version);
  dict.Set("id", surface.id.value());
  dict.Set("kind", SurfaceKindToString(surface.kind));
  if (!surface.title.empty()) {
    dict.Set("title", surface.title);
  }
  base::Value::List components;
  for (const ComponentNode& node : surface.components) {
    components.Append(ComponentToValue(node));
  }
  dict.Set("components", std::move(components));
  if (!surface.data.empty()) {
    base::Value::Dict data;
    for (const auto& [name, entry] : surface.data) {
      data.Set(name, DataEntryToValue(entry));
    }
    dict.Set("data", std::move(data));
  }
  if (!surface.actions.empty()) {
    base::Value::List actions;
    for (const SurfaceAction& action : surface.actions) {
      base::Value::Dict action_dict;
      action_dict.Set("id", action.id);
      action_dict.Set("label", action.label);
      action_dict.Set("kind", SurfaceActionKindToString(action.kind));
      action_dict.Set("target", action.target);
      if (!action.payload.empty()) {
        action_dict.Set("payload", action.payload.Clone());
      }
      action_dict.Set("requires_confirmation", action.requires_confirmation);
      actions.Append(std::move(action_dict));
    }
    dict.Set("actions", std::move(actions));
  }
  base::Value::Dict provenance;
  if (!surface.provenance.generator.empty()) {
    provenance.Set("generator", surface.provenance.generator);
  }
  if (!surface.provenance.created_at.is_null()) {
    provenance.Set("created_at_ms",
                   MillisFromTime(surface.provenance.created_at));
  }
  if (!surface.provenance.source_urls.empty()) {
    base::Value::List sources;
    for (const std::string& source : surface.provenance.source_urls) {
      sources.Append(source);
    }
    provenance.Set("source_urls", std::move(sources));
  }
  if (!provenance.empty()) {
    dict.Set("provenance", std::move(provenance));
  }
  if (surface.pinned) {
    dict.Set("pinned", true);
  }
  return dict;
}

const char* SurfaceKindToString(SurfaceKind kind) {
  switch (kind) {
    case SurfaceKind::kResponse:
      return "response";
    case SurfaceKind::kForm:
      return "form";
    case SurfaceKind::kDashboard:
      return "dashboard";
    case SurfaceKind::kReport:
      return "report";
    case SurfaceKind::kApproval:
      return "approval";
    case SurfaceKind::kWorkflowCanvas:
      return "workflow_canvas";
    case SurfaceKind::kTaskStatus:
      return "task_status";
    case SurfaceKind::kMonitor:
      return "monitor";
  }
  return "response";
}

bool SurfaceKindFromString(std::string_view s, SurfaceKind* out) {
  static constexpr std::pair<std::string_view, SurfaceKind> kKinds[] = {
      {"response", SurfaceKind::kResponse},
      {"form", SurfaceKind::kForm},
      {"dashboard", SurfaceKind::kDashboard},
      {"report", SurfaceKind::kReport},
      {"approval", SurfaceKind::kApproval},
      {"workflow_canvas", SurfaceKind::kWorkflowCanvas},
      {"task_status", SurfaceKind::kTaskStatus},
      {"monitor", SurfaceKind::kMonitor},
  };
  for (const auto& [name, kind] : kKinds) {
    if (s == name) {
      *out = kind;
      return true;
    }
  }
  return false;
}

const char* ComponentStateToString(ComponentState state) {
  switch (state) {
    case ComponentState::kReady:
      return "ready";
    case ComponentState::kLoading:
      return "loading";
    case ComponentState::kError:
      return "error";
    case ComponentState::kEmpty:
      return "empty";
  }
  return "ready";
}

bool ComponentStateFromString(std::string_view s, ComponentState* out) {
  if (s == "ready") {
    *out = ComponentState::kReady;
  } else if (s == "loading") {
    *out = ComponentState::kLoading;
  } else if (s == "error") {
    *out = ComponentState::kError;
  } else if (s == "empty") {
    *out = ComponentState::kEmpty;
  } else {
    return false;
  }
  return true;
}

const char* SurfaceActionKindToString(SurfaceActionKind kind) {
  switch (kind) {
    case SurfaceActionKind::kToolCall:
      return "tool_call";
    case SurfaceActionKind::kLocalState:
      return "local_state";
    case SurfaceActionKind::kWorkflowEdit:
      return "workflow_edit";
    case SurfaceActionKind::kBrowserAction:
      return "browser_action";
    case SurfaceActionKind::kTaskApproval:
      return "task_approval";
    case SurfaceActionKind::kNavigate:
      return "navigate";
  }
  return "local_state";
}

bool SurfaceActionKindFromString(std::string_view s, SurfaceActionKind* out) {
  static constexpr std::pair<std::string_view, SurfaceActionKind> kKinds[] = {
      {"tool_call", SurfaceActionKind::kToolCall},
      {"local_state", SurfaceActionKind::kLocalState},
      {"workflow_edit", SurfaceActionKind::kWorkflowEdit},
      {"browser_action", SurfaceActionKind::kBrowserAction},
      {"task_approval", SurfaceActionKind::kTaskApproval},
      {"navigate", SurfaceActionKind::kNavigate},
  };
  for (const auto& [name, kind] : kKinds) {
    if (s == name) {
      *out = kind;
      return true;
    }
  }
  return false;
}

const char* DataEntryKindToString(DataEntryKind kind) {
  switch (kind) {
    case DataEntryKind::kScalar:
      return "scalar";
    case DataEntryKind::kRecord:
      return "record";
    case DataEntryKind::kSeries:
      return "series";
    case DataEntryKind::kTable:
      return "table";
  }
  return "scalar";
}

bool DataEntryKindFromString(std::string_view s, DataEntryKind* out) {
  if (s == "scalar") {
    *out = DataEntryKind::kScalar;
  } else if (s == "record") {
    *out = DataEntryKind::kRecord;
  } else if (s == "series") {
    *out = DataEntryKind::kSeries;
  } else if (s == "table") {
    *out = DataEntryKind::kTable;
  } else {
    return false;
  }
  return true;
}

const char* SauiErrorToString(SauiError error) {
  switch (error) {
    case SauiError::kInvalidDocument:
      return "invalid_document";
    case SauiError::kUnsupportedSchemaVersion:
      return "unsupported_schema_version";
    case SauiError::kUnknownSurfaceKind:
      return "unknown_surface_kind";
    case SauiError::kInvalidTitle:
      return "invalid_title";
    case SauiError::kEmptySurface:
      return "empty_surface";
    case SauiError::kLimitExceeded:
      return "limit_exceeded";
    case SauiError::kDepthExceeded:
      return "depth_exceeded";
    case SauiError::kUnknownComponentType:
      return "unknown_component_type";
    case SauiError::kInvalidComponentId:
      return "invalid_component_id";
    case SauiError::kDuplicateComponentId:
      return "duplicate_component_id";
    case SauiError::kChildrenNotAllowed:
      return "children_not_allowed";
    case SauiError::kInvalidPropertyKey:
      return "invalid_property_key";
    case SauiError::kForbiddenPropertyKey:
      return "forbidden_property_key";
    case SauiError::kInvalidPropertyValue:
      return "invalid_property_value";
    case SauiError::kMissingRequiredProperty:
      return "missing_required_property";
    case SauiError::kInvalidUrlProperty:
      return "invalid_url_property";
    case SauiError::kMissingAccessibleName:
      return "missing_accessible_name";
    case SauiError::kUnknownDataEntry:
      return "unknown_data_entry";
    case SauiError::kInvalidDataEntry:
      return "invalid_data_entry";
    case SauiError::kBindingKindMismatch:
      return "binding_kind_mismatch";
    case SauiError::kMissingRequiredBinding:
      return "missing_required_binding";
    case SauiError::kMissingProvenance:
      return "missing_provenance";
    case SauiError::kChartRequirementMissing:
      return "chart_requirement_missing";
    case SauiError::kTruncatedAxisNotIndicated:
      return "truncated_axis_not_indicated";
    case SauiError::kInvalidAction:
      return "invalid_action";
    case SauiError::kDuplicateActionId:
      return "duplicate_action_id";
    case SauiError::kUnknownActionReference:
      return "unknown_action_reference";
    case SauiError::kInvalidState:
      return "invalid_state";
    case SauiError::kInvalidPatch:
      return "invalid_patch";
    case SauiError::kPatchTargetMissing:
      return "patch_target_missing";
    case SauiError::kPatchLimitExceeded:
      return "patch_limit_exceeded";
  }
  return "invalid_document";
}

}  // namespace seoul
