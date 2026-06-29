// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_COMMAND_EXECUTOR_H_
#define SEOUL_BROWSER_COMMANDS_COMMAND_EXECUTOR_H_

#include <map>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/chromium_mutation_adapter.h"
#include "seoul/browser/commands/command_completion_observer.h"
#include "seoul/browser/commands/command_errors.h"
#include "seoul/browser/commands/expected_observation_registry.h"
#include "seoul/browser/commands/model_command_facade.h"
#include "seoul/browser/commands/target_resolver.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/organization/organization_model.h"

class Profile;

namespace seoul {

class LifecycleCoordinator;

class CommandExecutor {
 public:
  CommandExecutor(Profile* profile,
                  OrganizationModel* model,
                  LifecycleCoordinator* lifecycle,
                  TargetResolver* resolver,
                  ChromiumMutationAdapter* adapter);
  CommandExecutor(const CommandExecutor&) = delete;
  CommandExecutor& operator=(const CommandExecutor&) = delete;
  ~CommandExecutor();

  CommandResult<CommandStatus> Submit(BrowserCommand command);
  void OnNormalizedLifecycleEvent(const NormalizedEvent& event);
  void OnCloseCancelled(LiveTabKey tab);
  void Shutdown();
  bool HasPendingPinCommandForTab(LiveTabKey tab) const;
  bool ShouldDeferLifecyclePinRoleMutation(LiveTabKey tab) const;
  void ReconcileLateObservation(const NormalizedEvent& event);

  void AddCompletionObserver(CommandCompletionObserver* observer);
  void RemoveCompletionObserver(CommandCompletionObserver* observer);

  bool IsModelOnlyKind(CommandKind kind) const;
  size_t in_flight_count() const { return registry_.in_flight_count(); }

 private:
  CommandResult<CommandStatus> ValidateCommand(const BrowserCommand& command);
  CommandResult<CommandStatus> DispatchChromiumCommand(
      const BrowserCommand& command);
  CommandResult<CommandStatus> ConfirmObservation(
      const ExpectedObservation& observation,
      const NormalizedEvent& event);
  bool VerifyPostcondition(const BrowserCommand& command) const;
  ExpectedObservation BuildObservation(const BrowserCommand& command) const;

  raw_ptr<Profile> profile_;
  raw_ptr<OrganizationModel> model_;
  raw_ptr<LifecycleCoordinator> lifecycle_;
  raw_ptr<TargetResolver> resolver_;
  raw_ptr<ChromiumMutationAdapter> adapter_;
  ModelCommandFacade model_facade_;
  ExpectedObservationRegistry registry_;
  bool shutting_down_ = false;
  std::map<CommandId, BrowserCommand> pending_commands_;
  std::map<CommandId, CommandStatus> outcome_unknown_commands_;
  base::ObserverList<CommandCompletionObserver> completion_observers_;

  void NotifyCommandCompleted(const CommandId& id,
                              const BrowserCommand& command,
                              CommandStatus status);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_COMMAND_EXECUTOR_H_
