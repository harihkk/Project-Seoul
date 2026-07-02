// Project Seoul Canvas - Mojo page handler (browser side).

#include "seoul/browser/canvas/seoul_canvas_page_handler.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "seoul/browser/product/browser/seoul_runtime_service.h"
#include "seoul/browser/product/browser/seoul_runtime_service_factory.h"

namespace seoul {

namespace {

// Bound on a component-event value payload accepted from the renderer.
constexpr size_t kMaxEventValueBytes = 64 * 1024;
// Bound on a conversational turn accepted from the renderer.
constexpr size_t kMaxTurnBytes = 8 * 1024;

ComponentEventKind FromMojo(canvas::mojom::ComponentEventKind kind) {
  switch (kind) {
    case canvas::mojom::ComponentEventKind::kActivate:
      return ComponentEventKind::kActivate;
    case canvas::mojom::ComponentEventKind::kValueChanged:
      return ComponentEventKind::kValueChanged;
    case canvas::mojom::ComponentEventKind::kSubmit:
      return ComponentEventKind::kSubmit;
    case canvas::mojom::ComponentEventKind::kSelect:
      return ComponentEventKind::kSelect;
    case canvas::mojom::ComponentEventKind::kDismiss:
      return ComponentEventKind::kDismiss;
  }
  return ComponentEventKind::kActivate;
}

}  // namespace

SeoulCanvasPageHandler::SeoulCanvasPageHandler(
    mojo::PendingReceiver<canvas::mojom::PageHandler> receiver,
    mojo::PendingRemote<canvas::mojom::Page> page,
    Profile* profile,
    BrowserWindowInterface* browser_window)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile) {
  runtime_ =
      profile_ ? SeoulRuntimeServiceFactory::GetForProfile(profile_) : nullptr;
  if (runtime_) {
    window_binding_token_ = runtime_->CreateWindowBinding(browser_window).token;
    runtime_->surfaces()->AddObserver(this);
    runtime_->tasks()->AddObserver(this);
    observing_ = true;
  }
}

SeoulCanvasPageHandler::~SeoulCanvasPageHandler() {
  if (runtime_ && !window_binding_token_.is_empty()) {
    runtime_->InvalidateWindowBinding(window_binding_token_);
  }
  if (observing_ && runtime_) {
    runtime_->surfaces()->RemoveObserver(this);
    runtime_->tasks()->RemoveObserver(this);
  }
}

void SeoulCanvasPageHandler::RequestInitialState() {
  if (!runtime_) {
    // Never a blank page: tell the renderer the runtime is unavailable so it
    // can show a real error state instead of nothing.
    PushStatus("runtime_unavailable");
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    PushStatus("window_unbound");
    return;
  }
  // Replay pinned surfaces so a reopened Canvas restores its dashboards.
  for (const SurfaceId& id : runtime_->surfaces()->PinnedSurfaces()) {
    if (std::optional<std::string> json = runtime_->surfaces()->SurfaceJson(id);
        json.has_value() && page_) {
      page_->PushSurface(id.value(), json.value());
    }
  }
  PushStatus("ready");
}

