// Project Seoul product runtime - browser and page capability executors.

#include "seoul/browser/product/browser/browser_capabilities.h"

#include <utility>

#include "base/values.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_executor.h"
#include "seoul/browser/lifecycle/live_window_state.h"
#include "seoul/browser/organization/organization_model.h"
#include "url/gurl.h"

namespace seoul {

namespace {

void SetProvenance(SemanticResult* result, const std::string& source) {
  result->provenance.base.source_name = source;
  result->provenance.base.retrieved_at = base::Time::Now();
  result->provenance.base.effective_at = result->provenance.base.retrieved_at;
  result->provenance.base.freshness = FreshnessState::kRealTime;
  result->provenance.provider = "seoul";
}

FieldSpec Field(const std::string& id,
                FieldPrimitive primitive,
                SemanticRole role) {
  FieldSpec field;
  field.id = id;
  field.label = id;
  field.primitive = primitive;
  field.role = role;
  field.nullable = false;
  return field;
}

CapabilityOutcome FailOutcome(const std::string& summary) {
  CapabilityOutcome outcome;
  outcome.step.status = StepStatus::kFailed;
  outcome.step.observed_summary = summary;
  return outcome;
}

}  // namespace

// --- BrowserCommandExecutor -------------------------------------------------

BrowserCommandExecutor::BrowserCommandExecutor(const std::string& capability_id,
                                               CommandKind kind,
                                               CommandExecutor* commands)
    : id_(ToolId::FromString(capability_id)),
      kind_(kind),
      commands_(commands) {}

BrowserCommandExecutor::~BrowserCommandExecutor() {
  if (observing_ && commands_) {
    commands_->RemoveCompletionObserver(this);
  }
}

ToolId BrowserCommandExecutor::capability_id() const {
  return id_;
}

void BrowserCommandExecutor::Execute(CapabilityRequest request,
                                     CapabilityCallback callback) {
  if (!commands_) {
    std::move(callback).Run(FailOutcome("Command layer unavailable."));
    return;
  }
  BrowserCommand command;
  command.id = CommandId::Next();
  command.kind = kind_;
  command.window = request.window;

  switch (kind_) {
    case CommandKind::kOpenTemporaryTab:
    case CommandKind::kOpenRetainedTab: {
      const std::string* url = request.args.FindString("url");
      const GURL parsed(url ? *url : std::string());
      if (!parsed.is_valid() || !parsed.SchemeIsHTTPOrHTTPS()) {
        std::move(callback).Run(
            FailOutcome("A valid http(s) url is required."));
        return;
      }
      command.url = parsed;
      break;
    }
    case CommandKind::kActivateTab:
    case CommandKind::kCloseTab: {
      // Field name must match the capability descriptor's schema exactly
      // (generic_capabilities.cc declares "tab_key"); a mismatch makes the
      // capability unrunnable because ValidatePlan rejects unknown fields.
      const std::string* tab = request.args.FindString("tab_key");
      if (!tab || tab->empty()) {
        std::move(callback).Run(FailOutcome("A tab reference is required."));
        return;
      }
      command.tab = LiveTabKey::Parse(*tab);
      if (!command.tab.is_valid()) {
        std::move(callback).Run(FailOutcome("The tab reference is invalid."));
        return;
      }
      break;
    }
    case CommandKind::kSetActiveWorkspace: {
      const std::string* workspace = request.args.FindString("workspace_id");
      if (!workspace || workspace->empty()) {
        std::move(callback).Run(FailOutcome("A workspace id is required."));
        return;
      }
      command.workspace_id = WorkspaceId::FromString(*workspace);
      break;
    }
    case CommandKind::kOpenNewTab:
      break;  // no payload
    default:
      std::move(callback).Run(FailOutcome("Unsupported browser command."));
      return;
  }

  if (!observing_) {
    commands_->AddCompletionObserver(this);
    observing_ = true;
  }
  Pending pending;
  pending.callback = std::move(callback);
  pending.task_id = request.task_id;
  pending.step_id = request.step_id;
  const CommandId command_id = command.id;

  const CommandResult<CommandStatus> submitted =
      commands_->Submit(std::move(command));
  if (!submitted.has_value()) {
    std::move(pending.callback).Run(FailOutcome("The command was rejected."));
    return;
  }
  if (submitted.value() == CommandStatus::kApplied) {
    // Model-only fast path: already confirmed, no observation to await.
    CapabilityOutcome outcome;
    outcome.step.status = StepStatus::kSucceeded;
    outcome.step.observed_summary = "Applied.";
    outcome.step.verification.verified = true;
    outcome.step.verification.method = "postcondition";
    std::move(pending.callback).Run(std::move(outcome));
    return;
  }
  pending_[command_id] = std::move(pending);
}

void BrowserCommandExecutor::Cancel(const TaskId& task_id,
                                    const std::string& step_id) {
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second.task_id == task_id && it->second.step_id == step_id) {
      CapabilityOutcome outcome;
      outcome.step.status = StepStatus::kOutcomeUnknown;
      outcome.step.observed_summary = "Cancelled before confirmation.";
      std::move(it->second.callback).Run(std::move(outcome));
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
}

void BrowserCommandExecutor::OnCommandCompleted(CommandId id,
                                                CommandKind kind,
                                                CommandStatus status) {
  auto it = pending_.find(id);
  if (it == pending_.end()) {
    return;
  }
  Pending pending = std::move(it->second);
  pending_.erase(it);

  CapabilityOutcome outcome;
  switch (status) {
    case CommandStatus::kApplied:
    case CommandStatus::kAppliedWithOrganizationRepairRequired:
      outcome.step.status = StepStatus::kSucceeded;
      outcome.step.observed_summary = "Confirmed by the lifecycle bridge.";
      outcome.step.verification.verified = true;
      outcome.step.verification.method = "observation";
      break;
    case CommandStatus::kRejected:
      outcome.step.status = StepStatus::kFailed;
      outcome.step.observed_summary = "Rejected during dispatch.";
      break;
    case CommandStatus::kCancelled:
      outcome.step.status = StepStatus::kCancelled;
      outcome.step.observed_summary = "Cancelled.";
      break;
    case CommandStatus::kOutcomeUnknown:
      outcome.step.status = StepStatus::kOutcomeUnknown;
      outcome.step.observed_summary = "No confirmation observed.";
      break;
    default:
      outcome.step.status = StepStatus::kOutcomeUnknown;
      outcome.step.observed_summary = "Indeterminate command status.";
      break;
  }
  std::move(pending.callback).Run(std::move(outcome));
}

// --- EnumerateTabsExecutor --------------------------------------------------

EnumerateTabsExecutor::EnumerateTabsExecutor(
    OrganizationModel* model,
    LiveWindowStateProvider* live_state)
    : model_(model), live_state_(live_state) {}

EnumerateTabsExecutor::~EnumerateTabsExecutor() = default;

ToolId EnumerateTabsExecutor::capability_id() const {
  return ToolId::FromString("browser.tabs.enumerate");
}

void EnumerateTabsExecutor::Execute(CapabilityRequest request,
                                    CapabilityCallback callback) {
  CapabilityOutcome outcome;
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {
      Field("tab", FieldPrimitive::kString, SemanticRole::kIdentifier),
      Field("order", FieldPrimitive::kInteger, SemanticRole::kDimension),
      Field("pinned", FieldPrimitive::kBoolean, SemanticRole::kCategory),
      Field("active", FieldPrimitive::kBoolean, SemanticRole::kStatus)};

  base::ListValue rows;
  if (live_state_) {
    const std::optional<LiveWindowSnapshot> snapshot =
        live_state_->GetSnapshot(request.window);
    if (snapshot.has_value()) {
      for (const LiveTabDescriptor& tab : snapshot->tabs) {
        base::DictValue row;
        row.Set("tab", tab.tab.value());
        row.Set("order", tab.strip_order);
        row.Set("pinned", tab.chromium_pinned);
        row.Set("active", tab.is_active);
        rows.Append(std::move(row));
      }
    }
  }
  result.data = base::Value(std::move(rows));
  SetProvenance(&result, "browser.tabs.enumerate");

  outcome.step.status = StepStatus::kSucceeded;
  outcome.step.observed_summary = "Read the live tab strip.";
  outcome.step.verification.verified = true;
  outcome.step.verification.method = "observation";
  outcome.semantic = std::move(result);
  std::move(callback).Run(std::move(outcome));
}

// --- PageObserveExecutor ----------------------------------------------------

PageObserveExecutor::PageObserveExecutor(PageAgent* page_agent,
                                         LiveWindowStateProvider* live_state)
    : page_agent_(page_agent), live_state_(live_state) {}

PageObserveExecutor::~PageObserveExecutor() = default;

ToolId PageObserveExecutor::capability_id() const {
  return ToolId::FromString("page.observe.text");
}

void PageObserveExecutor::Execute(CapabilityRequest request,
                                  CapabilityCallback callback) {
  if (!page_agent_ || !live_state_) {
    std::move(callback).Run(FailOutcome("Page agent unavailable."));
    return;
  }
  const std::optional<LiveWindowSnapshot> snapshot =
      live_state_->GetSnapshot(request.window);
  if (!snapshot.has_value() || !snapshot->active_tab.is_valid()) {
    std::move(callback).Run(FailOutcome("No active tab to observe."));
    return;
  }
  page_agent_->Observe(
      snapshot->active_tab,
      base::BindOnce(&PageObserveExecutor::OnObserved,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void PageObserveExecutor::OnObserved(
    CapabilityCallback callback,
    std::optional<PageObservation> observation) {
  if (!observation.has_value()) {
    std::move(callback).Run(FailOutcome("The page could not be observed."));
    return;
  }
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {
      Field("handle", FieldPrimitive::kString, SemanticRole::kIdentifier),
      Field("role", FieldPrimitive::kString, SemanticRole::kCategory),
      Field("name", FieldPrimitive::kString, SemanticRole::kName),
      Field("value", FieldPrimitive::kString, SemanticRole::kDescription),
      Field("editable", FieldPrimitive::kBoolean, SemanticRole::kStatus)};
  base::ListValue rows;
  for (const PageObservation::Element& element : observation->elements) {
    base::DictValue row;
    row.Set("handle", element.handle);
    row.Set("role", element.role);
    row.Set("name", element.name);
    row.Set("value", element.value);
    row.Set("editable", element.editable);
    rows.Append(std::move(row));
  }
  result.data = base::Value(std::move(rows));
  SetProvenance(&result, "page.observe.text");
  result.provenance.base.source_url = observation->url;

  CapabilityOutcome outcome;
  outcome.step.status = StepStatus::kSucceeded;
  outcome.step.observed_summary = "Observed the active document.";
  outcome.step.verification.verified = true;
  outcome.step.verification.method = "observation";
  outcome.semantic = std::move(result);
  std::move(callback).Run(std::move(outcome));
}

// --- PageActionExecutor -----------------------------------------------------

PageActionExecutor::PageActionExecutor(const std::string& capability_id,
                                       PageActionKind action,
                                       PageAgent* page_agent,
                                       LiveWindowStateProvider* live_state)
    : id_(ToolId::FromString(capability_id)),
      action_(action),
      page_agent_(page_agent),
      live_state_(live_state) {}

PageActionExecutor::~PageActionExecutor() = default;

ToolId PageActionExecutor::capability_id() const {
  return id_;
}

void PageActionExecutor::Execute(CapabilityRequest request,
                                 CapabilityCallback callback) {
  if (!page_agent_ || !live_state_) {
    std::move(callback).Run(FailOutcome("Page agent unavailable."));
    return;
  }
  const std::optional<LiveWindowSnapshot> snapshot =
      live_state_->GetSnapshot(request.window);
  if (!snapshot.has_value() || !snapshot->active_tab.is_valid()) {
    std::move(callback).Run(FailOutcome("No active tab for the action."));
    return;
  }
  const std::string* handle = request.args.FindString("handle");
  if (!handle || handle->empty()) {
    std::move(callback).Run(FailOutcome("An element handle is required."));
    return;
  }
  PageActionRequest action;
  action.kind = action_;
  action.handle = *handle;
  if (const std::string* value = request.args.FindString("value")) {
    action.value = *value;
  }
  const PageActionStatus status =
      page_agent_->PerformAction(snapshot->active_tab, action);

  CapabilityOutcome outcome;
  if (status == PageActionStatus::kOk) {
    outcome.step.status = StepStatus::kSucceeded;
    outcome.step.observed_summary = "Performed the page action.";
    outcome.step.verification.verified = true;
    outcome.step.verification.method = "postcondition";
  } else if (status == PageActionStatus::kExpiredHandle) {
    // A stale handle is an invalid assumption, not a silent failure: replan.
    outcome.step.status = StepStatus::kFailed;
    outcome.step.observed_summary =
        "The element handle expired; re-observe the page first.";
  } else {
    outcome.step.status = StepStatus::kFailed;
    outcome.step.observed_summary = "The page action could not be performed.";
  }
  std::move(callback).Run(std::move(outcome));
}

}  // namespace seoul
