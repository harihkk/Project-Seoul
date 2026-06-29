// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/lifecycle_coordinator.h"

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "seoul/browser/organization/organization_errors.h"
#include "seoul/browser/organization/organization_model.h"

namespace seoul {

LifecycleCoordinator::LifecycleCoordinator(
    OrganizationModel* model,
    base::RepeatingClosure schedule_persist)
    : model_(model), schedule_persist_(std::move(schedule_persist)) {}

LifecycleCoordinator::~LifecycleCoordinator() = default;

void LifecycleCoordinator::OnNormalizedEvent(const NormalizedEvent& event) {
  // Reentrancy guard: never process an event while applying another. Inbound
  // events do not nest; this protects against a future outbound mutation
  // re-entering through an observer.
  if (applying_) {
    return;
  }
  // Ignore late callbacks after shutdown except the shutdown event itself.
  if (shutting_down_ && event.type != NormalizedEventType::kShutdownBegan) {
    return;
  }

  base::AutoReset<bool> applying(&applying_, true);
  const MutationOrigin effective =
      reconciling_ ? MutationOrigin::kStartupReconciliation : event.origin;
  base::AutoReset<MutationOrigin> origin(&current_origin_, effective);

  switch (event.type) {
    case NormalizedEventType::kWindowDiscovered:
      HandleWindowDiscovered(event);
      break;
    case NormalizedEventType::kWindowClosing:
      // Observation only; observer detachment is owned by the window watcher.
      break;
    case NormalizedEventType::kWindowDestroyed:
    case NormalizedEventType::kTabStripDestroyed:
      HandleWindowGone(event);
      break;
    case NormalizedEventType::kTabInserted:
      HandleTabInserted(event);
      break;
    case NormalizedEventType::kTabRemoved:
      HandleTabRemoved(event);
      break;
    case NormalizedEventType::kTabMoved:
      HandleTabMoved(event);
      break;
    case NormalizedEventType::kActiveTabChanged:
      HandleActiveTabChanged(event);
      break;
    case NormalizedEventType::kPinnedStateChanged:
      HandlePinnedStateChanged(event);
      break;
    case NormalizedEventType::kTabReplaced:
      // Logical tab preserved across WebContents replacement/discard; the
      // adapter rebinds transient observation. No membership change.
      break;
    case NormalizedEventType::kTabCloseCancelled:
      // Seoul acts only on a real removal, so a cancelled close needs no undo.
      break;
    case NormalizedEventType::kSplitAdded:
      HandleSplitAdded(event);
      break;
    case NormalizedEventType::kSplitRemoved:
      HandleSplitRemoved(event);
      break;
    case NormalizedEventType::kSplitContentsChanged:
      HandleSplitContentsChanged(event);
      break;
    case NormalizedEventType::kSplitVisualsChanged:
      HandleSplitVisualsChanged(event);
      break;
    case NormalizedEventType::kReconciliationBegan:
      reconciling_ = true;
      break;
    case NormalizedEventType::kReconciliationCompleted:
      HandleReconciliationCompleted(event);
      break;
    case NormalizedEventType::kShutdownBegan:
      shutting_down_ = true;
      pending_transfers_.clear();
      NotePersist();
      break;
  }
}

void LifecycleCoordinator::HandleWindowDiscovered(
    const NormalizedEvent& event) {
  if (!event.window.is_valid()) {
    return;
  }
  // Idempotent: a window is registered at most once.
  if (known_windows_.count(event.window)) {
    return;
  }
  known_windows_.insert(event.window);

  model_->EnsureDefaultWorkspace();
  const std::string window_key = event.window.value();
  // Recover the window's persisted active workspace, else project the default.
  const WorkspaceId active = model_->ActiveWorkspaceForWindow(window_key);
  if (!active.is_valid()) {
    model_->SetActiveWorkspaceForWindow(window_key,
                                        model_->default_workspace());
  }
  NotePersist();
}

void LifecycleCoordinator::HandleWindowGone(const NormalizedEvent& event) {
  if (!event.window.is_valid()) {
    return;
  }
  known_windows_.erase(event.window);
  // Forget only the window projection. Workspaces and their memberships are
  // preserved; the window's tabs are not individually closed here (window
  // teardown owns that transition; their metadata survives for restoration).
  const MutationStatus status = model_->ForgetWindow(event.window.value());
  if (status.has_value()) {
    NotePersist();
  }
}

void LifecycleCoordinator::HandleTabInserted(const NormalizedEvent& event) {
  if (!event.tab.is_valid()) {
    return;
  }
  const std::string tab_key = event.tab.value();
  const TabMembershipId existing = model_->FindMembershipIdByTabKey(tab_key);

  // Transfer-in: consume a pending transfer. Membership was preserved on
  // detach, so no new membership is created.
  auto pending = pending_transfers_.find(event.tab);
  if (pending != pending_transfers_.end()) {
    pending_transfers_.erase(pending);
    if (existing.is_valid() && event.order_index >= 0) {
      model_->ReorderTabMembership(existing, event.order_index);
    }
    NotePersist();
    return;
  }

  // Already tracked (duplicate insertion, or a restored tab matched by key):
  // never create a second membership.
  if (existing.is_valid()) {
    return;
  }

  // Genuinely new tab: assign to the window's active-or-default workspace.
  const WorkspaceId ws = ActiveOrDefaultWorkspace(event.window);
  if (!ws.is_valid()) {
    return;  // No eligible workspace; do not assign an unsupported tab.
  }
  const MutationResult<TabMembershipId> result =
      model_->AddTabMembership(ws, tab_key, kNewTabRole);
  if (result.has_value()) {
    if (event.order_index >= 0) {
      model_->ReorderTabMembership(result.value(), event.order_index);
    }
    NotePersist();
  }
}

void LifecycleCoordinator::HandleTabRemoved(const NormalizedEvent& event) {
  if (!event.tab.is_valid()) {
    return;
  }
  switch (event.removal_kind) {
    case TabRemovalKind::kGenuineClose: {
      // A real close removes the live membership. Chromium already owns
      // recently closed tabs and session restore, so Seoul does NOT
      // auto-archive here.
      const TabMembershipId id =
          model_->FindMembershipIdByTabKey(event.tab.value());
      if (id.is_valid()) {
        const MutationStatus status = model_->RemoveTabMembership(id);
        if (status.has_value()) {
          NotePersist();
        }
      }
      break;
    }
    case TabRemovalKind::kTransferOut: {
      // Detach for transfer to another window: preserve membership, remember
      // the workspace so the matching insertion can be reconciled without
      // duplicating.
      const TabMembershipId id =
          model_->FindMembershipIdByTabKey(event.tab.value());
      const TabMembershipRecord* m =
          id.is_valid() ? model_->FindMembership(id) : nullptr;
      if (m) {
        EvictOldestTransferIfNeeded();
        pending_transfers_[event.tab] = {m->workspace_id, ++sequence_};
      }
      break;  // No model mutation, no persist.
    }
    case TabRemovalKind::kSidePanel:
    case TabRemovalKind::kReplaced:
    case TabRemovalKind::kWindowShutdown:
    case TabRemovalKind::kStripDestroyed:
    case TabRemovalKind::kUnknown:
      // Preserve membership: side-panel/replace keep the logical tab, and
      // window/strip teardown is owned by the window transition. No per-tab
      // close mutation is emitted.
      break;
  }
}

void LifecycleCoordinator::HandleTabMoved(const NormalizedEvent& event) {
  // An intra-window index change updates ordering only; it is never a workspace
  // change.
  if (!event.tab.is_valid() || event.order_index < 0) {
    return;
  }
  const TabMembershipId id =
      model_->FindMembershipIdByTabKey(event.tab.value());
  if (!id.is_valid()) {
    return;
  }
  const MutationStatus status =
      model_->ReorderTabMembership(id, event.order_index);
  if (status.has_value()) {
    NotePersist();
  }
}

void LifecycleCoordinator::HandleActiveTabChanged(
    const NormalizedEvent& event) {
  if (!event.tab.is_valid()) {
    return;
  }
  const TabMembershipId id =
      model_->FindMembershipIdByTabKey(event.tab.value());
  if (!id.is_valid()) {
    // An untracked, unsupported tab becoming active never creates a membership.
    return;
  }
  model_->TouchTabActivated(id);

  // Activating a tab in another workspace switches the window to that
  // workspace.
  const TabMembershipRecord* m = model_->FindMembership(id);
  if (m && event.window.is_valid() && m->workspace_id.is_valid()) {
    const WorkspaceId current =
        model_->ActiveWorkspaceForWindow(event.window.value());
    if (!(m->workspace_id == current)) {
      model_->SetActiveWorkspaceForWindow(event.window.value(),
                                          m->workspace_id);
    }
  }
  NotePersist();
}

void LifecycleCoordinator::HandlePinnedStateChanged(
    const NormalizedEvent& event) {
  if (!event.tab.is_valid()) {
    return;
  }
  const TabMembershipId id =
      model_->FindMembershipIdByTabKey(event.tab.value());
  if (!id.is_valid()) {
    return;
  }
  // Mapping: a Chromium pinned tab becomes a Seoul WORKSPACE-pinned tab. It is
  // never collapsed into a global Essential. The inbound bridge does not
  // capture page content, so no saved reset URL is supplied here (RESEARCH
  // REQUIRED: the source of a pin's reset URL for the later command layer).
  const MutationStatus status =
      event.pinned ? model_->PinTab(id, std::string()) : model_->UnpinTab(id);
  if (status.has_value()) {
    NotePersist();
  }
}

void LifecycleCoordinator::HandleSplitAdded(const NormalizedEvent& event) {
  if (event.upstream_split_token.empty() || !event.split_pane_a.is_valid() ||
      !event.split_pane_b.is_valid()) {
    return;
  }
  // Duplicate split event: a split for this upstream token already exists.
  if (FindSplitByToken(event.upstream_split_token).is_valid()) {
    return;
  }
  const TabMembershipId a =
      model_->FindMembershipIdByTabKey(event.split_pane_a.value());
  const TabMembershipId b =
      model_->FindMembershipIdByTabKey(event.split_pane_b.value());
  const TabMembershipRecord* ma =
      a.is_valid() ? model_->FindMembership(a) : nullptr;
  const TabMembershipRecord* mb =
      b.is_valid() ? model_->FindMembership(b) : nullptr;
  if (!ma || !mb) {
    return;  // Untracked pane: fail safely.
  }
  if (!(ma->workspace_id == mb->workspace_id)) {
    return;  // Cross-workspace split: rejected (invariant 9).
  }
  const std::vector<std::string> panes = {event.split_pane_a.value(),
                                          event.split_pane_b.value()};
  const MutationResult<SplitGroupId> result = model_->CreateSplitGroup(
      ma->workspace_id, panes, event.divider_ratio, event.upstream_split_token);
  if (result.has_value()) {
    NotePersist();
  }
}

void LifecycleCoordinator::HandleSplitRemoved(const NormalizedEvent& event) {
  const SplitGroupId id = FindSplitByToken(event.upstream_split_token);
  if (!id.is_valid()) {
    return;
  }
  const MutationStatus status = model_->DissolveSplitGroup(id);
  if (status.has_value()) {
    NotePersist();
  }
}

void LifecycleCoordinator::HandleSplitContentsChanged(
    const NormalizedEvent& event) {
  // v0: a pane-membership change dissolves the old Seoul record and recreates
  // it from the new panes (recreation is rejected if it would be
  // cross-workspace).
  const SplitGroupId id = FindSplitByToken(event.upstream_split_token);
  if (id.is_valid()) {
    model_->DissolveSplitGroup(id);
  }
  HandleSplitAdded(event);
}

void LifecycleCoordinator::HandleSplitVisualsChanged(
    const NormalizedEvent& event) {
  if (event.split_visuals_intermediate) {
    // Divider drag in progress: never persist per pixel. The final
    // (non-intermediate) event commits the ratio.
    return;
  }
  const SplitGroupId id = FindSplitByToken(event.upstream_split_token);
  if (!id.is_valid()) {
    return;
  }
  const SplitGroupRecord* s = model_->FindSplit(id);
  const int active = s ? s->active_pane_index : 0;
  const MutationStatus status =
      model_->UpdateSplitLayout(id, event.divider_ratio, active);
  if (status.has_value()) {
    NotePersist();
  }
}

void LifecycleCoordinator::HandleReconciliationCompleted(
    const NormalizedEvent& event) {
  // Idempotent. Unresolved persisted references (memberships whose tab never
  // appeared) are left as bounded restorable metadata; Seoul never fabricates a
  // live tab or reopens a URL to satisfy missing metadata. A future explicit
  // policy may prune them.
  reconciling_ = false;
  NotePersist();
}

WorkspaceId LifecycleCoordinator::ActiveOrDefaultWorkspace(
    const LiveWindowKey& window) {
  WorkspaceId ws;
  if (window.is_valid()) {
    ws = model_->ActiveWorkspaceForWindow(window.value());
  }
  if (!ws.is_valid()) {
    ws = model_->default_workspace();
  }
  return ws;
}

SplitGroupId LifecycleCoordinator::FindSplitByToken(
    const std::string& token) const {
  // Resolved through the model (no cached token map), so it stays consistent
  // even when a split is dissolved implicitly by closing one of its panes.
  return model_->FindSplitIdByUpstreamToken(token);
}

void LifecycleCoordinator::EvictOldestTransferIfNeeded() {
  if (pending_transfers_.size() < kMaxPendingTransfers) {
    return;
  }
  auto oldest = pending_transfers_.begin();
  for (auto it = pending_transfers_.begin(); it != pending_transfers_.end();
       ++it) {
    if (it->second.sequence < oldest->second.sequence) {
      oldest = it;
    }
  }
  pending_transfers_.erase(oldest);
}

void LifecycleCoordinator::NotePersist() {
  if (schedule_persist_) {
    schedule_persist_.Run();
  }
}

}  // namespace seoul