void SeoulCanvasPageHandler::NotifyComponentEvent(
    canvas::mojom::ComponentEventPtr event) {
  if (!event || !runtime_) {
    return;
  }
  if (event->value_json.size() > kMaxEventValueBytes) {
    return;  // oversized payloads never reach the parser
  }
  ComponentEvent typed;
  typed.surface_id = SurfaceId::FromString(event->surface_id);
  typed.component_id = event->component_id;
  typed.kind = FromMojo(event->kind);
  if (event->action_id.has_value()) {
    typed.action_id = event->action_id.value();
  }
  std::optional<base::Value> parsed_value =
      base::JSONReader::Read(event->value_json);
  if (parsed_value.has_value()) {
    typed.value = std::move(parsed_value.value());
  }

  // The browser decides: resolve the event against the surface's declared
  // actions, then route the typed outcome. The renderer's report is advisory.
  const SurfaceEventOutcome outcome =
      runtime_->surfaces()->HandleComponentEvent(typed);
  switch (outcome.kind) {
    case SurfaceEventOutcome::Kind::kRunCapability: {
      // A declared tool-call action runs exactly that capability with exactly
      // its declared payload - never re-inferred from text.
      const std::optional<LiveWindowKey> window = ResolveBoundWindow();
      if (window.has_value()) {
        runtime_->StartCapability(outcome.target, std::move(outcome.payload),
                                  window.value());
      } else {
        PushStatus("window_unbound");
      }
      break;
    }
    case SurfaceEventOutcome::Kind::kTaskApproval: {
      // target is "<task_id>:<step_id>"; split and approve.
      const size_t sep = outcome.target.find(':');
      if (sep != std::string::npos) {
        const TaskId task = TaskId::FromString(outcome.target.substr(0, sep));
        runtime_->tasks()->Approve(task, outcome.target.substr(sep + 1),
                                   /*approved=*/true);
      }
      break;
    }
    case SurfaceEventOutcome::Kind::kSubmitTurn: {
      const std::string* text = outcome.payload.FindString("text");
      if (text && !text->empty()) {
        StartBoundGoal(*text);
      }
      break;
    }
    case SurfaceEventOutcome::Kind::kNavigate: {
      // A navigate action opens its (already http(s)-validated) URL in the
      // bound window through the same validated capability path as any other
      // tab open - it never bypasses the command layer.
      const std::optional<LiveWindowKey> window = ResolveBoundWindow();
      if (window.has_value()) {
        base::Value::Dict args;
        args.Set("url", outcome.target);
        runtime_->StartCapability("browser.tabs.open", std::move(args),
                                  window.value());
      } else {
        PushStatus("window_unbound");
      }
      break;
    }
    case SurfaceEventOutcome::Kind::kBrowserCommand:
    case SurfaceEventOutcome::Kind::kWorkflowEdit:
      // These action kinds are only emitted by surfaces the runtime does not
      // render yet (the launcher and the workflow editor). Rather than fail
      // silently, tell the renderer so it can show an explicit "not available
      // yet" state; the interface compiler never emits them today.
      PushStatus("action_unsupported");
      break;
    case SurfaceEventOutcome::Kind::kNone:
      // Genuinely renderer-local (local-state toggles) or an unresolved event;
      // nothing for the browser to do.
      break;
  }
}

void SeoulCanvasPageHandler::SubmitTurn(canvas::mojom::TurnInputPtr input) {
  if (!input || input->text.empty() || input->text.size() > kMaxTurnBytes ||
      !runtime_) {
    return;
  }
  StartBoundGoal(input->text);
}

void SeoulCanvasPageHandler::StartVoice() {
  PushStatus("voice_requested");
}

void SeoulCanvasPageHandler::StopVoice() {
  PushStatus("voice_stopped");
}

void SeoulCanvasPageHandler::CancelActiveTask(const std::string& task_id) {
  if (runtime_) {
    runtime_->tasks()->Cancel(TaskId::FromString(task_id));
  }
}

void SeoulCanvasPageHandler::OnSurfaceUpdated(const SurfaceId& id,
                                              const std::string& surface_json) {
  if (page_) {
    page_->PushSurface(id.value(), surface_json);
  }
}

void SeoulCanvasPageHandler::OnSurfaceRemoved(const SurfaceId& id) {
  PushStatus("surface_removed");
}

void SeoulCanvasPageHandler::OnTaskUpdated(const TaskId& task_id) {
  PushStatus("task_updated");
}

void SeoulCanvasPageHandler::OnTaskNeedsApproval(const TaskId& task_id,
                                                 const std::string& step_id,
                                                 const std::string& prompt) {
  PushStatus("task_needs_approval");
}

void SeoulCanvasPageHandler::OnTaskFinished(const TaskId& task_id) {
  PushStatus("task_finished");
}

void SeoulCanvasPageHandler::PushStatus(const std::string& detail) {
  if (!page_) {
    return;
  }
  base::Value::Dict status;
  status.Set("detail", detail);
  if (runtime_) {
    const ProviderStateSnapshot providers = runtime_->providers()->Snapshot();
    status.Set("local_ready", providers.local_healthy);
    status.Set("cloud_ready", runtime_->providers()->cloud_available());
    status.Set("route", providers.local_healthy ? "local" : "cloud");
    status.Set("active_task_count",
               static_cast<int>(runtime_->tasks()->task_count()));
  } else {
    status.Set("local_ready", false);
    status.Set("cloud_ready", false);
  }
  std::string json;
  base::JSONWriter::Write(status, &json);
  page_->SetStatus(json);
}

std::optional<LiveWindowKey> SeoulCanvasPageHandler::ResolveBoundWindow()
    const {
  if (!runtime_ || window_binding_token_.is_empty()) {
    return std::nullopt;
  }
  return runtime_->ResolveWindowBinding(window_binding_token_);
}

TaskId SeoulCanvasPageHandler::StartBoundGoal(const std::string& goal) {
  if (!runtime_) {
    return TaskId();
  }
  std::optional<LiveWindowKey> window = ResolveBoundWindow();
  if (!window.has_value()) {
    PushStatus("window_unbound");
    return TaskId();
  }
  return runtime_->StartGoal(goal, window.value());
}

}  // namespace seoul
