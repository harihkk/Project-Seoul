// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/command_executor.h"

#include "base/time/time.h"
#include "seoul/browser/commands/url_policy.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/organization/organization_types.h"

namespace seoul {

CommandExecutor::CommandExecutor(Profile* profile,
                                 OrganizationModel* model,
                                 LifecycleCoordinator* lifecycle,
                                 TargetResolver* resolver,
                                 ChromiumMutationAdapter* adapter)
    : profile_(profile),
      model_(model),
      lifecycle_(lifecycle),
      resolver_(resolver),
      adapter_(adapter),
      model_facade_(model) {}

CommandExecutor::~CommandExecutor() = default;

bool CommandExecutor::IsModelOnlyKind(CommandKind kind) const {
  switch (kind) {
    case CommandKind::kCreateWorkspace:
    case CommandKind::kRenameWorkspace:
    case CommandKind::kReorderWorkspace:
    case CommandKind::kArchiveWorkspace:
    case CommandKind::kRestoreWorkspace:
    case CommandKind::kDeleteWorkspace:
    case CommandKind::kSetActiveWorkspace:
    case CommandKind::kMoveTabMembership:
    case CommandKind::kMarkTemporary:
    case CommandKind::kRetainTab:
    case CommandKind::kSetWorkspacePinned:
    case CommandKind::kUnsetWorkspacePinned:
    case CommandKind::kCreateOrUpdateEssential:
    case CommandKind::kRemoveEssential:
    case CommandKind::kAddRoutingRule:
    case CommandKind::kRemoveRoutingRule:
      return true;
    default:
      return false;
  }
}

CommandResult<CommandStatus> CommandExecutor::ValidateCommand(
    const BrowserCommand& command) {
  if (!command.id.is_valid()) {
    return base::unexpected(CommandError::kInvalidCommand);
  }
  if (shutting_down_) {
    return base::unexpected(CommandError::kShutdown);
  }
  if (lifecycle_ && lifecycle_->lifecycle_degraded()) {
    return base::unexpected(CommandError::kLifecycleQueueDegraded);
  }
  if (lifecycle_ && lifecycle_->reconciliation_required() &&
      !IsModelOnlyKind(command.kind)) {
    return base::unexpected(CommandError::kReconciliationRequired);
  }
  switch (command.kind) {
    case CommandKind::kOpenTemporaryTab:
    case CommandKind::kOpenRetainedTab: {
      CommandStatusResult url = UrlPolicy::ValidateNavigationUrl(command.url);
      if (!url.has_value()) {
        return base::unexpected(url.error());
      }
      break;
    }
    default:
      break;
  }
  return base::ok(CommandStatus::kValidating);
}

CommandResult<CommandStatus> CommandExecutor::Submit(BrowserCommand command) {
  CommandResult<CommandStatus> validated = ValidateCommand(command);
  if (!validated.has_value()) {
    return validated;
  }

  if (IsModelOnlyKind(command.kind)) {
    CommandStatusResult applied = model_facade_.Execute(command);
    if (!applied.has_value()) {
      return base::unexpected(CommandError::kInvalidCommand);
    }
    return base::ok(CommandStatus::kApplied);
  }

  return DispatchChromiumCommand(command);
}

CommandResult<CommandStatus> CommandExecutor::DispatchChromiumCommand(
    const BrowserCommand& command) {
  if (!resolver_ || !adapter_) {
    return base::unexpected(CommandError::kDispatchFailure);
  }

  ExpectedObservation observation = BuildObservation(command);
  if (!registry_.Register(observation).has_value()) {
    return base::unexpected(CommandError::kDuplicateCommandId);
  }
  pending_commands_[command.id] = command;

  CommandStatusResult dispatch = CommandOk();
  switch (command.kind) {
    case CommandKind::kOpenTemporaryTab:
    case CommandKind::kOpenRetainedTab: {
      auto window = resolver_->ResolveWindow(profile_, command.window);
      if (!window.has_value()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kWindowNotFound);
      }
      LiveTabKey inserted;
      dispatch = adapter_->OpenTab(profile_, window.value(), command.url,
                                   command.foreground, &inserted);
      break;
    }
    case CommandKind::kActivateTab: {
      auto tab = resolver_->ResolveTab(profile_, command.window, command.tab);
      if (!tab.has_value()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kTabNotFound);
      }
      if (tab->already_active) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::ok(CommandStatus::kApplied);
      }
      dispatch = adapter_->ActivateTab(profile_, tab.value());
      break;
    }
    case CommandKind::kCloseTab: {
      auto tab = resolver_->ResolveTab(profile_, command.window, command.tab);
      if (!tab.has_value()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kTabNotFound);
      }
      dispatch = adapter_->CloseTab(profile_, tab.value());
      break;
    }
    case CommandKind::kPinTab:
    case CommandKind::kUnpinTab: {
      auto tab = resolver_->ResolveTab(profile_, command.window, command.tab);
      if (!tab.has_value()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kTabNotFound);
      }
      const bool want_pinned = command.kind == CommandKind::kPinTab;
      if (tab->already_pinned == want_pinned) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::ok(CommandStatus::kApplied);
      }
      dispatch = adapter_->SetPinned(profile_, tab.value(), want_pinned);
      break;
    }
    case CommandKind::kMoveTabWithinWindow: {
      auto tab = resolver_->ResolveTab(profile_, command.window, command.tab);
      if (!tab.has_value()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kTabNotFound);
      }
      dispatch =
          adapter_->MoveTab(profile_, tab.value(), command.destination_index);
      break;
    }
    case CommandKind::kCreateSplit: {
      auto split = resolver_->ResolveSplitPanes(
          profile_, command.window, command.tab, command.split_pane_b);
      if (!split.has_value()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kSplitPreconditionFailure);
      }
      const TabMembershipId id_a =
          model_->FindMembershipIdByTabKey(split->pane_a.value());
      const TabMembershipId id_b =
          model_->FindMembershipIdByTabKey(split->pane_b.value());
      const TabMembershipRecord* ma =
          id_a.is_valid() ? model_->FindMembership(id_a) : nullptr;
      const TabMembershipRecord* mb =
          id_b.is_valid() ? model_->FindMembership(id_b) : nullptr;
      if (!ma || !mb || !(ma->workspace_id == mb->workspace_id)) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kSplitPreconditionFailure);
      }
      std::string token;
      dispatch = adapter_->CreateSplit(profile_, split.value(),
                                       command.split_ratio, &token);
      break;
    }
    case CommandKind::kDissolveSplit: {
      if (command.upstream_split_token.empty()) {
        registry_.Consume(command.id);
        pending_commands_.erase(command.id);
        return base::unexpected(CommandError::kSplitPreconditionFailure);
      }
      dispatch = adapter_->DissolveSplit(profile_, command.window,
                                         command.upstream_split_token);
      break;
    }
    default:
      registry_.Consume(command.id);
      pending_commands_.erase(command.id);
      return base::unexpected(CommandError::kUnsupportedCommand);
  }

  if (!dispatch.has_value()) {
    registry_.Consume(command.id);
    pending_commands_.erase(command.id);
    return base::unexpected(CommandError::kDispatchFailure);
  }
  return base::ok(CommandStatus::kAwaitingObservation);
}

