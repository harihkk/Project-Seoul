// Project Seoul product runtime - browser and page capability executors.
// Concrete CapabilityExecutors that drive the confirmed outbound command
// layer (CommandExecutor + mutation adapter) and the browser-owned page agent.
// Each executor owns one capability id: it decodes args, submits the typed
// browser command or page action, observes the real lifecycle confirmation,
// verifies the outcome, and reports a receipt. There is no central action
// switch; the runtime registers one executor per capability descriptor.

#ifndef SEOUL_BROWSER_PRODUCT_BROWSER_BROWSER_CAPABILITIES_H_
#define SEOUL_BROWSER_PRODUCT_BROWSER_BROWSER_CAPABILITIES_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "seoul/browser/commands/command_completion_observer.h"
#include "seoul/browser/commands/command_id.h"
#include "seoul/browser/commands/command_types.h"
#include "seoul/browser/product/browser/page_agent.h"
#include "seoul/browser/product/capability_executor.h"

namespace seoul {

class CommandExecutor;
class OrganizationModel;
class LiveWindowStateProvider;
class PreviewHostService;

// Executes a single browser-mutating capability by submitting a typed
// BrowserCommand and resolving when the lifecycle bridge confirms it.
class BrowserCommandExecutor : public CapabilityExecutor,
                               public CommandCompletionObserver {
 public:
  BrowserCommandExecutor(const std::string& capability_id,
                         CommandKind kind,
                         CommandExecutor* commands);
  ~BrowserCommandExecutor() override;

  // CapabilityExecutor:
  ToolId capability_id() const override;
  void Execute(CapabilityRequest request, CapabilityCallback callback) override;
  void Cancel(const TaskId& task_id, const std::string& step_id) override;

  // CommandCompletionObserver:
  void OnCommandCompleted(CommandId id,
                          CommandKind kind,
                          CommandStatus status) override;

 private:
  struct Pending {
    CapabilityCallback callback;
    TaskId task_id;
    std::string step_id;
  };

  const ToolId id_;
  const CommandKind kind_;
  raw_ptr<CommandExecutor> commands_;
  std::map<CommandId, Pending> pending_;
  bool observing_ = false;
};

// Reads the live tabs of the request window into an entity-collection
// SemanticResult. Read-only; resolves synchronously from the current snapshot.
class EnumerateTabsExecutor : public CapabilityExecutor {
 public:
  EnumerateTabsExecutor(OrganizationModel* model,
                        LiveWindowStateProvider* live_state);
  ~EnumerateTabsExecutor() override;

  ToolId capability_id() const override;
  void Execute(CapabilityRequest request, CapabilityCallback callback) override;

 private:
  raw_ptr<OrganizationModel> model_;
  raw_ptr<LiveWindowStateProvider> live_state_;
};

class PreviewOpenExecutor : public CapabilityExecutor {
 public:
  explicit PreviewOpenExecutor(PreviewHostService* preview_host);
  ~PreviewOpenExecutor() override;

  ToolId capability_id() const override;
  void Execute(CapabilityRequest request, CapabilityCallback callback) override;

 private:
  raw_ptr<PreviewHostService> preview_host_;
};

// Observes the active tab of the request window through the page agent and
// returns a record SemanticResult of visible semantic elements.
class PageObserveExecutor : public CapabilityExecutor {
 public:
  PageObserveExecutor(PageAgent* page_agent,
                      LiveWindowStateProvider* live_state);
  ~PageObserveExecutor() override;

  ToolId capability_id() const override;
  void Execute(CapabilityRequest request, CapabilityCallback callback) override;

 private:
  void OnObserved(CapabilityCallback callback,
                  std::optional<PageObservation> observation);

  raw_ptr<PageAgent> page_agent_;
  raw_ptr<LiveWindowStateProvider> live_state_;
  base::WeakPtrFactory<PageObserveExecutor> weak_factory_{this};
};

// Performs one typed page action (click/type) on the active tab via the page
// agent. The action targets an element handle from a prior observation.
class PageActionExecutor : public CapabilityExecutor {
 public:
  PageActionExecutor(const std::string& capability_id,
                     PageActionKind action,
                     PageAgent* page_agent,
                     LiveWindowStateProvider* live_state);
  ~PageActionExecutor() override;

  ToolId capability_id() const override;
  void Execute(CapabilityRequest request, CapabilityCallback callback) override;

 private:
  const ToolId id_;
  const PageActionKind action_;
  raw_ptr<PageAgent> page_agent_;
  raw_ptr<LiveWindowStateProvider> live_state_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_BROWSER_BROWSER_CAPABILITIES_H_
