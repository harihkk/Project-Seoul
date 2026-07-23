// Project Seoul product runtime - the profile-scoped product owner.

#include "seoul/browser/product/browser/seoul_runtime_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "seoul/browser/commands/command_executor.h"
#include "seoul/browser/connectors/connector.h"
#include "seoul/browser/connectors/connector_registry.h"
#include "seoul/browser/library/library_service.h"
#include "seoul/browser/lifecycle/persistence_scheduler.h"
#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/organization/seoul_organization_service.h"
#include "seoul/browser/product/browser/browser_capabilities.h"
#include "seoul/browser/product/browser/keychain_credential_store.h"
#include "seoul/browser/product/browser/network_http_transport.h"
#include "seoul/browser/product/browser/page_agent.h"
#include "seoul/browser/preview/preview_host_service.h"
#include "seoul/browser/scenes/scene_registry.h"
#include "seoul/browser/shell/shell_service.h"
#include "seoul/browser/site_layers/site_layer_registry.h"
#include "seoul/browser/themes/theme_registry.h"
#include "url/gurl.h"

namespace seoul {

namespace {

base::Time Now() {
  return base::Time::Now();
}

// Builds the scene resolvers over the organization model plus runtime-owned
// catalogs. Every reference resolves against its authoritative owner so a
// Scene cannot retain a phantom workspace, Theme, or Site Layer.
SceneResolvers MakeSceneResolvers(SeoulOrganizationService* organization,
                                  ThemeRegistry* themes,
                                  SiteLayerRegistry* site_layers) {
  SceneResolvers resolvers;
  resolvers.workspace_exists = base::BindRepeating(
      [](SeoulOrganizationService* org, const std::string& workspace_id) {
        return org && org->model().FindWorkspace(
                          WorkspaceId::FromString(workspace_id)) != nullptr;
      },
      organization);
  resolvers.theme_exists = base::BindRepeating(
      [](ThemeRegistry* registry, const std::string& theme_id) {
        return registry && registry->Exists(theme_id);
      },
      themes);
  resolvers.site_layer_exists = base::BindRepeating(
      [](SiteLayerRegistry* registry, const std::string& site_layer_id) {
        return registry && registry->Exists(site_layer_id);
      },
      site_layers);
  return resolvers;
}

}  // namespace

SeoulRuntimeService::SeoulRuntimeService(
    Profile* profile,
    PrefService* prefs,
    SeoulOrganizationService* organization,
    WebContentsResolver web_contents_resolver)
    : profile_(profile),
      prefs_(prefs),
      organization_(organization),
      web_contents_resolver_(web_contents_resolver),
      themes_(std::make_unique<ThemeRegistry>()),
      site_layers_(std::make_unique<SiteLayerRegistry>()),
      library_service_(std::make_unique<LibraryService>(
          base::BindRepeating(&Now),
          base::BindRepeating(&SeoulRuntimeService::SchedulePersist,
                              base::Unretained(this)))),
      runtime_(MakeSceneResolvers(organization, themes_.get(),
                                  site_layers_.get())) {
  // Concrete transports: the general one for cloud/connectors, a
  // loopback-only one for the local reasoning provider.
  scoped_refptr<network::SharedURLLoaderFactory> factory =
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();
  cloud_transport_ = std::make_unique<NetworkHttpTransport>(
      factory, NetworkHttpTransport::Mode::kGeneral);
  local_transport_ = std::make_unique<NetworkHttpTransport>(
      factory, NetworkHttpTransport::Mode::kLoopbackOnly);
  credentials_ = std::make_unique<KeychainCredentialStore>(
      profile_->GetBaseName().MaybeAsASCII());
  page_agent_ = std::make_unique<PageAgent>(web_contents_resolver_);

  runtime_.RegisterBuiltinCapabilities();

  provider_registry_ = std::make_unique<ProviderRegistry>(
      local_transport_.get(), cloud_transport_.get(), credentials_.get());
  preview_manager_ = std::make_unique<PreviewManager>(
      base::BindRepeating(&Now),
      base::BindRepeating(&PreviewId::GenerateNew));
  preview_host_service_ = std::make_unique<PreviewHostService>(
      profile_, preview_manager_.get(),
      organization_ ? organization_->lifecycle_coordinator() : nullptr);
  realtime_voice_agent_ = std::make_unique<RealtimeVoiceAgent>(
      cloud_transport_.get(), credentials_.get());
  planner_ = std::make_unique<Planner>(runtime_.capabilities(),
                                       provider_registry_->MakePlanRequester());
  agent_permissions_ =
      std::make_unique<AgentPermissionService>(base::BindRepeating(&Now));
  if (organization_) {
    if (LiveWindowStateProvider* live_state =
            organization_->live_window_state_provider()) {
      for (const LiveWindowKey& window : live_state->Windows()) {
        if (std::optional<LiveWindowSnapshot> snapshot =
                live_state->GetSnapshot(window)) {
          OnLiveWindowSnapshotChanged(*snapshot);
        }
      }
      live_window_observation_.Observe(live_state);
    }
  }
  task_service_ =
      std::make_unique<TaskService>(&runtime_.capabilities(), &executors_,
                                    planner_.get(), base::BindRepeating(&Now),
                                    agent_permissions_.get(),
                                    base::BindRepeating(
                                        &SeoulRuntimeService::
                                            ResolveAgentPermissionRequest,
                                        base::Unretained(this)));
  task_service_->AddObserver(this);
  voice_controller_ = std::make_unique<VoiceRuntimeController>(
      task_service_.get(), speech_to_text_.get(), text_to_speech_.get(),
      base::BindRepeating(&SeoulRuntimeService::StartGoal,
                          base::Unretained(this)),
      base::BindRepeating(&Now));
  surface_service_ = std::make_unique<SurfaceService>();
  // The bridge is the production path that turns a verified task result into a
  // Canvas surface; without it, tasks would complete with no artifact.
  task_surface_bridge_ = std::make_unique<TaskSurfaceBridge>(
      task_service_.get(), surface_service_.get());
  thread_service_ = std::make_unique<ThreadService>(base::BindRepeating(&Now));
  workflow_service_ = std::make_unique<WorkflowService>(
      task_service_.get(), base::BindRepeating(&Now));

  RegisterBuiltinExecutors();
  LoadState();
  persistence_scheduler_ = std::make_unique<PersistenceScheduler>(
      base::BindRepeating(&SeoulRuntimeService::PersistState,
                          base::Unretained(this)),
      base::SequencedTaskRunner::GetCurrentDefault());
}

SeoulRuntimeService::~SeoulRuntimeService() = default;

void SeoulRuntimeService::OnLiveWindowSnapshotChanged(
    const LiveWindowSnapshot& snapshot) {
  if (shutting_down_ || !snapshot.window.is_valid()) {
    return;
  }
  std::set<LiveTabKey> current_tabs;
  for (const LiveTabDescriptor& descriptor : snapshot.tabs) {
    if (descriptor.tab.is_valid()) {
      current_tabs.insert(descriptor.tab);
    }
  }

  auto [it, inserted] =
      live_tabs_by_window_.try_emplace(snapshot.window, current_tabs);
  if (inserted) {
    return;
  }
  for (const LiveTabKey& previous : it->second) {
    if (!current_tabs.contains(previous)) {
      if (agent_permissions_) {
        agent_permissions_->RevokeTab(previous);
      }
      if (preview_host_service_) {
        preview_host_service_->DismissForParent(previous);
      }
    }
  }
  it->second = std::move(current_tabs);
}

void SeoulRuntimeService::OnLiveWindowRemoved(LiveWindowKey window) {
  live_tabs_by_window_.erase(window);
  if (preview_host_service_) {
    preview_host_service_->DismissForWindow(window);
  }
  if (!shutting_down_ && agent_permissions_) {
    agent_permissions_->RevokeWindow(window);
  }
}

void SeoulRuntimeService::OnTaskUpdated(const TaskId& task_id) {
  if (!task_service_ || shutting_down_) {
    return;
  }
  if (std::optional<TaskSnapshot> snapshot = task_service_->Snapshot(task_id)) {
    PublishShellTaskSummary(snapshot->window);
  }
}

void SeoulRuntimeService::OnTaskFinished(const TaskId& task_id) {
  OnTaskUpdated(task_id);
}

void SeoulRuntimeService::PublishShellTaskSummary(
    const LiveWindowKey& window) {
  if (!window.is_valid() || !organization_ || !task_service_) {
    return;
  }
  ShellService* shell = organization_->shell_service();
  if (!shell) {
    return;
  }
  ShellTaskSummary summary;
  for (const TaskStateSummary& task : task_service_->StateSummaries()) {
    if (task.window != window) {
      continue;
    }
    ++summary.total;
    switch (task.state) {
      case TaskState::kDraft:
      case TaskState::kPlanning:
      case TaskState::kExecuting:
      case TaskState::kMonitoring:
        ++summary.active;
        break;
      case TaskState::kAwaitingApproval:
        ++summary.waiting_for_user;
        break;
      case TaskState::kPaused:
        ++summary.paused;
        break;
      case TaskState::kFailed:
        ++summary.failed;
        break;
      case TaskState::kCompleted:
      case TaskState::kCancelled:
        break;
    }
  }
  shell->UpdateTaskSummary(window, summary);
}

// static
void SeoulRuntimeService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kProductRuntimePref);
}

