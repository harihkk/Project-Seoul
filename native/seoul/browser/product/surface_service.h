// Project Seoul product runtime: the surface service.
// Production owner of adaptive surfaces. Stores each surface together with
// the semantic result and interface intent that produced it, so any generic
// follow-up ("show as chart", "hide that field", "group by this", "compare
// these") is a re-compile of the SAME surface id under a modified intent -
// never an unrelated duplicate, and never a phrase-specific handler. Resolves
// typed component events against the surface's validated action list.
//
// STATE OWNERSHIP
//   owner:        one SurfaceService per profile runtime.
//   lifetime:     the profile runtime.
//   persistence:  pinned surfaces serialize through TakePersistedState() /
//                 RestorePersistedState() (the runtime service owns the pref).
//   recovery:     unpinned surfaces are session-scoped; pinned ones restore.
//   teardown:     dropped with the runtime; no callbacks.
//   bounds:       kMaxSurfaces with oldest-unpinned eviction.
//   isolation:    per profile.

#ifndef SEOUL_BROWSER_PRODUCT_SURFACE_SERVICE_H_
#define SEOUL_BROWSER_PRODUCT_SURFACE_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/values.h"
#include "seoul/browser/saui/interface_compiler.h"
#include "seoul/browser/saui/saui_events.h"
#include "seoul/browser/saui/saui_types.h"
#include "seoul/browser/semantic/semantic_types.h"
#include "seoul/browser/tasks/task_types.h"

namespace seoul {

// What a validated component event resolved to. The browser decides; the
// renderer's report is advisory.
struct SurfaceEventOutcome {
  enum class Kind {
    kNone,            // no declared action / local-only state
    kRunCapability,   // execute action target as a capability with payload
    kTaskApproval,    // approve/reject the task step in `target`
    kNavigate,        // open validated http(s) URL in `target`
    kBrowserCommand,  // launcher catalog command name in `target`
    kWorkflowEdit,    // workflow node edit; target is the node id
    kSubmitTurn,      // form submission routed as a typed turn
  };

  SurfaceEventOutcome();
  SurfaceEventOutcome(SurfaceEventOutcome&&);
  SurfaceEventOutcome& operator=(SurfaceEventOutcome&&);
  ~SurfaceEventOutcome();

  Kind kind = Kind::kNone;
  std::string target;
  base::Value::Dict payload;  // declared action payload merged with the value
  SurfaceId surface_id;
};

class SurfaceServiceObserver : public base::CheckedObserver {
 public:
  // Fired when a surface is created or recompiled; `surface_json` is the full
  // validated document for the renderer.
  virtual void OnSurfaceUpdated(const SurfaceId& id,
                                const std::string& surface_json) = 0;
  virtual void OnSurfaceRemoved(const SurfaceId& id) {}
};

class SurfaceService {
 public:
  SurfaceService();
  SurfaceService(const SurfaceService&) = delete;
  SurfaceService& operator=(const SurfaceService&) = delete;
  ~SurfaceService();

  // Compiles `result` under `intent` into a new surface. Returns the id, or
  // an invalid id when compilation fails (the caller shows the error state).
  SurfaceId CreateFromSemantic(const SemanticResult& result,
                               const InterfaceIntent& intent,
                               const TaskId& task_id);

  // Generic representation changes: each mutates the stored intent and
  // recompiles the SAME surface id in place. Returns false for unknown ids or
  // failed compiles (prior surface stays).
  bool SetRepresentation(const SurfaceId& id, const std::string& wire_name);
  bool ToggleFieldHidden(const SurfaceId& id, const std::string& field_id);
  bool SetGroupBy(const SurfaceId& id, const std::string& field_id);
  bool SetCompareEntities(const SurfaceId& id,
                          const std::vector<std::string>& entity_ids);
  bool SetEditable(const SurfaceId& id, bool editable);
  bool SetPinned(const SurfaceId& id, bool pinned);
  bool SetTitle(const SurfaceId& id, const std::string& title);

  // Replaces the stored semantic result (a refreshed capability run) and
  // recompiles under the existing intent, keeping the id.
  bool RefreshSemantic(const SurfaceId& id, const SemanticResult& result);

  bool Remove(const SurfaceId& id);

  // Validates and resolves one renderer event against the surface's declared
  // actions. Unknown surface/component/action ids resolve to kNone (fail
  // closed); a kSubmit event with no declared action becomes kSubmitTurn so
  // form flows work without arbitrary handlers.
  SurfaceEventOutcome HandleComponentEvent(const ComponentEvent& event);

  // Association: surfaces created by a task; used by the deck and threads.
  std::vector<SurfaceId> SurfacesForTask(const TaskId& task_id) const;

  std::optional<std::string> SurfaceJson(const SurfaceId& id) const;
  std::vector<SurfaceId> PinnedSurfaces() const;
  std::vector<SurfaceId> AllSurfaces() const;
  const AdaptiveSurface* FindSurface(const SurfaceId& id) const;
  std::vector<CompilerReason> ReasonsFor(const SurfaceId& id) const;

  // Pinned-surface persistence, owned by the runtime service's pref plumbing.
  base::Value::Dict TakePersistedState() const;
  void RestorePersistedState(const base::Value::Dict& state);

  void AddObserver(SurfaceServiceObserver* observer);
  void RemoveObserver(SurfaceServiceObserver* observer);

  size_t size() const { return surfaces_.size(); }

 private:
  struct StoredSurface {
    StoredSurface();
    ~StoredSurface();

    SemanticResult semantic;
    InterfaceIntent intent;
    AdaptiveSurface surface;
    std::vector<CompilerReason> reasons;
    TaskId task_id;
    base::Time created_at;
  };

  bool Recompile(const SurfaceId& id, StoredSurface& stored);
  void EvictIfNeeded();
  void NotifyUpdated(const SurfaceId& id, const StoredSurface& stored);

  std::map<SurfaceId, std::unique_ptr<StoredSurface>> surfaces_;
  base::ObserverList<SurfaceServiceObserver> observers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_SURFACE_SERVICE_H_