ExpectedObservation CommandExecutor::BuildObservation(
    const BrowserCommand& command) const {
  ExpectedObservation obs;
  obs.command_id = command.id;
  obs.kind = command.kind;
  obs.window = command.window;
  obs.tab = command.tab;
  obs.split_pane_b = command.split_pane_b;
  obs.created_at = base::TimeTicks::Now();
  obs.destination_index = command.destination_index;
  switch (command.kind) {
    case CommandKind::kOpenTemporaryTab:
    case CommandKind::kOpenRetainedTab:
      obs.expected_event = NormalizedEventType::kTabInserted;
      break;
    case CommandKind::kActivateTab:
      obs.expected_event = NormalizedEventType::kActiveTabChanged;
      break;
    case CommandKind::kCloseTab:
      obs.expected_event = NormalizedEventType::kTabRemoved;
      break;
    case CommandKind::kPinTab:
    case CommandKind::kUnpinTab:
      obs.expected_event = NormalizedEventType::kPinnedStateChanged;
      break;
    case CommandKind::kMoveTabWithinWindow:
      obs.expected_event = NormalizedEventType::kTabMoved;
      break;
    case CommandKind::kCreateSplit:
      obs.expected_event = NormalizedEventType::kSplitAdded;
      break;
    case CommandKind::kDissolveSplit:
      obs.expected_event = NormalizedEventType::kSplitRemoved;
      break;
    default:
      break;
  }
  return obs;
}