void SeoulRuntimeService::RegisterBuiltinExecutors() {
  CommandExecutor* commands =
      organization_ ? organization_->command_executor() : nullptr;
  OrganizationModel* model = organization_ ? &organization_->model() : nullptr;
  LiveWindowStateProvider* live_state =
      organization_ ? organization_->live_window_state_provider() : nullptr;

  // Browser-mutating capabilities: one executor per descriptor id, each
  // mapping to a typed browser command observed to completion.
  executors_.Register(std::make_unique<BrowserCommandExecutor>(
      "browser.tabs.open", CommandKind::kOpenTemporaryTab, commands));
  executors_.Register(std::make_unique<BrowserCommandExecutor>(
      "browser.tabs.activate", CommandKind::kActivateTab, commands));
  executors_.Register(std::make_unique<BrowserCommandExecutor>(
      "browser.tabs.close", CommandKind::kCloseTab, commands));
  executors_.Register(std::make_unique<BrowserCommandExecutor>(
      "browser.workspace.switch", CommandKind::kSetActiveWorkspace, commands));

  // Browser surfaces and read-only browser/page capabilities.
  executors_.Register(
      std::make_unique<EnumerateTabsExecutor>(model, live_state));
  executors_.Register(
      std::make_unique<PreviewOpenExecutor>(preview_host_service_.get()));
  executors_.Register(
      std::make_unique<PageObserveExecutor>(page_agent_.get(), live_state));
  executors_.Register(std::make_unique<PageActionExecutor>(
      "page.act.click", PageActionKind::kClick, page_agent_.get(), live_state));
  executors_.Register(std::make_unique<PageActionExecutor>(
      "page.act.type", PageActionKind::kType, page_agent_.get(), live_state));

  // Any registered capability descriptor that has no executor yet is marked
  // unavailable so the planner never offers a capability that cannot run.
  // (Provider-backed descriptors like info.search.web become available when a
  // connector supplies them.)
  const ToolPermissionContext everything = [] {
    ToolPermissionContext context;
    context.max_sensitivity = DataSensitivity::kCredentialAdjacent;
    context.allow_network = true;
    return context;
  }();
  for (const ToolDescriptor* descriptor :
       runtime_.capabilities().ListAvailable(everything)) {
    if (!executors_.Find(descriptor->id, descriptor->version)) {
      runtime_.capabilities().SetAvailability(
          descriptor->id, AvailabilityState::kUnavailable,
          "No executor registered for this capability yet.");
    }
  }
}

