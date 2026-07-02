// Project Seoul product runtime: the surface service.

#include "seoul/browser/product/surface_service.h"

#include <algorithm>
#include <utility>

#include "base/json/json_writer.h"
#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_document.h"
#include "url/gurl.h"

namespace seoul {

namespace {

// Live + pinned surfaces held per profile. Oldest unpinned evict first.
constexpr size_t kMaxSurfaces = 128;

std::string SerializeSurface(const AdaptiveSurface& surface) {
  std::string json;
  base::JSONWriter::Write(SurfaceToValue(surface), &json);
  return json;
}

}  // namespace

SurfaceEventOutcome::SurfaceEventOutcome() = default;
SurfaceEventOutcome::SurfaceEventOutcome(SurfaceEventOutcome&&) = default;
SurfaceEventOutcome& SurfaceEventOutcome::operator=(SurfaceEventOutcome&&) =
    default;
SurfaceEventOutcome::~SurfaceEventOutcome() = default;

SurfaceService::StoredSurface::StoredSurface() = default;
SurfaceService::StoredSurface::~StoredSurface() = default;

SurfaceService::SurfaceService() = default;
SurfaceService::~SurfaceService() = default;

SurfaceId SurfaceService::CreateFromSemantic(const SemanticResult& result,
                                             const InterfaceIntent& intent,
                                             const TaskId& task_id) {
  CompileResult compiled = CompileInterface(result, intent);
  if (!compiled.has_value()) {
    return SurfaceId();
  }
  auto stored = std::make_unique<StoredSurface>();
  stored->semantic = result;
  stored->intent = intent;
  stored->surface = std::move(compiled->surface);
  stored->reasons = std::move(compiled->reasons);
  stored->task_id = task_id;
  stored->created_at = base::Time::Now();
  const SurfaceId id = stored->surface.id;
  // Insert before notifying so a synchronous observer that reads back the
  // surface (SurfaceJson/FindSurface) sees it.
  surfaces_[id] = std::move(stored);
  NotifyUpdated(id, *surfaces_[id]);
  EvictIfNeeded();
  return id;
}

bool SurfaceService::Recompile(const SurfaceId& id, StoredSurface& stored) {
  CompileResult compiled = CompileInterface(stored.semantic, stored.intent, id);
  if (!compiled.has_value()) {
    return false;
  }
  stored.surface = std::move(compiled->surface);
  stored.reasons = std::move(compiled->reasons);
  NotifyUpdated(id, stored);
  return true;
}

bool SurfaceService::SetRepresentation(const SurfaceId& id,
                                       const std::string& wire_name) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  const ComponentTypeInfo* info = FindComponentTypeByName(wire_name);
  if (!info) {
    return false;  // unknown component types fail closed
  }
  InterfaceIntent previous = it->second->intent;
  it->second->intent.requested_representation = info->type;
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::ToggleFieldHidden(const SurfaceId& id,
                                       const std::string& field_id) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end() || field_id.empty()) {
    return false;
  }
  InterfaceIntent previous = it->second->intent;
  std::vector<std::string>& hidden = it->second->intent.hidden_field_ids;
  auto hidden_it = std::find(hidden.begin(), hidden.end(), field_id);
  if (hidden_it == hidden.end()) {
    hidden.push_back(field_id);
  } else {
    hidden.erase(hidden_it);
  }
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::SetGroupBy(const SurfaceId& id,
                                const std::string& field_id) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  InterfaceIntent previous = it->second->intent;
  it->second->intent.group_by_field_id = field_id;
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::SetCompareEntities(
    const SurfaceId& id,
    const std::vector<std::string>& entity_ids) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  InterfaceIntent previous = it->second->intent;
  it->second->intent.compare_entity_ids = entity_ids;
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::SetEditable(const SurfaceId& id, bool editable) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  InterfaceIntent previous = it->second->intent;
  it->second->intent.wants_editable = editable;
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::SetPinned(const SurfaceId& id, bool pinned) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  InterfaceIntent previous = it->second->intent;
  it->second->intent.pin = pinned;
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::SetTitle(const SurfaceId& id, const std::string& title) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end() || title.empty()) {
    return false;
  }
  InterfaceIntent previous = it->second->intent;
  it->second->intent.title = title;
  if (!Recompile(id, *it->second)) {
    it->second->intent = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::RefreshSemantic(const SurfaceId& id,
                                     const SemanticResult& result) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  SemanticResult previous = it->second->semantic;
  it->second->semantic = result;
  if (!Recompile(id, *it->second)) {
    it->second->semantic = std::move(previous);
    return false;
  }
  return true;
}