void CommandExecutor::OnNormalizedLifecycleEvent(const NormalizedEvent& event) {
  const base::TimeTicks now = base::TimeTicks::Now();
  std::vector<CommandId> expired;
  for (const auto& [id, obs] : registry_.entries()) {
    if (now - obs.created_at > ExpectedObservationRegistry::kWatchdogTimeout) {
      expired.push_back(id);
    }
  }
  for (const CommandId& id : expired) {
    registry_.Consume(id);
    auto cmd_it = pending_commands_.find(id);
    if (cmd_it != pending_commands_.end()) {
      outcome_unknown_commands_[id] = CommandStatus::kOutcomeUnknown;
      NotifyCommandCompleted(id, cmd_it->second,
                             CommandStatus::kOutcomeUnknown);
      pending_commands_.erase(cmd_it);
    }
  }

  ReconcileLateObservation(event);

  std::vector<CommandId> completed;
  std::vector<CommandId> ids;
  ids.reserve(registry_.in_flight_count());
  for (const auto& [id, obs] : registry_.entries()) {
    ids.push_back(id);
  }
  for (const CommandId& id : ids) {
    auto obs = registry_.Consume(id);
    if (!obs.has_value()) {
      continue;
    }
    if (obs->expected_event != event.type) {
      registry_.Register(*obs);
      continue;
    }
    if (obs->window.is_valid() && obs->window != event.window) {
      registry_.Register(*obs);
      continue;
    }
    if (obs->tab.is_valid() && event.tab.is_valid() && obs->tab != event.tab) {
      registry_.Register(*obs);
      continue;
    }
    auto cmd_it = pending_commands_.find(id);
    if (cmd_it == pending_commands_.end()) {
      continue;
    }
    CommandStatus final_status = CommandStatus::kApplied;
    if (!VerifyPostcondition(cmd_it->second)) {
      if (cmd_it->second.kind == CommandKind::kOpenRetainedTab &&
          event.tab.is_valid()) {
        final_status = CommandStatus::kAppliedWithOrganizationRepairRequired;
      } else {
        NotifyCommandCompleted(id, cmd_it->second, CommandStatus::kRejected);
        completed.push_back(id);
        continue;
      }
    }
    if (cmd_it->second.kind == CommandKind::kOpenRetainedTab &&
        event.tab.is_valid()) {
      const TabMembershipId membership =
          model_->FindMembershipIdByTabKey(event.tab.value());
      if (membership.is_valid()) {
        const MutationStatus retained = model_->RetainTab(membership);
        if (!retained.has_value()) {
          final_status = CommandStatus::kAppliedWithOrganizationRepairRequired;
        }
      } else {
        final_status = CommandStatus::kAppliedWithOrganizationRepairRequired;
      }
    }
    if (cmd_it->second.kind == CommandKind::kPinTab && event.tab.is_valid()) {
      const TabMembershipId membership =
          model_->FindMembershipIdByTabKey(event.tab.value());
      if (membership.is_valid()) {
        model_->PinTab(membership, cmd_it->second.saved_root_url);
      }
    }
    if (cmd_it->second.kind == CommandKind::kUnpinTab && event.tab.is_valid()) {
      const TabMembershipId membership =
          model_->FindMembershipIdByTabKey(event.tab.value());
      if (membership.is_valid()) {
        model_->UnpinTab(membership);
      }
    }
    completed.push_back(id);
    NotifyCommandCompleted(id, cmd_it->second, final_status);
  }
  for (const CommandId& id : completed) {
    pending_commands_.erase(id);
  }
}

void CommandExecutor::AddCompletionObserver(
    CommandCompletionObserver* observer) {
  completion_observers_.AddObserver(observer);
}

void CommandExecutor::RemoveCompletionObserver(
    CommandCompletionObserver* observer) {
  completion_observers_.RemoveObserver(observer);
}

void CommandExecutor::NotifyCommandCompleted(const CommandId& id,
                                             const BrowserCommand& command,
                                             CommandStatus status) {
  for (CommandCompletionObserver& observer : completion_observers_) {
    observer.OnCommandCompleted(id, command.kind, status);
  }
}

bool CommandExecutor::HasPendingPinCommandForTab(LiveTabKey tab) const {
  return registry_.HasPendingPinCommandForTab(tab);
}

bool CommandExecutor::ShouldDeferLifecyclePinRoleMutation(
    LiveTabKey tab) const {
  return registry_.HasPendingPinCommandForTab(tab);
}

void CommandExecutor::ReconcileLateObservation(const NormalizedEvent& event) {
  // Late events reconcile actual Chromium state through the lifecycle bridge.
  // Historical command outcomes remain kOutcomeUnknown; no automatic retry.
  (void)event;
}

void CommandExecutor::OnCloseCancelled(LiveTabKey tab) {
  for (auto it = pending_commands_.begin(); it != pending_commands_.end();) {
    if (it->second.kind == CommandKind::kCloseTab && it->second.tab == tab) {
      const BrowserCommand command = it->second;
      const CommandId id = it->first;
      registry_.Consume(id);
      it = pending_commands_.erase(it);
      NotifyCommandCompleted(id, command, CommandStatus::kCancelled);
    } else {
      ++it;
    }
  }
}

bool CommandExecutor::VerifyPostcondition(const BrowserCommand& command) const {
  if (!model_) {
    return false;
  }
  switch (command.kind) {
    case CommandKind::kOpenTemporaryTab:
    case CommandKind::kOpenRetainedTab:
      return true;
    case CommandKind::kActivateTab:
      return model_->FindMembershipIdByTabKey(command.tab.value()).is_valid();
    case CommandKind::kCloseTab:
      return !model_->FindMembershipIdByTabKey(command.tab.value()).is_valid();
    case CommandKind::kPinTab:
    case CommandKind::kUnpinTab: {
      const TabMembershipId id =
          model_->FindMembershipIdByTabKey(command.tab.value());
      const TabMembershipRecord* m =
          id.is_valid() ? model_->FindMembership(id) : nullptr;
      if (!m) {
        return false;
      }
      return command.kind == CommandKind::kPinTab
                 ? m->role == TabRole::kPinned
                 : m->role == TabRole::kRetained;
    }
    default:
      return true;
  }
}

void CommandExecutor::Shutdown() {
  shutting_down_ = true;
  for (const auto& [id, command] : pending_commands_) {
    outcome_unknown_commands_[id] = CommandStatus::kOutcomeUnknown;
  }
  pending_commands_.clear();
  registry_.Clear();
}

}  // namespace seoul