ToolPermissionContext SeoulRuntimeService::BuildPermissionContext() const {
  ToolPermissionContext context;
  // Organization-level reads are always allowed; page content is allowed
  // because the page agent is browser-owned and returns only semantics.
  context.max_sensitivity = DataSensitivity::kPageContent;
  context.allow_network =
      provider_registry_ && provider_registry_->HasUsableProvider();
  // Connector providers add themselves here as they connect.
  for (const Connector* connector : runtime_.connectors().Connected()) {
    context.connected_providers.insert(connector->provider());
  }
  return context;
}

AgentPermissionRequest SeoulRuntimeService::ResolveAgentPermissionRequest(
    const LiveWindowKey& window,
    const ToolDescriptor& descriptor,
    const base::DictValue& args) const {
  AgentPermissionRequest request;
  request.capability = descriptor.id;
  request.approval = descriptor.approval;
  request.risk = descriptor.risk;
  request.sensitivity = descriptor.sensitivity;
  request.window = window;
  if (descriptor.provider != "seoul") {
    request.service_scope = descriptor.provider;
  }

  if (const std::string* explicit_tab = args.FindString("tab_key")) {
    request.tab = LiveTabKey::Parse(*explicit_tab);
  }
  if (descriptor.id.root_namespace() == "page") {
    if (!request.tab.is_valid() && organization_) {
      if (LiveWindowStateProvider* live_state =
              organization_->live_window_state_provider()) {
        if (std::optional<LiveWindowSnapshot> snapshot =
                live_state->GetSnapshot(window)) {
          request.tab = snapshot->active_tab;
        }
      }
    }
    request.frame_scope = "main";
  } else if (descriptor.id.value() == "browser.preview.open") {
    request.frame_scope = "main";
  }
  if (request.tab.is_valid() && web_contents_resolver_) {
    if (content::WebContents* contents =
            web_contents_resolver_.Run(request.tab)) {
      request.source_origin =
          url::Origin::Create(contents->GetLastCommittedURL());
    }
  }

  if (const std::string* destination = args.FindString("url")) {
    const GURL url(*destination);
    if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
      request.destination_origin = url::Origin::Create(url);
    }
  }
  return request;
}

