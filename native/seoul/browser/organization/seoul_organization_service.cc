// Project Seoul native organization engine.

#include "seoul/browser/organization/seoul_organization_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/lifecycle/persistence_scheduler.h"
#include "seoul/browser/lifecycle/window_watcher.h"
#include "seoul/browser/organization/organization_store.h"

namespace seoul {

SeoulOrganizationService::SeoulOrganizationService(Profile* profile,
                                                   PrefService* prefs)
    : profile_(profile), prefs_(prefs) {
  observation_.Observe(&model_);
  LoadFromPrefs();

  // Persistence is coalesced through the scheduler. The writer is the in-memory
  // pref write; a burst of events collapses into a single posted write.
  scheduler_ = std::make_unique<PersistenceScheduler>(
      base::BindRepeating(&SeoulOrganizationService::WriteToPrefs,
                          base::Unretained(this)),
      base::SequencedTaskRunner::GetCurrentDefault());

  // The coordinator applies normalized events to the model and schedules
  // writes.
  coordinator_ = std::make_unique<LifecycleCoordinator>(
      &model_, base::BindRepeating(&PersistenceScheduler::ScheduleWrite,
                                   base::Unretained(scheduler_.get())));

  // The watcher discovers this profile's normal windows and feeds the
  // coordinator. RESEARCH REQUIRED (resolve at the build host): confirm the
  // service is created before or after the first window for this profile.
  // StartObserving handles both (it enumerates existing windows once and
  // observes future ones), but the exact session-restore reconciliation
  // handshake is wired at the build host.
  window_watcher_ =
      std::make_unique<WindowWatcher>(profile_, coordinator_.get());
  window_watcher_->StartObserving();
}

SeoulOrganizationService::~SeoulOrganizationService() = default;

// static
void SeoulOrganizationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // A single, bounded dict. Not syncable in v0: organization layout is local
  // until a sync design exists. The value carries its own schema_version.
  registry->RegisterDictionaryPref(kOrganizationPref);
}

void SeoulOrganizationService::Shutdown() {
  // Detach window/tab observers before the browsers and tab strips are torn
  // down.
  window_watcher_.reset();
  // Tell the coordinator we are shutting down, then flush any pending write
  // once.
  if (coordinator_) {
    NormalizedEvent event;
    event.type = NormalizedEventType::kShutdownBegan;
    coordinator_->OnNormalizedEvent(event);
  }
  if (scheduler_) {
    scheduler_->Flush();
    scheduler_->Shutdown();
  }
}

void SeoulOrganizationService::OnOrganizationChanged(
    const OrganizationChange& change) {
  if (loading_) {
    return;  // do not persist the state we are in the middle of loading
  }
  if (scheduler_) {
    scheduler_->ScheduleWrite();  // coalesced
  } else {
    WriteToPrefs();  // before the scheduler exists (during initial load)
  }
}

void SeoulOrganizationService::LoadFromPrefs() {
  loading_ = true;
  const base::Value::Dict& stored = prefs_->GetDict(kOrganizationPref);
  if (!stored.empty()) {
    MutationResult<OrganizationSnapshot> parsed = DeserializeSnapshot(stored);
    if (parsed.has_value()) {
      // LoadSnapshot is atomic; on semantic failure the in-memory state is
      // untouched and we keep the last known valid state (we do not overwrite
      // prefs here, preserving the stored bytes for manual recovery/diagnosis).
      model_.LoadSnapshot(parsed.value());
    }
    // Corrupt/unsupported stored data is intentionally not overwritten on load.
  }
  // Always guarantee exactly one default workspace for an eligible profile.
  model_.EnsureDefaultWorkspace();
  loading_ = false;
  // Persist the (possibly newly initialized) valid state once.
  WriteToPrefs();
}

bool SeoulOrganizationService::WriteToPrefs() {
  base::Value::Dict dict = SerializeSnapshot(model_.ToSnapshot());
  if (!SerializedSizeWithinLimit(dict)) {
    // Refuse to write an over-limit blob; keep the last valid stored state.
    return false;
  }
  prefs_->SetDict(kOrganizationPref, std::move(dict));
  return true;
}

}  // namespace seoul
