// Project Seoul native lifecycle bridge.
// The coordinator consumes NormalizedEvents and updates the OrganizationModel.
// This milestone is INBOUND ONLY: the coordinator never asks Chromium to open,
// close, move, pin, split, or switch a tab. It is pure logic over normalized
// events (no Chromium types), so it is unit-testable without a browser.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_COORDINATOR_H_
#define SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_COORDINATOR_H_

#include <cstddef>
#include <map>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "seoul/browser/lifecycle/lifecycle_event_sink.h"
#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/organization/organization_ids.h"
#include "seoul/browser/organization/organization_types.h"

namespace seoul {

class OrganizationModel;

class LifecycleCoordinator : public LifecycleEventSink {
 public:
  // `model` must outlive this. `schedule_persist` is run when a committed
  // mutation should eventually be written; the callee coalesces. Both required.
  LifecycleCoordinator(OrganizationModel* model,
                       base::RepeatingClosure schedule_persist);
  LifecycleCoordinator(const LifecycleCoordinator&) = delete;
  LifecycleCoordinator& operator=(const LifecycleCoordinator&) = delete;
  ~LifecycleCoordinator() override;

  // LifecycleEventSink:
  void OnNormalizedEvent(const NormalizedEvent& event) override;

  // A genuinely new tab starts temporary (Seoul semantics section 5).
  static constexpr TabRole kNewTabRole = TabRole::kTemporary;

  // Cap on tabs in flight between windows. A transfer that never lands is
  // evicted oldest-first past this cap; shutdown and reconciliation also clear
  // pending transfers. No timer is used.
  static constexpr size_t kMaxPendingTransfers = 256;

  // Exposed for tests and the future outbound command layer.
  MutationOrigin current_origin() const { return current_origin_; }
  bool is_reconciling() const { return reconciling_; }
  bool is_shutting_down() const { return shutting_down_; }
  size_t pending_transfer_count() const { return pending_transfers_.size(); }
  size_t known_window_count() const { return known_windows_.size(); }

 private:
  struct PendingTransfer {
    WorkspaceId workspace;
    int sequence = 0;
  };

  void HandleWindowDiscovered(const NormalizedEvent& event);
  void HandleWindowGone(const NormalizedEvent& event);
  void HandleTabInserted(const NormalizedEvent& event);
  void HandleTabRemoved(const NormalizedEvent& event);
  void HandleTabMoved(const NormalizedEvent& event);
  void HandleActiveTabChanged(const NormalizedEvent& event);
  void HandlePinnedStateChanged(const NormalizedEvent& event);
  void HandleSplitAdded(const NormalizedEvent& event);
  void HandleSplitRemoved(const NormalizedEvent& event);
  void HandleSplitContentsChanged(const NormalizedEvent& event);
  void HandleSplitVisualsChanged(const NormalizedEvent& event);
  void HandleReconciliationCompleted(const NormalizedEvent& event);

  WorkspaceId ActiveOrDefaultWorkspace(const LiveWindowKey& window);
  SplitGroupId FindSplitByToken(const std::string& token) const;
  void EvictOldestTransferIfNeeded();
  void NotePersist();

  raw_ptr<OrganizationModel> model_;
  base::RepeatingClosure schedule_persist_;

  MutationOrigin current_origin_ = MutationOrigin::kChromiumObservation;
  bool applying_ = false;     // reentrancy guard
  bool reconciling_ = false;  // between reconciliation begin and complete
  bool shutting_down_ = false;
  int sequence_ = 0;

  std::set<LiveWindowKey> known_windows_;
  std::map<LiveTabKey, PendingTransfer> pending_transfers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_COORDINATOR_H_