WindowRuntimeBinding SeoulRuntimeService::CreateWindowBinding(
    BrowserWindowInterface* browser) {
  if (shutting_down_ || !browser || browser->GetProfile() != profile_ ||
      browser->IsDeleteScheduled() ||
      browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return {};
  }
  const SessionID& session_id = browser->GetSessionID();
  if (!session_id.is_valid()) {
    return {};
  }

  WindowRuntimeBinding binding;
  binding.token = WindowRuntimeBindingToken::Create();
  binding.window = LiveWindowKey::FromSessionId(session_id.id());

  WindowBindingRecord record;
  record.browser = browser;
  record.window = binding.window;
  record.close_subscription = browser->RegisterBrowserDidClose(
      base::BindRepeating(&SeoulRuntimeService::OnWindowBindingClosed,
                          base::Unretained(this), binding.token));
  window_bindings_.emplace(binding.token, std::move(record));
  return binding;
}

std::optional<LiveWindowKey> SeoulRuntimeService::ResolveWindowBinding(
    const WindowRuntimeBindingToken& token) const {
  if (shutting_down_ || token.is_empty()) {
    return std::nullopt;
  }
  auto it = window_bindings_.find(token);
  if (it == window_bindings_.end()) {
    return std::nullopt;
  }
  BrowserWindowInterface* browser = it->second.browser.get();
  if (!browser || browser->GetProfile() != profile_ ||
      browser->IsDeleteScheduled() ||
      browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return std::nullopt;
  }
  const SessionID& session_id = browser->GetSessionID();
  if (!session_id.is_valid()) {
    return std::nullopt;
  }
  const LiveWindowKey current = LiveWindowKey::FromSessionId(session_id.id());
  if (current != it->second.window ||
      BrowserWindowInterface::FromSessionID(session_id) != browser) {
    return std::nullopt;
  }
  return current;
}

void SeoulRuntimeService::InvalidateWindowBinding(
    const WindowRuntimeBindingToken& token) {
  if (!token.is_empty()) {
    window_bindings_.erase(token);
  }
}

void SeoulRuntimeService::OnWindowBindingClosed(
    WindowRuntimeBindingToken token,
    BrowserWindowInterface* browser) {
  auto it = window_bindings_.find(token);
  if (it == window_bindings_.end() || it->second.browser != browser) {
    return;
  }
  if (agent_permissions_) {
    agent_permissions_->RevokeWindow(it->second.window);
  }
  window_bindings_.erase(it);
}

