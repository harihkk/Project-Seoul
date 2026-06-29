// Project Seoul native organization engine.
// Profile-scoped KeyedService that owns the pure OrganizationModel and binds it
// to a single bounded profile preference for persistence. This is the only file
// in the engine that depends on Chromium's PrefService/KeyedService; the model
// itself stays Chromium-free.
//
// As of the lifecycle-bridge milestone it also owns the INBOUND lifecycle
// bridge: a per-profile WindowWatcher that attaches one TabStripBridge per
// eligible normal window, a LifecycleCoordinator that applies normalized events
// to the model, and a PersistenceScheduler that coalesces writes. No outbound
// (Seoul -> Chromium) command path exists yet.

#ifndef SEOUL_BROWSER_ORGANIZATION_SEOUL_ORGANIZATION_SERVICE_H_
#define SEOUL_BROWSER_ORGANIZATION_SEOUL_ORGANIZATION_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/organization_observer.h"

class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace seoul {

class LifecycleCoordinator;
class PersistenceScheduler;
class WindowWatcher;

// Single dict preference holding the bounded organization snapshot. Versioned
// in the value itself (schema_version), not in the key.
inline constexpr char kOrganizationPref[] = "seoul.organization.v1";

class SeoulOrganizationService : public KeyedService,
                                 public OrganizationModelObserver {
 public:
  // `profile` is the owning regular profile (used only to discover its
  // windows); `prefs` is its PrefService. Both must outlive the service.
  SeoulOrganizationService(Profile* profile, PrefService* prefs);
  SeoulOrganizationService(const SeoulOrganizationService&) = delete;
  SeoulOrganizationService& operator=(const SeoulOrganizationService&) = delete;
  ~SeoulOrganizationService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  OrganizationModel& model() { return model_; }
  const OrganizationModel& model() const { return model_; }

  // KeyedService:
  void Shutdown() override;

  // OrganizationModelObserver:
  void OnOrganizationChanged(const OrganizationChange& change) override;

 private:
  // Loads the snapshot from prefs. On corrupt/unsupported data it leaves the
  // last known valid in-memory state untouched (does not overwrite prefs).
  // Always ensures a default workspace exists afterward.
  void LoadFromPrefs();
  // Serializes the current model and writes it to prefs if within size limits.
  // Returns true on a successful write, false if the blob exceeds the size cap
  // (in which case the last valid stored state is kept). PrefService writes are
  // in-memory and flushed to disk asynchronously by Chromium, so this adds no
  // blocking file I/O to the UI thread. Used as the PersistenceScheduler
  // writer.
  bool WriteToPrefs();

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> prefs_;
  OrganizationModel model_;
  bool loading_ = false;  // suppress save-on-change while loading

  // Inbound lifecycle bridge (created after the initial load). Destruction
  // order is the reverse of construction: watcher first (detach observers),
  // then coordinator, then scheduler.
  std::unique_ptr<PersistenceScheduler> scheduler_;
  std::unique_ptr<LifecycleCoordinator> coordinator_;
  std::unique_ptr<WindowWatcher> window_watcher_;

  base::ScopedObservation<OrganizationModel, OrganizationModelObserver>
      observation_{this};
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_ORGANIZATION_SEOUL_ORGANIZATION_SERVICE_H_
