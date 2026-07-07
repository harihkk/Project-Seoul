// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_EXPECTED_OBSERVATION_REGISTRY_H_
#define SEOUL_BROWSER_COMMANDS_EXPECTED_OBSERVATION_REGISTRY_H_

#include <map>
#include <optional>

#include "base/time/time.h"
#include "seoul/browser/commands/command_errors.h"
#include "seoul/browser/commands/expected_observation.h"

namespace seoul {

class ExpectedObservationRegistry {
 public:
  static constexpr size_t kMaxInFlight = 64;
  static constexpr base::TimeDelta kWatchdogTimeout = base::Seconds(30);

  ExpectedObservationRegistry();
  ~ExpectedObservationRegistry();

  CommandStatusResult Register(const ExpectedObservation& observation);
  std::optional<ExpectedObservation> Consume(const CommandId& id);
  std::optional<ExpectedObservation> FindByTarget(LiveWindowKey window,
                                                  LiveTabKey tab) const;
  bool HasConflict(LiveWindowKey window, LiveTabKey tab) const;
  bool HasPendingPinCommandForTab(LiveTabKey tab) const;
  void Clear();
  void MarkOutcomeUnknown(const CommandId& id);
  void ExpireWatchdogEntries(base::TimeTicks now);
  size_t in_flight_count() const { return entries_.size(); }
  const std::map<CommandId, ExpectedObservation>& entries() const {
    return entries_;
  }

 private:
  std::map<CommandId, ExpectedObservation> entries_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_EXPECTED_OBSERVATION_REGISTRY_H_