bool SurfaceService::Remove(const SurfaceId& id) {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return false;
  }
  surfaces_.erase(it);
  for (SurfaceServiceObserver& observer : observers_) {
    observer.OnSurfaceRemoved(id);
  }
  return true;
}

SurfaceEventOutcome SurfaceService::HandleComponentEvent(
    const ComponentEvent& event) {
  SurfaceEventOutcome outcome;
  outcome.surface_id = event.surface_id;
  auto it = surfaces_.find(event.surface_id);
  if (it == surfaces_.end()) {
    return outcome;  // unknown surface: fail closed
  }
  const AdaptiveSurface& surface = it->second->surface;

  if (!event.action_id.has_value()) {
    // A submit without a declared action becomes a typed turn (generic form
    // flow); everything else is renderer-local.
    if (event.kind == ComponentEventKind::kSubmit && event.value.is_dict()) {
      outcome.kind = SurfaceEventOutcome::Kind::kSubmitTurn;
      outcome.payload = event.value.GetDict().Clone();
    }
    return outcome;
  }

  const SurfaceAction* action = nullptr;
  for (const SurfaceAction& candidate : surface.actions) {
    if (candidate.id == event.action_id.value()) {
      action = &candidate;
      break;
    }
  }
  if (!action) {
    return outcome;  // undeclared action id: fail closed
  }

  outcome.target = action->target;
  // The server-declared action payload is authoritative; the renderer's value
  // is advisory and fills only keys the action did not declare. Start from the
  // renderer input, then overlay the declared payload so a declared argument
  // can never be overridden by the renderer.
  if (event.value.is_dict()) {
    outcome.payload = event.value.GetDict().Clone();
  } else if (!event.value.is_none()) {
    outcome.payload.Set("value", event.value.Clone());
  }
  outcome.payload.Merge(action->payload.Clone());

  switch (action->kind) {
    case SurfaceActionKind::kToolCall:
      outcome.kind = ToolId::IsValidString(action->target)
                         ? SurfaceEventOutcome::Kind::kRunCapability
                         : SurfaceEventOutcome::Kind::kNone;
      break;
    case SurfaceActionKind::kLocalState:
      outcome.kind = SurfaceEventOutcome::Kind::kNone;
      break;
    case SurfaceActionKind::kWorkflowEdit:
      outcome.kind = SurfaceEventOutcome::Kind::kWorkflowEdit;
      break;
    case SurfaceActionKind::kBrowserAction:
      outcome.kind = SurfaceEventOutcome::Kind::kBrowserCommand;
      break;
    case SurfaceActionKind::kTaskApproval:
      outcome.kind = SurfaceEventOutcome::Kind::kTaskApproval;
      break;
    case SurfaceActionKind::kNavigate: {
      const GURL url(action->target);
      outcome.kind = url.is_valid() && url.SchemeIsHTTPOrHTTPS()
                         ? SurfaceEventOutcome::Kind::kNavigate
                         : SurfaceEventOutcome::Kind::kNone;
      break;
    }
  }
  return outcome;
}

std::vector<SurfaceId> SurfaceService::SurfacesForTask(
    const TaskId& task_id) const {
  std::vector<SurfaceId> out;
  for (const auto& [id, stored] : surfaces_) {
    if (stored->task_id == task_id) {
      out.push_back(id);
    }
  }
  return out;
}

