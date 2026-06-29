// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/expected_observation_registry.h"

namespace seoul {

ExpectedObservationRegistry::ExpectedObservationRegistry() = default;
ExpectedObservationRegistry::~ExpectedObservationRegistry() = default;

CommandStatusResult ExpectedObservationRegistry::Register(
    const ExpectedObservation& observation) {
  if (!observation.command_id.is_valid()) {
    return CommandErr(CommandError::kInvalidCommand);
  }
  if (entries_.size() >= kMaxInFlight) {
    return CommandErr(CommandError::kCapacityExceeded);
  }
  if (entries_.count(observation.command_id)) {
    return CommandErr(CommandError::kDuplicateCommandId);
  }
  if (HasConflict(observation.window, observation.tab)) {
    return CommandErr(CommandError::kConflictingCommand);
  }
  entries_.emplace(observation.command_id, observation);
  return CommandOk();
}

std::optional<ExpectedObservation> ExpectedObservationRegistry::Consume(
    const CommandId& id) {
  auto it = entries_.find(id);
  if (it == entries_.end()) {
    return std::nullopt;
  }
  ExpectedObservation result = it->second;
  entries_.erase(it);
  return result;
}

std::optional<ExpectedObservation> ExpectedObservationRegistry::FindByTarget(
    LiveWindowKey window,
    LiveTabKey tab) const {
  for (const auto& [id, obs] : entries_) {
    if (obs.window == window && obs.tab == tab) {
      return obs;
    }
  }
  return std::nullopt;
}

bool ExpectedObservationRegistry::HasConflict(LiveWindowKey window,
                                              LiveTabKey tab) const {
  if (!window.is_valid() && !tab.is_valid()) {
    return false;
  }
  for (const auto& [id, obs] : entries_) {
    if (window.is_valid() && obs.window == window) {
      if (!tab.is_valid() || obs.tab == tab) {
        return true;
      }
    }
    if (tab.is_valid() && obs.tab == tab) {
      return true;
    }
  }
  return false;
}

bool ExpectedObservationRegistry::HasPendingPinCommandForTab(
    LiveTabKey tab) const {
  if (!tab.is_valid()) {
    return false;
  }
  for (const auto& [id, obs] : entries_) {
    if (obs.tab == tab && (obs.kind == CommandKind::kPinTab ||
                           obs.kind == CommandKind::kUnpinTab)) {
      return true;
    }
  }
  return false;
}

void ExpectedObservationRegistry::Clear() {
  entries_.clear();
}

void ExpectedObservationRegistry::MarkOutcomeUnknown(const CommandId& id) {
  auto it = entries_.find(id);
  if (it != entries_.end()) {
    it->second.status = CommandStatus::kOutcomeUnknown;
    entries_.erase(it);
  }
}

void ExpectedObservationRegistry::ExpireWatchdogEntries(base::TimeTicks now) {
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (now - it->second.created_at > kWatchdogTimeout) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace seoul