TaskId SeoulRuntimeService::StartCapability(const std::string& capability_id,
                                            base::DictValue args,
                                            const LiveWindowKey& window) {
  if (shutting_down_ || !task_service_ || !window.is_valid()) {
    return TaskId();
  }
  const ToolId id = ToolId::FromString(capability_id);
  if (!id.is_valid() || !runtime_.capabilities().Find(id)) {
    return TaskId();
  }
  Plan plan;
  plan.goal = capability_id;
  PlanStep step;
  step.id = "step_1";
  step.kind = PlanStepKind::kToolCall;
  step.tool = id;
  step.args = std::move(args);
  plan.steps.push_back(std::move(step));
  // Approval policy is enforced from the descriptor, never from the surface.
  Planner::EnforceApprovalPolicy(plan, runtime_.capabilities());
  return task_service_->StartTaskWithPlan(capability_id, std::move(plan),
                                          PlanOrigin::kDeterministic, window,
                                          BuildPermissionContext());
}

TaskId SeoulRuntimeService::StartGoal(const std::string& goal,
                                      const LiveWindowKey& window) {
  if (shutting_down_ || !task_service_ || !window.is_valid()) {
    return TaskId();
  }
  const bool use_model =
      provider_registry_ && provider_registry_->HasUsableProvider();
  const bool prefer_local = false;  // capability-first: best available model
  return task_service_->StartTask(goal, window, BuildPermissionContext(),
                                  use_model, prefer_local);
}

VoiceStatusResult SeoulRuntimeService::StartVoice(const LiveWindowKey& window) {
  if (shutting_down_ || !voice_controller_) {
    return VoiceErr(VoiceError::kProviderUnavailable);
  }
  return voice_controller_->StartVoice(window);
}

VoiceStatusResult SeoulRuntimeService::StopVoice() {
  if (shutting_down_ || !voice_controller_) {
    return VoiceErr(VoiceError::kProviderUnavailable);
  }
  return voice_controller_->StopVoice();
}

VoiceRuntimeSnapshot SeoulRuntimeService::VoiceSnapshot() const {
  return voice_controller_ ? voice_controller_->Snapshot()
                           : VoiceRuntimeSnapshot();
}

void SeoulRuntimeService::CreateRealtimeVoiceSession(
    const std::string& safety_identifier,
    RealtimeVoiceAgent::CreateSessionCallback callback) {
  if (shutting_down_ || !realtime_voice_agent_) {
    std::move(callback).Run(
        base::unexpected("Realtime voice agent is unavailable."));
    return;
  }
  realtime_voice_agent_->CreateSession(safety_identifier, std::move(callback));
}

RealtimeVoiceAgentSnapshot SeoulRuntimeService::RealtimeVoiceSnapshot() const {
  return realtime_voice_agent_ ? realtime_voice_agent_->Snapshot()
                               : RealtimeVoiceAgentSnapshot();
}

bool SeoulRuntimeService::SetCredential(const std::string& account_key,
                                        const std::string& secret) {
  return credentials_ && credentials_->Set(account_key, secret);
}

bool SeoulRuntimeService::DeleteCredential(const std::string& account_key) {
  return credentials_ && credentials_->Delete(account_key);
}

bool SeoulRuntimeService::PersistState() {
  if (!prefs_ || shutting_down_) {
    return false;
  }
  base::DictValue state;
  if (surface_service_) {
    state.Set("surfaces", surface_service_->TakePersistedState());
  }
  if (thread_service_) {
    state.Set("threads", thread_service_->TakePersistedState());
  }
  if (workflow_service_) {
    state.Set("workflows", workflow_service_->TakePersistedState());
  }
  if (provider_registry_) {
    state.Set("providers", provider_registry_->TakePersistedState());
  }
  if (library_service_) {
    state.Set("library", library_service_->TakePersistedState());
  }
  if (themes_) {
    state.Set("themes", themes_->TakePersistedState());
  }
  if (site_layers_) {
    state.Set("site_layers", site_layers_->TakePersistedState());
  }
  state.Set("scenes", runtime_.scenes().TakePersistedState());
  prefs_->SetDict(kProductRuntimePref, std::move(state));
  return true;
}