std::optional<std::string> SurfaceService::SurfaceJson(
    const SurfaceId& id) const {
  auto it = surfaces_.find(id);
  if (it == surfaces_.end()) {
    return std::nullopt;
  }
  return SerializeSurface(it->second->surface);
}

std::vector<SurfaceId> SurfaceService::PinnedSurfaces() const {
  std::vector<SurfaceId> out;
  for (const auto& [id, stored] : surfaces_) {
    if (stored->surface.pinned) {
      out.push_back(id);
    }
  }
  return out;
}

std::vector<SurfaceId> SurfaceService::AllSurfaces() const {
  std::vector<SurfaceId> out;
  out.reserve(surfaces_.size());
  for (const auto& [id, stored] : surfaces_) {
    out.push_back(id);
  }
  return out;
}

const AdaptiveSurface* SurfaceService::FindSurface(const SurfaceId& id) const {
  auto it = surfaces_.find(id);
  return it != surfaces_.end() ? &it->second->surface : nullptr;
}

std::vector<CompilerReason> SurfaceService::ReasonsFor(
    const SurfaceId& id) const {
  auto it = surfaces_.find(id);
  return it != surfaces_.end() ? it->second->reasons
                               : std::vector<CompilerReason>();
}

base::Value::Dict SurfaceService::TakePersistedState() const {
  base::Value::Dict state;
  base::Value::List pinned;
  for (const auto& [id, stored] : surfaces_) {
    if (!stored->surface.pinned) {
      continue;
    }
    base::Value::Dict entry;
    entry.Set("surface", SurfaceToValue(stored->surface));
    pinned.Append(std::move(entry));
  }
  state.Set("pinned", std::move(pinned));
  return state;
}

void SurfaceService::RestorePersistedState(const base::Value::Dict& state) {
  const base::Value::List* pinned = state.FindList("pinned");
  if (!pinned) {
    return;
  }
  for (const base::Value& entry : *pinned) {
    const base::Value::Dict* dict = entry.GetIfDict();
    if (!dict) {
      continue;
    }
    const base::Value::Dict* surface_value = dict->FindDict("surface");
    if (!surface_value) {
      continue;
    }
    SauiResult<AdaptiveSurface> parsed =
        ParseSurface(base::Value(surface_value->Clone()));
    if (!parsed.has_value()) {
      continue;  // corrupt persisted surface: skipped, never guessed at
    }
    auto stored = std::make_unique<StoredSurface>();
    stored->surface = std::move(parsed.value());
    stored->intent.pin = true;
    stored->intent.title = stored->surface.title;
    stored->created_at = base::Time::Now();
    const SurfaceId id = stored->surface.id;
    surfaces_[id] = std::move(stored);
    NotifyUpdated(id, *surfaces_[id]);
  }
  EvictIfNeeded();
}

void SurfaceService::AddObserver(SurfaceServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void SurfaceService::RemoveObserver(SurfaceServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SurfaceService::EvictIfNeeded() {
  while (surfaces_.size() > kMaxSurfaces) {
    auto oldest = surfaces_.end();
    for (auto it = surfaces_.begin(); it != surfaces_.end(); ++it) {
      if (it->second->surface.pinned) {
        continue;
      }
      if (oldest == surfaces_.end() ||
          it->second->created_at < oldest->second->created_at) {
        oldest = it;
      }
    }
    if (oldest == surfaces_.end()) {
      return;  // everything pinned; the create path rejects beyond this
    }
    const SurfaceId id = oldest->first;
    surfaces_.erase(oldest);
    for (SurfaceServiceObserver& observer : observers_) {
      observer.OnSurfaceRemoved(id);
    }
  }
}

void SurfaceService::NotifyUpdated(const SurfaceId& id,
                                   const StoredSurface& stored) {
  const std::string json = SerializeSurface(stored.surface);
  for (SurfaceServiceObserver& observer : observers_) {
    observer.OnSurfaceUpdated(id, json);
  }
}

}  // namespace seoul
