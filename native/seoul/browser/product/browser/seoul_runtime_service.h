// Project Seoul product runtime - the profile-scoped product owner.
// The single authoritative product runtime for a regular profile. It does NOT
// duplicate the organization service; it references it (through the factory
// DependsOn) for the model, lifecycle, command executor, projection, and
// shell, and adds the product layer on top: the capability graph and its
// executors, the connector/provider registries, the planner, task, surface,
// thread, and workflow services, the page agent, and the concrete network and
// credential transports.
//
// STATE OWNERSHIP
//   owner:        the profile (via SeoulRuntimeServiceFactory, a
//                 ProfileKeyedServiceFactory that DependsOn the organization
//                 service factory).
//   lifetime:     the regular original profile; excluded for OTR/guest/system.
//   persistence:  pinned surfaces, threads, workflows, and provider settings
//                 serialize to one bounded profile pref; secrets never do.
//   recovery:     product services rebuild from that pref on construction.
//   teardown:     Shutdown() tears down in reverse dependency order, removes
//                 observers before their subjects, and cancels in-progress
//                 work; no callback runs afterward.
//   isolation:    per profile; never process-global.

#ifndef SEOUL_BROWSER_PRODUCT_BROWSER_SEOUL_RUNTIME_SERVICE_H_
#define SEOUL_BROWSER_PRODUCT_BROWSER_SEOUL_RUNTIME_SERVICE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "components/keyed_service/core/keyed_service.h"
#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/product/capability_executor.h"
#include "seoul/browser/product/planner.h"
#include "seoul/browser/product/provider_registry.h"
#include "seoul/browser/product/surface_service.h"
#include "seoul/browser/product/task_service.h"
#include "seoul/browser/product/task_surface_bridge.h"
#include "seoul/browser/product/thread_service.h"
#include "seoul/browser/product/workflow_service.h"
#include "seoul/browser/runtime/seoul_runtime.h"
#include "seoul/browser/tools/tool_registry.h"

class PrefService;
class Profile;
class BrowserWindowInterface;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content {
class WebContents;
}

namespace seoul {

class SeoulOrganizationService;
class CredentialStore;
class HttpTransport;
class PageAgent;

// Single dict pref holding the bounded product state (pinned surfaces,
// threads, workflows, provider settings). Secrets are excluded by construction.
inline constexpr char kProductRuntimePref[] = "seoul.product.v1";

using WindowRuntimeBindingToken = base::UnguessableToken;

struct WindowRuntimeBinding {
  WindowRuntimeBindingToken token;
  LiveWindowKey window;

  bool is_valid() const { return !token.is_empty() && window.is_valid(); }
};

class SeoulRuntimeService : public KeyedService {
 public:
  // `organization` must be the same profile's organization service (the
  // factory guarantees it via DependsOn). `web_contents_resolver` maps a
  // live tab key to its WebContents for the page agent.
  SeoulRuntimeService(Profile* profile,
                      PrefService* prefs,
                      SeoulOrganizationService* organization,
                      WebContentsResolver web_contents_resolver);
  SeoulRuntimeService(const SeoulRuntimeService&) = delete;
  SeoulRuntimeService& operator=(const SeoulRuntimeService&) = delete;
  ~SeoulRuntimeService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Narrow accessors (never the whole mutable runtime).
  TaskService* tasks() { return task_service_.get(); }
  SurfaceService* surfaces() { return surface_service_.get(); }
  ThreadService* threads() { return thread_service_.get(); }
  WorkflowService* workflows() { return workflow_service_.get(); }
  TaskSurfaceBridge* task_surface_bridge() {
    return task_surface_bridge_.get();
  }
  ProviderRegistry* providers() { return provider_registry_.get(); }
  ToolRegistry& capabilities() { return runtime_.capabilities(); }
  PageAgent* page_agent() { return page_agent_.get(); }

  // The permission context for a user-initiated turn in `window`, built from
  // provider availability and the connected connector providers.
  ToolPermissionContext BuildPermissionContext() const;

  // Creates an opaque binding from a Canvas WebContents' embedding browser
  // window to the exact Seoul live window. A turn must present this token back
  // before it can act on browser state. Tokens are per profile, per window, and
  // invalidated when the browser window closes.
  WindowRuntimeBinding CreateWindowBinding(BrowserWindowInterface* browser);
  std::optional<LiveWindowKey> ResolveWindowBinding(
      const WindowRuntimeBindingToken& token) const;
  void InvalidateWindowBinding(const WindowRuntimeBindingToken& token);

  // Entry point for a text goal from Canvas/voice: plans and runs a task in
  // `window`, returning the task id. The final semantic result flows to the
  // surface service; the Canvas observes both services.
  TaskId StartGoal(const std::string& goal, const LiveWindowKey& window);

  // Runs one already-chosen capability with an explicit typed payload as a
  // single-step task. A surface action that declared a tool_call executes
  // exactly this capability with exactly its declared arguments; the payload
  // is never re-inferred from text. Returns an invalid id for an unknown
  // capability or window (StartTaskWithPlan re-validates against the schema
  // and permission context).
  TaskId StartCapability(const std::string& capability_id,
                         base::DictValue args,
                         const LiveWindowKey& window);

  // Credential writes go straight to the OS store; the value never returns to
  // any caller (the Canvas only learns presence via ProviderRegistry).
  bool SetCredential(const std::string& account_key, const std::string& secret);
  bool DeleteCredential(const std::string& account_key);

  // KeyedService:
  void Shutdown() override;

 private:
  void RegisterBuiltinExecutors();
  void OnWindowBindingClosed(WindowRuntimeBindingToken token,
                             BrowserWindowInterface* browser);
  void PersistState();
  void LoadState();

  struct WindowBindingRecord {
    raw_ptr<BrowserWindowInterface> browser = nullptr;
    LiveWindowKey window;
    std::optional<base::CallbackListSubscription> close_subscription;
  };

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<SeoulOrganizationService> organization_;

  // Chromium-facing transports and agent (owned).
  std::unique_ptr<HttpTransport> cloud_transport_;
  std::unique_ptr<HttpTransport> local_transport_;
  std::unique_ptr<CredentialStore> credentials_;
  std::unique_ptr<PageAgent> page_agent_;

  // The pure runtime composition (capability graph, connectors, scenes,
  // routing policy) - this is what instantiates SeoulRuntime.
  SeoulRuntime runtime_;

  // Product services (owned; pure product_core).
  CapabilityExecutorRegistry executors_;
  std::unique_ptr<ProviderRegistry> provider_registry_;
  std::unique_ptr<Planner> planner_;
  std::unique_ptr<TaskService> task_service_;
  std::unique_ptr<SurfaceService> surface_service_;
  // Projects verified task results into surfaces; observes task_service_ and
  // drives surface_service_, so it is constructed after both and destroyed
  // before either.
  std::unique_ptr<TaskSurfaceBridge> task_surface_bridge_;
  std::unique_ptr<ThreadService> thread_service_;
  std::unique_ptr<WorkflowService> workflow_service_;

  std::map<WindowRuntimeBindingToken, WindowBindingRecord> window_bindings_;
  bool shutting_down_ = false;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_BROWSER_SEOUL_RUNTIME_SERVICE_H_
