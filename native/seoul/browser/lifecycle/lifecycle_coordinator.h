// Project Seoul native lifecycle bridge.
// The coordinator consumes NormalizedEvents and updates the OrganizationModel.
// This milestone is INBOUND ONLY: the coordinator never asks Chromium to open,
// close, move, pin, split, or switch a tab. It is pure logic over normalized
// events (no Chromium types), so it is unit-testable without a browser.

#ifndef SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_COORDINATOR_H_
#define SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_COORDINATOR_H_

#include <cstddef>
#include <deque>
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

using ReconciliationRequestCallback = base::RepeatingClosure;

class LifecycleCoordinator : public LifecycleEventSink {
 public:
  explicit LifecycleCoordinator(OrganizationModel* model);
  LifecycleCoordinator(const LifecycleCoordinator&) = delete;
  LifecycleCoordinator& operator=(const LifecycleCoordinator&) = delete;
  ~LifecycleCoordinator() override;

  // LifecycleEventSink:
  void OnNormalizedEvent(const NormalizedEvent& event) override;

  void SetReconciliationRequestCallback(ReconciliationRequestCallback callback);
  void SetConfirmationCallback(
      base::RepeatingCallback<void(const NormalizedEvent&)> callback);
  void SetPinHandlingSuppressor(
      base::RepeatingCallback<bool(LiveTabKey tab)> suppressor);

  static constexpr TabRole kNewTabRole = TabRole::kTemporary;
  static constexpr size_t kMaxPendingTransfers = 256;
  static constexpr size_t kMaxQueuedEvents = 128;

  MutationOrigin current_origin() const { return current_origin_; }
  bool is_reconciling() const { return reconciling_; }
  bool is_shutting_down() const { return shutting_down_; }
  bool queue_overflow() const { return queue_overflow_; }
  bool reconciliation_required() const { return reconciliation_required_; }
  bool lifecycle_degraded() const {
    return queue_overflow_ || reconciliation_required_;
  }
  size_t pending_transfer_count() const { return pending_transfers_.size(); }
  size_t queued_event_count() const { return pending_events_.size(); }
  size_t known_window_count() const { return known_windows_.size(); }

 private:
  struct PendingTransfer {
    WorkspaceId workspace;
    int sequence = 0;
  };

  void ProcessEvent(const NormalizedEvent& event);
  void DrainPendingEvents();
  static bool EventsAreDuplicate(const NormalizedEvent& a,
                                 const NormalizedEvent& b);

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
  void ExpireTransfersForWindow(const LiveWindowKey& window);
  void HandleQueueOverflow();
  bool ShouldAcceptEvent(const NormalizedEvent& event) const;

  raw_ptr<OrganizationModel> model_;

  MutationOrigin current_origin_ = MutationOrigin::kChromiumObservation;
  bool applying_ = false;
  bool draining_ = false;
  bool reconciling_ = false;
  bool shutting_down_ = false;
  bool queue_overflow_ = false;
  bool reconciliation_required_ = false;
  int sequence_ = 0;

  ReconciliationRequestCallback reconciliation_request_callback_;
  base::RepeatingCallback<void(const NormalizedEvent&)> confirmation_callback_;
  base::RepeatingCallback<bool(LiveTabKey)> pin_handling_suppressor_;

  std::deque<NormalizedEvent> pending_events_;
  std::set<LiveWindowKey> known_windows_;
  std::map<LiveTabKey, PendingTransfer> pending_transfers_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_LIFECYCLE_COORDINATOR_H_
