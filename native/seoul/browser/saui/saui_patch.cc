// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_patch.h"

#include <cmath>
#include <utility>

#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_document.h"
#include "seoul/browser/saui/saui_limits.h"
#include "seoul/browser/saui/saui_validator.h"

namespace seoul {

// Clone-based copy semantics for SurfacePatchOp (its props Dict is move-only;
// its DataEntry/ComponentNode/SurfaceAction parts are already clone-copyable).
SurfacePatchOp::SurfacePatchOp() = default;
SurfacePatchOp::SurfacePatchOp(SurfacePatchOp&&) = default;
SurfacePatchOp& SurfacePatchOp::operator=(SurfacePatchOp&&) = default;
SurfacePatchOp::~SurfacePatchOp() = default;
SurfacePatchOp::SurfacePatchOp(const SurfacePatchOp& other)
    : kind(other.kind),
      target_component(other.target_component),
      entry_name(other.entry_name),
      props(other.props.Clone()),
      state(other.state),
      state_message(other.state_message),
      title(other.title),
      entry(other.entry),
      points(other.points),
      component(other.component),
      actions(other.actions) {}
SurfacePatchOp& SurfacePatchOp::operator=(const SurfacePatchOp& other) {
  kind = other.kind;
  target_component = other.target_component;
  entry_name = other.entry_name;
  props = other.props.Clone();
  state = other.state;
  state_message = other.state_message;
  title = other.title;
  entry = other.entry;
  points = other.points;
  component = other.component;
  actions = other.actions;
  return *this;
}

namespace {

ComponentNode* FindComponent(std::vector<ComponentNode>& nodes,
                             std::string_view id) {
  for (ComponentNode& node : nodes) {
    if (node.id == id) {
      return &node;
    }
    if (ComponentNode* found = FindComponent(node.children, id)) {
      return found;
    }
  }
  return nullptr;
}

bool RemoveComponentById(std::vector<ComponentNode>& nodes,
                         std::string_view id) {
  for (auto it = nodes.begin(); it != nodes.end(); ++it) {
    if (it->id == id) {
      nodes.erase(it);
      return true;
    }
    if (RemoveComponentById(it->children, id)) {
      return true;
    }
  }
  return false;
}

size_t CountComponents(const std::vector<ComponentNode>& nodes) {
  size_t count = 0;
  for (const ComponentNode& node : nodes) {
    count += 1 + CountComponents(node.children);
  }
  return count;
}

SauiResult<PatchOpKind> ParseOpKind(const std::string& name) {
  if (name == "set_props") {
    return PatchOpKind::kSetProps;
  }
  if (name == "set_state") {
    return PatchOpKind::kSetState;
  }
  if (name == "set_title") {
    return PatchOpKind::kSetTitle;
  }
  if (name == "upsert_data_entry") {
    return PatchOpKind::kUpsertDataEntry;
  }
  if (name == "append_series_points") {
    return PatchOpKind::kAppendSeriesPoints;
  }
  if (name == "append_child") {
    return PatchOpKind::kAppendChild;
  }
  if (name == "remove_component") {
    return PatchOpKind::kRemoveComponent;
  }
  if (name == "replace_component") {
    return PatchOpKind::kReplaceComponent;
  }
  if (name == "set_actions") {
    return PatchOpKind::kSetActions;
  }
  return base::unexpected(SauiError::kInvalidPatch);
}

SauiStatusResult ApplyOp(AdaptiveSurface& surface,
                         const SurfacePatchOp& op,
                         AppliedPatch* summary) {
  switch (op.kind) {
    case PatchOpKind::kSetProps: {
      ComponentNode* node =
          FindComponent(surface.components, op.target_component);
      if (!node) {
        return SauiErr(SauiError::kPatchTargetMissing);
      }
      if (auto valid = ValidateSurfacePropsDict(op.props); !valid.has_value()) {
        return valid;
      }
      for (const auto [key, value] : op.props) {
        node->props.Set(key, value.Clone());
      }
      if (node->props.size() > kMaxPropsPerComponent) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      summary->changed_component_ids.push_back(node->id);
      return SauiOk();
    }
    case PatchOpKind::kSetState: {
      ComponentNode* node =
          FindComponent(surface.components, op.target_component);
      if (!node) {
        return SauiErr(SauiError::kPatchTargetMissing);
      }
      if (op.state_message.size() > kMaxLabelLength * 2) {
        return SauiErr(SauiError::kInvalidState);
      }
      node->state = op.state;
      node->state_message = op.state_message;
      summary->changed_component_ids.push_back(node->id);
      return SauiOk();
    }
    case PatchOpKind::kSetTitle: {
      if (op.title.size() > kMaxTitleLength) {
        return SauiErr(SauiError::kInvalidTitle);
      }
      surface.title = op.title;
      summary->title_changed = true;
      return SauiOk();
    }
    case PatchOpKind::kUpsertDataEntry: {
      if (!IsValidSauiIdentifier(op.entry_name)) {
        return SauiErr(SauiError::kInvalidDataEntry);
      }
      const bool exists =
          surface.data.find(op.entry_name) != surface.data.end();
      if (!exists && surface.data.size() >= kMaxDataEntries) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      surface.data[op.entry_name] = op.entry;
      summary->changed_entry_names.push_back(op.entry_name);
      return SauiOk();
    }
    case PatchOpKind::kAppendSeriesPoints: {
      auto it = surface.data.find(op.entry_name);
      if (it == surface.data.end()) {
        return SauiErr(SauiError::kPatchTargetMissing);
      }
      if (it->second.kind != DataEntryKind::kSeries) {
        return SauiErr(SauiError::kBindingKindMismatch);
      }
      if (it->second.series.points.size() + op.points.size() >
          kMaxSeriesPoints) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      for (const SeriesPoint& point : op.points) {
        if (!std::isfinite(point.y) ||
            (!point.has_time && !std::isfinite(point.x))) {
          return SauiErr(SauiError::kInvalidDataEntry);
        }
        it->second.series.points.push_back(point);
      }
      summary->changed_entry_names.push_back(op.entry_name);
      return SauiOk();
    }
    case PatchOpKind::kAppendChild: {
      ComponentNode* parent =
          FindComponent(surface.components, op.target_component);
      if (!parent) {
        return SauiErr(SauiError::kPatchTargetMissing);
      }
      if (!GetComponentTypeInfo(parent->type).container) {
        return SauiErr(SauiError::kChildrenNotAllowed);
      }
      if (parent->children.size() >= kMaxChildrenPerComponent) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      if (CountComponents(surface.components) + 1 +
              CountComponents(op.component.children) >
          kMaxSurfaceComponents) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      parent->children.push_back(op.component);
      summary->changed_component_ids.push_back(parent->id);
      summary->changed_component_ids.push_back(op.component.id);
      return SauiOk();
    }
    case PatchOpKind::kRemoveComponent: {
      if (!RemoveComponentById(surface.components, op.target_component)) {
        return SauiErr(SauiError::kPatchTargetMissing);
      }
      summary->changed_component_ids.push_back(op.target_component);
      return SauiOk();
    }
    case PatchOpKind::kReplaceComponent: {
      ComponentNode* node =
          FindComponent(surface.components, op.target_component);
      if (!node) {
        return SauiErr(SauiError::kPatchTargetMissing);
      }
      const size_t others = CountComponents(surface.components) - 1 -
                            CountComponents(node->children);
      if (others + 1 + CountComponents(op.component.children) >
          kMaxSurfaceComponents) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      *node = op.component;
      summary->changed_component_ids.push_back(op.component.id);
      return SauiOk();
    }
    case PatchOpKind::kSetActions: {
      if (op.actions.size() > kMaxSurfaceActions) {
        return SauiErr(SauiError::kLimitExceeded);
      }
      surface.actions = op.actions;
      summary->actions_changed = true;
      return SauiOk();
    }
  }
  return SauiErr(SauiError::kInvalidPatch);
}

}  // namespace

SauiResult<SurfacePatch> ParseSurfacePatch(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(SauiError::kInvalidPatch);
  }
  SurfacePatch patch;
  const std::string* surface_id = dict->FindString("surface_id");
  if (!surface_id) {
    return base::unexpected(SauiError::kInvalidPatch);
  }
  patch.surface_id = SurfaceId::FromString(*surface_id);
  if (!patch.surface_id.is_valid()) {
    return base::unexpected(SauiError::kInvalidPatch);
  }
  const base::Value::List* ops = dict->FindList("ops");
  if (!ops || ops->empty()) {
    return base::unexpected(SauiError::kInvalidPatch);
  }
  if (ops->size() > kMaxPatchOps) {
    return base::unexpected(SauiError::kPatchLimitExceeded);
  }
  for (const base::Value& op_value : *ops) {
    const base::Value::Dict* op_dict = op_value.GetIfDict();
    if (!op_dict) {
      return base::unexpected(SauiError::kInvalidPatch);
    }
    const std::string* op_name = op_dict->FindString("op");
    if (!op_name) {
      return base::unexpected(SauiError::kInvalidPatch);
    }
    auto kind = ParseOpKind(*op_name);
    if (!kind.has_value()) {
      return base::unexpected(kind.error());
    }
    SurfacePatchOp op;
    op.kind = kind.value();
    if (const std::string* target = op_dict->FindString("target")) {
      if (!IsValidSauiIdentifier(*target)) {
        return base::unexpected(SauiError::kInvalidPatch);
      }
      op.target_component = *target;
    }
    if (const std::string* entry_name = op_dict->FindString("entry")) {
      if (!IsValidSauiIdentifier(*entry_name)) {
        return base::unexpected(SauiError::kInvalidPatch);
      }
      op.entry_name = *entry_name;
    }
    switch (op.kind) {
      case PatchOpKind::kSetProps: {
        const base::Value::Dict* props = op_dict->FindDict("props");
        if (!props || op.target_component.empty()) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        if (auto valid = ValidateSurfacePropsDict(*props); !valid.has_value()) {
          return base::unexpected(valid.error());
        }
        op.props = props->Clone();
        break;
      }
      case PatchOpKind::kSetState: {
        const std::string* state = op_dict->FindString("state");
        if (!state || op.target_component.empty() ||
            !ComponentStateFromString(*state, &op.state)) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        if (const std::string* message = op_dict->FindString("message")) {
          op.state_message = *message;
        }
        break;
      }
      case PatchOpKind::kSetTitle: {
        const std::string* title = op_dict->FindString("title");
        if (!title) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        op.title = *title;
        break;
      }
      case PatchOpKind::kUpsertDataEntry: {
        const base::Value::Dict* entry = op_dict->FindDict("value");
        if (!entry || op.entry_name.empty()) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        auto parsed = ParseDataEntryValue(*entry);
        if (!parsed.has_value()) {
          return base::unexpected(parsed.error());
        }
        op.entry = std::move(parsed.value());
        break;
      }
      case PatchOpKind::kAppendSeriesPoints: {
        const base::Value::List* points = op_dict->FindList("points");
        if (!points || points->empty() || op.entry_name.empty() ||
            points->size() > kMaxSeriesPoints) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        for (const base::Value& point_value : *points) {
          const base::Value::Dict* point = point_value.GetIfDict();
          if (!point) {
            return base::unexpected(SauiError::kInvalidPatch);
          }
          SeriesPoint parsed;
          std::optional<double> y = point->FindDouble("y");
          std::optional<double> t_ms = point->FindDouble("t_ms");
          std::optional<double> x = point->FindDouble("x");
          if (!y || !std::isfinite(*y) || (t_ms.has_value() == x.has_value())) {
            return base::unexpected(SauiError::kInvalidPatch);
          }
          parsed.y = *y;
          if (t_ms) {
            if (!std::isfinite(*t_ms)) {
              return base::unexpected(SauiError::kInvalidPatch);
            }
            parsed.has_time = true;
            parsed.time = base::Time::UnixEpoch() + base::Milliseconds(*t_ms);
          } else {
            if (!std::isfinite(*x)) {
              return base::unexpected(SauiError::kInvalidPatch);
            }
            parsed.x = *x;
          }
          op.points.push_back(parsed);
        }
        break;
      }
      case PatchOpKind::kAppendChild:
      case PatchOpKind::kReplaceComponent: {
        const base::Value::Dict* component = op_dict->FindDict("component");
        if (!component || op.target_component.empty()) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        auto parsed = ParseComponentValue(*component);
        if (!parsed.has_value()) {
          return base::unexpected(parsed.error());
        }
        op.component = std::move(parsed.value());
        break;
      }
      case PatchOpKind::kRemoveComponent: {
        if (op.target_component.empty()) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        break;
      }
      case PatchOpKind::kSetActions: {
        const base::Value::List* actions = op_dict->FindList("actions");
        if (!actions || actions->size() > kMaxSurfaceActions) {
          return base::unexpected(SauiError::kInvalidPatch);
        }
        for (const base::Value& action_value : *actions) {
          const base::Value::Dict* action_dict = action_value.GetIfDict();
          if (!action_dict) {
            return base::unexpected(SauiError::kInvalidPatch);
          }
          auto parsed = ParseActionValue(*action_dict);
          if (!parsed.has_value()) {
            return base::unexpected(parsed.error());
          }
          op.actions.push_back(std::move(parsed.value()));
        }
        break;
      }
    }
    patch.ops.push_back(std::move(op));
  }
  return patch;
}

SauiResult<AppliedPatch> ApplySurfacePatch(AdaptiveSurface& surface,
                                           const SurfacePatch& patch) {
  if (!(patch.surface_id == surface.id)) {
    return base::unexpected(SauiError::kInvalidPatch);
  }
  if (patch.ops.empty() || patch.ops.size() > kMaxPatchOps) {
    return base::unexpected(SauiError::kPatchLimitExceeded);
  }
  // Apply to a working copy so a failed op or invalid result never leaves a
  // partially patched surface.
  AdaptiveSurface working = surface;
  AppliedPatch summary;
  for (const SurfacePatchOp& op : patch.ops) {
    if (auto result = ApplyOp(working, op, &summary); !result.has_value()) {
      return base::unexpected(result.error());
    }
  }
  if (auto valid = ValidateSurface(working); !valid.has_value()) {
    return base::unexpected(valid.error());
  }
  working.revision = surface.revision + 1;
  summary.new_revision = working.revision;
  surface = std::move(working);
  return summary;
}

}  // namespace seoul