void SeoulRuntimeService::SchedulePersist() {
  if (persistence_scheduler_ && !shutting_down_) {
    persistence_scheduler_->ScheduleWrite();
  }
}

void SeoulRuntimeService::LoadState() {
  if (!prefs_) {
    return;
  }
  const base::DictValue& state = prefs_->GetDict(kProductRuntimePref);
  if (const base::DictValue* surfaces = state.FindDict("surfaces")) {
    surface_service_->RestorePersistedState(*surfaces);
  }
  if (const base::DictValue* threads = state.FindDict("threads")) {
    thread_service_->RestorePersistedState(*threads);
  }
  if (const base::DictValue* workflows = state.FindDict("workflows")) {
    workflow_service_->RestorePersistedState(*workflows);
  }
  if (const base::DictValue* providers = state.FindDict("providers")) {
    provider_registry_->RestorePersistedState(*providers);
  }
  if (const base::DictValue* library = state.FindDict("library")) {
    library_service_->RestorePersistedState(*library);
  }
  if (const base::DictValue* themes = state.FindDict("themes")) {
    themes_->RestorePersistedState(*themes);
  }
  if (const base::DictValue* site_layers = state.FindDict("site_layers")) {
    site_layers_->RestorePersistedState(*site_layers);
  }
  // Scenes load after all referenced catalogs so invalid/removed references
  // are rejected during restore rather than retained as latent failures.
  if (const base::DictValue* scenes = state.FindDict("scenes")) {
    runtime_.scenes().RestorePersistedState(*scenes);
  }
}

void SeoulRuntimeService::Shutdown() {
  // Persist durable product state before anything is torn down.
  // PersistState intentionally refuses writes after shutdown begins, so the
  // ordering here is a correctness invariant.
  bool persisted = false;
  if (persistence_scheduler_) {
    persistence_scheduler_->Shutdown();
    persisted = persistence_scheduler_->Flush();
  }
  // Flush is a no-op when no Library mutation is pending. The explicit write
  // still captures changes owned by the other runtime services; when Flush did
  // write, avoid issuing the same preference write twice.
  if (!persisted) {
    PersistState();
  }
  shutting_down_ = true;
  live_window_observation_.Reset();
  live_tabs_by_window_.clear();
  window_bindings_.clear();
  // Reverse dependency order: tasks depend on providers/executors; workflows
  // depend on tasks; the runtime registries are torn down last.
  voice_controller_.reset();
  if (task_service_) {
    task_service_->RemoveObserver(this);
    if (organization_) {
      if (ShellService* shell = organization_->shell_service()) {
        shell->ClearTaskSummaries();
      }
    }
    task_service_->Shutdown();
  }
  if (agent_permissions_) {
    agent_permissions_->RevokeAll();
  }
  // The bridge observes the task service and drives the surface service; drop
  // it before either so no notification arrives after teardown.
  task_surface_bridge_.reset();
  workflow_service_.reset();
  thread_service_.reset();
  surface_service_.reset();
  library_service_.reset();
  persistence_scheduler_.reset();
  task_service_.reset();
  if (preview_host_service_) {
    preview_host_service_->Shutdown();
    preview_host_service_.reset();
  }
  preview_manager_.reset();
  agent_permissions_.reset();
  text_to_speech_.reset();
  speech_to_text_.reset();
  if (realtime_voice_agent_) {
    realtime_voice_agent_->Cancel();
    realtime_voice_agent_.reset();
  }
  planner_.reset();
  if (provider_registry_) {
    provider_registry_->Shutdown();
    provider_registry_.reset();
  }
  page_agent_.reset();
  runtime_.Shutdown();
  credentials_.reset();
  local_transport_.reset();
  cloud_transport_.reset();
}


SeoulRuntimeService::WindowBindingRecord::WindowBindingRecord() = default;
SeoulRuntimeService::WindowBindingRecord::WindowBindingRecord(WindowBindingRecord&&) = default;
SeoulRuntimeService::WindowBindingRecord& SeoulRuntimeService::WindowBindingRecord::operator=(WindowBindingRecord&&) = default;
SeoulRuntimeService::WindowBindingRecord::~WindowBindingRecord() = default;

}  // namespace seoul
