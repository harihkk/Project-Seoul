// Project Seoul Canvas - Mojo page handler (browser side).

#include "seoul/browser/canvas/seoul_canvas_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/uuid.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "seoul/browser/library/library_service.h"
#include "seoul/browser/product/browser/seoul_runtime_service.h"
#include "seoul/browser/product/browser/seoul_runtime_service_factory.h"
#include "seoul/browser/product/task_snapshot_wire.h"
#include "seoul/browser/product/task_surface_bridge.h"
#include "seoul/browser/product/workflow_service.h"
#include "seoul/browser/scenes/scene_registry.h"
#include "seoul/browser/site_layers/site_layer_compiler.h"
#include "seoul/browser/site_layers/site_layer_registry.h"
#include "seoul/browser/themes/theme_registry.h"

namespace seoul {

namespace {

// Bound on a component-event value payload accepted from the renderer.
constexpr size_t kMaxEventValueBytes = 64 * 1024;
// Bound on a conversational turn accepted from the renderer.
constexpr size_t kMaxTurnBytes = 8 * 1024;
constexpr size_t kMaxTaskInputBytes = 2 * 1024;
constexpr size_t kMaxProviderEndpointBytes = 2048;
constexpr size_t kMaxProviderModelBytes = 512;
constexpr size_t kMaxProviderSecretBytes = 64 * 1024;
constexpr size_t kMaxSiteLayerTextValueBytes = 512;

std::string WriteJson(base::DictValue value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

std::string ErrorJson(const std::string& detail) {
  base::DictValue value;
  value.Set("status", "error");
  value.Set("detail", detail);
  return WriteJson(std::move(value));
}

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

std::optional<SiteAdjustmentKind> SiteAdjustmentKindFromWire(
    const std::string& kind) {
  if (kind == "accent_color") {
    return SiteAdjustmentKind::kAccentColor;
  }
  if (kind == "background_color") {
    return SiteAdjustmentKind::kBackgroundColor;
  }
  if (kind == "text_color") {
    return SiteAdjustmentKind::kTextColor;
  }
  if (kind == "font_family") {
    return SiteAdjustmentKind::kFontFamily;
  }
  if (kind == "font_size_scale") {
    return SiteAdjustmentKind::kFontSizeScale;
  }
  if (kind == "content_width") {
    return SiteAdjustmentKind::kContentWidth;
  }
  if (kind == "line_spacing") {
    return SiteAdjustmentKind::kLineSpacing;
  }
  if (kind == "density") {
    return SiteAdjustmentKind::kDensity;
  }
  if (kind == "hide") {
    return SiteAdjustmentKind::kHide;
  }
  if (kind == "emphasize") {
    return SiteAdjustmentKind::kEmphasize;
  }
  if (kind == "sticky_header_off") {
    return SiteAdjustmentKind::kStickyHeaderOff;
  }
  if (kind == "reading_mode") {
    return SiteAdjustmentKind::kReadingMode;
  }
  if (kind == "increase_contrast") {
    return SiteAdjustmentKind::kIncreaseContrast;
  }
  if (kind == "reduce_motion") {
    return SiteAdjustmentKind::kReduceMotion;
  }
  return std::nullopt;
}

std::optional<DensityLevel> DensityFromWire(const std::string& density) {
  if (density == "compact") {
    return DensityLevel::kCompact;
  }
  if (density == "comfortable" || density.empty()) {
    return DensityLevel::kComfortable;
  }
  if (density == "spacious") {
    return DensityLevel::kSpacious;
  }
  return std::nullopt;
}

std::optional<SiteAdjustment> SiteAdjustmentFromMojo(
    const canvas::mojom::SiteLayerAdjustmentInputPtr& input) {
  if (!input || input->text_value.size() > kMaxSiteLayerTextValueBytes ||
      input->selectors.size() > kMaxSelectorsPerRule) {
    return std::nullopt;
  }
  const std::optional<SiteAdjustmentKind> kind =
      SiteAdjustmentKindFromWire(input->kind);
  const std::optional<DensityLevel> density =
      DensityFromWire(input->density);
  if (!kind.has_value() || !density.has_value()) {
    return std::nullopt;
  }
  SiteAdjustment adjustment;
  adjustment.kind = kind.value();
  adjustment.selectors = input->selectors;
  adjustment.color_value = input->text_value;
  adjustment.font_family = input->text_value;
  adjustment.numeric_value = input->numeric_value;
  adjustment.density = density.value();
  return adjustment;
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
    if (LiveWindowStateProvider* live_state =
            runtime_->live_window_state_provider()) {
      live_window_observation_.Observe(live_state);
    }
    observing_ = true;
  }
}

SeoulCanvasPageHandler::~SeoulCanvasPageHandler() {
  if (runtime_ && !window_binding_token_.is_empty()) {
    runtime_->InvalidateWindowBinding(window_binding_token_);
  }
  live_window_observation_.Reset();
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
  PushPageContext();
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
      base::JSONReader::Read(event->value_json, base::JSON_PARSE_RFC);
  if (parsed_value.has_value()) {
    typed.value = std::move(parsed_value.value());
  }

  // The browser decides: resolve the event against the surface's declared
  // actions, then route the typed outcome. The renderer's report is advisory.
  SurfaceEventOutcome outcome =
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
        base::DictValue args;
        args.Set("url", outcome.target);
        runtime_->StartCapability("browser.tabs.open", std::move(args),
                                  window.value());
      } else {
        PushStatus("window_unbound");
      }
      break;
    }
    case SurfaceEventOutcome::Kind::kBrowserCommand: {
      // A launcher-catalog command is a registered browser.* capability; the
      // registry fail-closes unknown ids and the normal approval, budget, and
      // receipt machinery applies. Nothing here bypasses the command layer.
      const std::optional<LiveWindowKey> window = ResolveBoundWindow();
      if (!window.has_value()) {
        PushStatus("window_unbound");
        break;
      }
      if (ToolId::FromString(outcome.target).is_valid()) {
        runtime_->StartCapability(outcome.target, std::move(outcome.payload),
                                  window.value());
      } else {
        PushStatus("browser_command_rejected");
      }
      break;
    }
    case SurfaceEventOutcome::Kind::kWorkflowEdit: {
      // Typed workflow edits: the payload names the workflow and operation;
      // the target is the node id the action was declared on. Every edit
      // revalidates atomically in the workflow service; a rejected edit is
      // reported, never swallowed.
      if (!ApplyWorkflowEdit(outcome.target, outcome.payload)) {
        PushStatus("workflow_edit_rejected");
      }
      break;
    }
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
  if (!runtime_) {
    PushStatus("runtime_unavailable");
    return;
  }
  std::optional<LiveWindowKey> window = ResolveBoundWindow();
  if (!window.has_value()) {
    PushStatus("window_unbound");
    return;
  }
  const VoiceStatusResult result = runtime_->StartVoice(window.value());
  PushStatus(result.has_value() ? "voice_started"
                                : VoiceErrorToString(result.error()));
}

void SeoulCanvasPageHandler::StopVoice() {
  if (!runtime_) {
    PushStatus("runtime_unavailable");
    return;
  }
  const VoiceStatusResult result = runtime_->StopVoice();
  PushStatus(result.has_value() ? "voice_stopped"
                                : VoiceErrorToString(result.error()));
}

void SeoulCanvasPageHandler::CreateRealtimeVoiceSession(
    CreateRealtimeVoiceSessionCallback callback) {
  if (!runtime_) {
    std::move(callback).Run(ErrorJson("runtime_unavailable"));
    return;
  }
  std::optional<LiveWindowKey> window = ResolveBoundWindow();
  if (!window.has_value()) {
    PushStatus("window_unbound");
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  runtime_->CreateRealtimeVoiceSession(
      window->value(),
      base::BindOnce(&SeoulCanvasPageHandler::OnRealtimeVoiceSessionCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  PushStatus("realtime_voice_session_requested");
}

void SeoulCanvasPageHandler::SubmitRealtimeToolCall(
    canvas::mojom::RealtimeToolCallPtr call,
    SubmitRealtimeToolCallCallback callback) {
  if (!call || !runtime_) {
    std::move(callback).Run(ErrorJson("runtime_unavailable"));
    return;
  }
  if (call->name != kSeoulRealtimeVoiceToolName ||
      call->arguments_json.size() > kMaxEventValueBytes) {
    std::move(callback).Run(ErrorJson("realtime_tool_rejected"));
    return;
  }
  std::optional<base::Value> parsed =
      base::JSONReader::Read(call->arguments_json, base::JSON_PARSE_RFC);
  if (!parsed.has_value() || !parsed->is_dict()) {
    std::move(callback).Run(ErrorJson("malformed_realtime_tool_args"));
    return;
  }
  const std::string* goal = parsed->GetDict().FindString("goal");
  if (!goal || goal->empty() || goal->size() > kMaxTurnBytes) {
    std::move(callback).Run(ErrorJson("invalid_realtime_goal"));
    return;
  }

  const TaskId task_id = StartBoundGoal(*goal);
  if (!task_id.is_valid()) {
    std::move(callback).Run(ErrorJson("task_rejected"));
    return;
  }

  base::DictValue output;
  output.Set("status", "accepted");
  output.Set("task_id", task_id.value());
  output.Set("goal", *goal);
  output.Set("message",
             "Seoul accepted the browser task. Canvas will show task state, "
             "approval prompts, and visual results as they are produced.");
  PushStatus("realtime_tool_started");
  std::move(callback).Run(WriteJson(std::move(output)));
}

void SeoulCanvasPageHandler::ListTasks(ListTasksCallback callback) {
  std::vector<std::string> snapshots_json;
  const std::optional<LiveWindowKey> window = ResolveBoundWindow();
  if (runtime_ && window.has_value()) {
    for (const TaskSnapshot& snapshot : runtime_->tasks()->Snapshots()) {
      if (!(snapshot.window == window.value())) {
        continue;  // other windows' tasks never leak into this Canvas
      }
      std::string json;
      base::JSONWriter::Write(TaskSnapshotToValue(snapshot), &json);
      snapshots_json.push_back(std::move(json));
    }
  }
  std::move(callback).Run(std::move(snapshots_json));
}

void SeoulCanvasPageHandler::PauseTask(const std::string& task_id) {
  if (BoundTask(task_id).has_value()) {
    runtime_->tasks()->Pause(TaskId::FromString(task_id));
  }
}

void SeoulCanvasPageHandler::ResumeTask(const std::string& task_id) {
  if (BoundTask(task_id).has_value()) {
    runtime_->tasks()->Resume(TaskId::FromString(task_id));
  }
}

void SeoulCanvasPageHandler::CancelActiveTask(const std::string& task_id) {
  if (BoundTask(task_id).has_value()) {
    runtime_->tasks()->Cancel(TaskId::FromString(task_id));
  }
}

void SeoulCanvasPageHandler::ApproveStep(const std::string& task_id,
                                         const std::string& step_id,
                                         bool approved) {
  if (BoundTask(task_id).has_value()) {
    runtime_->tasks()->Approve(TaskId::FromString(task_id), step_id, approved);
  }
}

void SeoulCanvasPageHandler::ProvideTaskInput(const std::string& task_id,
                                              const std::string& step_id,
                                              const std::string& input) {
  if (input.empty() || input.size() > kMaxTaskInputBytes ||
      !BoundTask(task_id).has_value()) {
    return;
  }
  base::DictValue typed_input;
  typed_input.Set("text", input);
  if (!runtime_->tasks()->ProvideInput(TaskId::FromString(task_id), step_id,
                                       std::move(typed_input))) {
    PushStatus("task_input_rejected");
  }
}

void SeoulCanvasPageHandler::ListTaskSurfaces(
    const std::string& task_id,
    ListTaskSurfacesCallback callback) {
  std::vector<std::string> surface_ids;
  if (BoundTask(task_id).has_value() && runtime_->task_surface_bridge()) {
    if (const SurfaceId* id = runtime_->task_surface_bridge()->SurfaceForTask(
            TaskId::FromString(task_id))) {
      surface_ids.push_back(id->value());
    }
  }
  std::move(callback).Run(std::move(surface_ids));
}

void SeoulCanvasPageHandler::SaveTaskAsWorkflow(
    const std::string& task_id,
    const std::string& name,
    SaveTaskAsWorkflowCallback callback) {
  std::string workflow_id;
  if (BoundTask(task_id).has_value() && runtime_->workflows() &&
      !name.empty()) {
    if (std::optional<WorkflowId> id =
            runtime_->workflows()->SaveTaskAsWorkflow(
                TaskId::FromString(task_id), name)) {
      workflow_id = id->value();
    }
  }
  std::move(callback).Run(std::move(workflow_id));
}

std::string SeoulCanvasPageHandler::LibrarySnapshotJson() const {
  if (!runtime_ || !runtime_->library()) {
    return ErrorJson("library_unavailable");
  }
  if (!ResolveBoundWindow().has_value()) {
    return ErrorJson("window_unbound");
  }
  return WriteJson(runtime_->library()->TakePersistedState());
}

void SeoulCanvasPageHandler::GetLibrarySnapshot(
    GetLibrarySnapshotCallback callback) {
  std::move(callback).Run(LibrarySnapshotJson());
}

void SeoulCanvasPageHandler::CreateBoard(const std::string& name,
                                         CreateBoardCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const LibraryResult<BoardId> result = runtime_->library()->CreateBoard(name);
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

void SeoulCanvasPageHandler::RenameBoard(const std::string& board_id,
                                         const std::string& name,
                                         RenameBoardCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const LibraryStatusResult result = runtime_->library()->RenameBoard(
      BoardId::FromString(board_id), name);
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

void SeoulCanvasPageHandler::SetBoardArchived(
    const std::string& board_id,
    bool archived,
    SetBoardArchivedCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const LibraryStatusResult result = runtime_->library()->SetBoardArchived(
      BoardId::FromString(board_id), archived);
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

void SeoulCanvasPageHandler::DeleteBoard(const std::string& board_id,
                                         DeleteBoardCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const LibraryStatusResult result =
      runtime_->library()->DeleteBoard(BoardId::FromString(board_id));
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

namespace {

std::optional<BoardElementKind> BoardElementKindFromWire(
    const std::string& kind) {
  if (kind == "text") {
    return BoardElementKind::kText;
  }
  if (kind == "link") {
    return BoardElementKind::kLink;
  }
  if (kind == "image_reference") {
    return BoardElementKind::kImageReference;
  }
  if (kind == "capture_reference") {
    return BoardElementKind::kCaptureReference;
  }
  if (kind == "surface_reference") {
    return BoardElementKind::kSurfaceReference;
  }
  return std::nullopt;
}

std::optional<BoardElement> BoardElementFromWire(
    const std::string& element_id,
    const std::string& kind,
    const std::string& title,
    const std::string& text,
    const std::string& reference,
    const std::string& origin,
    double x,
    double y,
    double width,
    double height,
    int32_t z_index) {
  const std::optional<BoardElementKind> parsed_kind =
      BoardElementKindFromWire(kind);
  if (!parsed_kind.has_value()) {
    return std::nullopt;
  }
  BoardElement element;
  if (!element_id.empty()) {
    element.id = BoardElementId::FromString(element_id);
    if (!element.id.is_valid()) {
      return std::nullopt;
    }
  }
  element.kind = *parsed_kind;
  element.title = title;
  element.text = text;
  element.reference = reference;
  element.origin = origin;
  element.x = x;
  element.y = y;
  element.width = width;
  element.height = height;
  element.z_index = z_index;
  return element;
}

}  // namespace

void SeoulCanvasPageHandler::AddBoardElement(
    const std::string& board_id,
    const std::string& element_id,
    const std::string& kind,
    const std::string& title,
    const std::string& text,
    const std::string& reference,
    const std::string& origin,
    double x,
    double y,
    double width,
    double height,
    int32_t z_index,
    AddBoardElementCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  std::optional<BoardElement> element = BoardElementFromWire(
      element_id, kind, title, text, reference, origin, x, y, width, height,
      z_index);
  if (!element.has_value()) {
    std::move(callback).Run(ErrorJson("invalid_element"));
    return;
  }
  const LibraryResult<BoardElementId> result =
      runtime_->library()->AddBoardElement(BoardId::FromString(board_id),
                                           std::move(*element));
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

void SeoulCanvasPageHandler::UpdateBoardElement(
    const std::string& board_id,
    const std::string& element_id,
    const std::string& kind,
    const std::string& title,
    const std::string& text,
    const std::string& reference,
    const std::string& origin,
    double x,
    double y,
    double width,
    double height,
    int32_t z_index,
    UpdateBoardElementCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  std::optional<BoardElement> element = BoardElementFromWire(
      element_id, kind, title, text, reference, origin, x, y, width, height,
      z_index);
  if (!element.has_value()) {
    std::move(callback).Run(ErrorJson("invalid_element"));
    return;
  }
  const LibraryStatusResult result = runtime_->library()->UpdateBoardElement(
      BoardId::FromString(board_id), std::move(*element));
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

void SeoulCanvasPageHandler::RemoveBoardElement(
    const std::string& board_id,
    const std::string& element_id,
    RemoveBoardElementCallback callback) {
  if (!runtime_ || !runtime_->library()) {
    std::move(callback).Run(ErrorJson("library_unavailable"));
    return;
  }
  if (!ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const LibraryStatusResult result = runtime_->library()->RemoveBoardElement(
      BoardId::FromString(board_id),
      BoardElementId::FromString(element_id));
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(LibraryErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(LibrarySnapshotJson());
}

std::string SeoulCanvasPageHandler::SiteLayerSnapshotJson() const {
  if (!runtime_ || !runtime_->site_layers()) {
    return ErrorJson("site_layers_unavailable");
  }
  const std::optional<LiveWindowKey> window = ResolveBoundWindow();
  if (!window.has_value()) {
    return ErrorJson("window_unbound");
  }

  base::DictValue active_page;
  std::optional<LiveTabDescriptor> active =
      runtime_->ActiveTabDescriptor(window.value());
  if (active.has_value()) {
    active_page.Set("tab_id", active->tab.value());
    active_page.Set("title", active->title);
    active_page.Set("origin", active->origin);
    active_page.Set("customizable",
                    IsValidOriginPattern(active->origin));
  } else {
    active_page.Set("tab_id", "");
    active_page.Set("title", "");
    active_page.Set("origin", "");
    active_page.Set("customizable", false);
  }

  base::ListValue layers;
  int matching_enabled_count = 0;
  for (const SiteLayer* layer : runtime_->site_layers()->List()) {
    base::DictValue value = SiteLayerToValue(*layer);
    const bool matches_active =
        active.has_value() && !active->origin.empty() &&
        SiteLayerMatchesOrigin(*layer, active->origin);
    value.Set("matches_active_page", matches_active);
    if (matches_active && layer->enabled && layer->scene_scope.empty()) {
      ++matching_enabled_count;
    }
    layers.Append(std::move(value));
  }

  base::DictValue snapshot;
  snapshot.Set("status", "ready");
  snapshot.Set("schema_version", 1);
  snapshot.Set("active_page", std::move(active_page));
  snapshot.Set("matching_enabled_count", matching_enabled_count);
  snapshot.Set("layers", std::move(layers));
  return WriteJson(std::move(snapshot));
}

void SeoulCanvasPageHandler::GetSiteLayerSnapshot(
    GetSiteLayerSnapshotCallback callback) {
  std::move(callback).Run(SiteLayerSnapshotJson());
}

void SeoulCanvasPageHandler::UpsertSiteLayer(
    const std::string& layer_id,
    const std::string& name,
    const std::string& origin_pattern,
    const std::string& scene_scope,
    bool enabled,
    std::vector<canvas::mojom::SiteLayerAdjustmentInputPtr> adjustments,
    UpsertSiteLayerCallback callback) {
  if (!runtime_ || !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  if (adjustments.empty() || adjustments.size() > kMaxLayerRules) {
    std::move(callback).Run(ErrorJson("invalid_adjustments"));
    return;
  }

  SiteLayer layer;
  layer.id = layer_id.empty()
                 ? "boost-" +
                       base::Uuid::GenerateRandomV4().AsLowercaseString()
                 : layer_id;
  layer.name = name;
  layer.origin_pattern = origin_pattern;
  layer.scene_scope = scene_scope;
  layer.enabled = enabled;
  layer.adjustments.reserve(adjustments.size());
  for (const canvas::mojom::SiteLayerAdjustmentInputPtr& input :
       adjustments) {
    std::optional<SiteAdjustment> adjustment =
        SiteAdjustmentFromMojo(input);
    if (!adjustment.has_value()) {
      std::move(callback).Run(ErrorJson("invalid_adjustment"));
      return;
    }
    layer.adjustments.push_back(std::move(adjustment.value()));
  }

  const SiteLayerStatusResult result =
      runtime_->UpsertSiteLayer(std::move(layer));
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(SiteLayerErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(SiteLayerSnapshotJson());
}

void SeoulCanvasPageHandler::SetSiteLayerEnabled(
    const std::string& layer_id,
    bool enabled,
    SetSiteLayerEnabledCallback callback) {
  if (!runtime_ || !runtime_->site_layers() ||
      !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const SiteLayer* stored = runtime_->site_layers()->Find(layer_id);
  if (!stored) {
    std::move(callback).Run(ErrorJson("unknown_layer"));
    return;
  }
  SiteLayer updated = *stored;
  updated.enabled = enabled;
  const SiteLayerStatusResult result =
      runtime_->UpsertSiteLayer(std::move(updated));
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(SiteLayerErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(SiteLayerSnapshotJson());
}

void SeoulCanvasPageHandler::DeleteSiteLayer(
    const std::string& layer_id,
    DeleteSiteLayerCallback callback) {
  if (!runtime_ || !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  const SiteLayerStatusResult result =
      runtime_->RemoveSiteLayer(layer_id);
  if (!result.has_value()) {
    std::move(callback).Run(ErrorJson(SiteLayerErrorToString(result.error())));
    return;
  }
  std::move(callback).Run(SiteLayerSnapshotJson());
}

std::string SeoulCanvasPageHandler::StudioSnapshotJson() const {
  if (!runtime_ || !runtime_->providers() || !runtime_->scenes() ||
      !runtime_->themes() || !runtime_->site_layers()) {
    return ErrorJson("studio_unavailable");
  }
  if (!ResolveBoundWindow().has_value()) {
    return ErrorJson("window_unbound");
  }

  const ProviderStateSnapshot providers = runtime_->providers()->Snapshot();
  base::DictValue local;
  local.Set("configured", providers.local_configured);
  local.Set("healthy", providers.local_healthy);
  local.Set("model_configured", !providers.local_model.empty());
  local.Set("model", providers.local_model);
  local.Set("discovered_model_count",
            static_cast<int>(providers.local_models_discovered.size()));
  base::DictValue cloud;
  cloud.Set("configured", providers.cloud_configured);
  cloud.Set("enabled", providers.cloud_enabled);
  cloud.Set("available", runtime_->providers()->cloud_available());
  cloud.Set("model_configured", !providers.cloud_model.empty());
  cloud.Set("model", providers.cloud_model);
  cloud.Set("voice_configured",
            runtime_->RealtimeVoiceSnapshot().configured);

  base::DictValue provider_routes;
  provider_routes.Set("local", std::move(local));
  provider_routes.Set("cloud", std::move(cloud));

  base::ListValue scenes;
  for (const SceneDefinition* scene : runtime_->scenes()->List()) {
    base::DictValue item;
    item.Set("id", scene->id);
    item.Set("name", scene->name);
    item.Set("workspace_id", scene->workspace_id);
    item.Set("theme_id", scene->theme_id);
    item.Set("site_layer_count",
             static_cast<int>(scene->site_layer_ids.size()));
    item.Set("prefer_compact", scene->prefer_compact);
    scenes.Append(std::move(item));
  }

  base::ListValue site_layers;
  for (const SiteLayer* layer : runtime_->site_layers()->List()) {
    base::DictValue item;
    item.Set("id", layer->id);
    item.Set("name", layer->name);
    item.Set("origin_pattern", layer->origin_pattern);
    item.Set("scene_scope", layer->scene_scope);
    item.Set("enabled", layer->enabled);
    item.Set("adjustment_count",
             static_cast<int>(layer->adjustments.size()));
    site_layers.Append(std::move(item));
  }

  base::ListValue themes;
  for (const Theme* theme : runtime_->themes()->List()) {
    base::DictValue item;
    item.Set("id", theme->id);
    item.Set("name", theme->name);
    switch (theme->scheme) {
      case ColorScheme::kLight:
        item.Set("scheme", "light");
        break;
      case ColorScheme::kDark:
        item.Set("scheme", "dark");
        break;
      case ColorScheme::kSystem:
        item.Set("scheme", "system");
        break;
    }
    item.Set("background", ColorToHex(theme->colors.background));
    item.Set("surface", ColorToHex(theme->colors.surface));
    item.Set("accent", ColorToHex(theme->colors.accent));
    item.Set("corner_radius_px", theme->corner_radius_px);
    item.Set("reduced_motion", theme->motion.reduced_motion);
    item.Set("reduced_transparency", theme->motion.reduced_transparency);
    themes.Append(std::move(item));
  }

  base::DictValue snapshot;
  snapshot.Set("schema_version", 1);
  snapshot.Set("providers", std::move(provider_routes));
  snapshot.Set("scenes", std::move(scenes));
  snapshot.Set("themes", std::move(themes));
  snapshot.Set("site_layers", std::move(site_layers));
  return WriteJson(std::move(snapshot));
}

void SeoulCanvasPageHandler::GetStudioSnapshot(
    GetStudioSnapshotCallback callback) {
  std::move(callback).Run(StudioSnapshotJson());
}

void SeoulCanvasPageHandler::SaveLocalProvider(
    const std::string& endpoint_url,
    const std::string& model_id,
    SaveLocalProviderCallback callback) {
  if (!runtime_ || !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  if (endpoint_url.empty() ||
      endpoint_url.size() > kMaxProviderEndpointBytes || model_id.empty() ||
      model_id.size() > kMaxProviderModelBytes ||
      !runtime_->ConfigureLocalProvider(endpoint_url, model_id)) {
    std::move(callback).Run(ErrorJson("invalid_local_provider"));
    return;
  }
  std::move(callback).Run(StudioSnapshotJson());
}

void SeoulCanvasPageHandler::ClearLocalProvider(
    ClearLocalProviderCallback callback) {
  if (!runtime_ || !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  runtime_->ClearLocalProvider();
  std::move(callback).Run(StudioSnapshotJson());
}

void SeoulCanvasPageHandler::CheckLocalProvider(
    CheckLocalProviderCallback callback) {
  if (!runtime_ || !runtime_->providers() ||
      !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  runtime_->providers()->CheckLocalHealth(base::BindOnce(
      &SeoulCanvasPageHandler::OnLocalProviderChecked,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SeoulCanvasPageHandler::OnLocalProviderChecked(
    CheckLocalProviderCallback callback,
    bool /*healthy*/) {
  std::move(callback).Run(StudioSnapshotJson());
}

void SeoulCanvasPageHandler::SaveCloudProvider(
    const std::string& model_id,
    bool enabled,
    const std::string& reasoning_secret,
    const std::string& voice_secret,
    SaveCloudProviderCallback callback) {
  if (!runtime_ || !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  if (model_id.empty() || model_id.size() > kMaxProviderModelBytes ||
      reasoning_secret.size() > kMaxProviderSecretBytes ||
      voice_secret.size() > kMaxProviderSecretBytes ||
      !runtime_->ConfigureCloudProvider(model_id, enabled, reasoning_secret,
                                        voice_secret)) {
    std::move(callback).Run(ErrorJson("cloud_provider_save_failed"));
    return;
  }
  std::move(callback).Run(StudioSnapshotJson());
}

void SeoulCanvasPageHandler::ClearCloudProvider(
    ClearCloudProviderCallback callback) {
  if (!runtime_ || !ResolveBoundWindow().has_value()) {
    std::move(callback).Run(ErrorJson("window_unbound"));
    return;
  }
  if (!runtime_->ClearCloudProviderAndCredentials()) {
    std::move(callback).Run(ErrorJson("credential_store_unavailable"));
    return;
  }
  std::move(callback).Run(StudioSnapshotJson());
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
  PushTaskSnapshot(task_id);
}

void SeoulCanvasPageHandler::OnTaskNeedsApproval(const TaskId& task_id,
                                                 const std::string& step_id,
                                                 const std::string& prompt) {
  PushTaskSnapshot(task_id);
}

void SeoulCanvasPageHandler::OnTaskFinished(const TaskId& task_id) {
  PushTaskSnapshot(task_id);
}

void SeoulCanvasPageHandler::OnLiveWindowSnapshotChanged(
    const LiveWindowSnapshot& snapshot) {
  const std::optional<LiveWindowKey> bound = ResolveBoundWindow();
  if (bound.has_value() && snapshot.window == bound.value()) {
    PushPageContext();
  }
}

void SeoulCanvasPageHandler::OnLiveWindowRemoved(LiveWindowKey window) {
  const std::optional<LiveWindowKey> bound = ResolveBoundWindow();
  if (!bound.has_value() || window == bound.value()) {
    PushPageContext();
  }
}

void SeoulCanvasPageHandler::PushTaskSnapshot(const TaskId& task_id) {
  if (!page_) {
    return;
  }
  const std::optional<TaskSnapshot> snapshot = BoundTask(task_id.value());
  if (!snapshot.has_value()) {
    return;  // unbound or another window's task
  }
  std::string json;
  base::JSONWriter::Write(TaskSnapshotToValue(snapshot.value()), &json);
  page_->PushTaskSnapshot(json);
}

std::optional<TaskSnapshot> SeoulCanvasPageHandler::BoundTask(
    const std::string& task_id) const {
  if (!runtime_) {
    return std::nullopt;
  }
  const std::optional<LiveWindowKey> window = ResolveBoundWindow();
  if (!window.has_value()) {
    return std::nullopt;
  }
  std::optional<TaskSnapshot> snapshot =
      runtime_->tasks()->Snapshot(TaskId::FromString(task_id));
  if (!snapshot.has_value() || !(snapshot->window == window.value())) {
    return std::nullopt;
  }
  return snapshot;
}

bool SeoulCanvasPageHandler::ApplyWorkflowEdit(
    const std::string& node_id,
    const base::DictValue& payload) {
  if (!runtime_ || !runtime_->workflows()) {
    return false;
  }
  const std::string* workflow_value = payload.FindString("workflow_id");
  const std::string* op = payload.FindString("op");
  if (!workflow_value || !op) {
    return false;
  }
  const WorkflowId workflow = WorkflowId::FromString(*workflow_value);
  if (!workflow.is_valid()) {
    return false;
  }
  WorkflowService* workflows = runtime_->workflows();
  if (*op == "remove_node") {
    return workflows->RemoveNode(workflow, node_id).has_value();
  }
  if (*op == "remove_edge") {
    const std::string* from = payload.FindString("from");
    const std::string* to = payload.FindString("to");
    return from && to &&
           workflows->RemoveEdge(workflow, *from, *to).has_value();
  }
  if (*op == "add_edge") {
    const std::string* from = payload.FindString("from");
    const std::string* to = payload.FindString("to");
    if (!from || !to) {
      return false;
    }
    WorkflowEdge edge;
    edge.from = *from;
    edge.to = *to;
    return workflows->AddEdge(workflow, edge).has_value();
  }
  return false;  // unknown ops fail closed and are reported by the caller
}

void SeoulCanvasPageHandler::OnRealtimeVoiceSessionCreated(
    CreateRealtimeVoiceSessionCallback callback,
    RealtimeVoiceAgent::CreateSessionResult result) {
  if (!result.has_value()) {
    PushStatus("realtime_voice_unavailable");
    std::move(callback).Run(ErrorJson(result.error()));
    return;
  }
  PushStatus("realtime_voice_ready");
  std::move(callback).Run(WriteJson(std::move(result.value())));
}

void SeoulCanvasPageHandler::PushStatus(const std::string& detail) {
  if (!page_) {
    return;
  }
  base::DictValue status;
  status.Set("detail", detail);
  if (runtime_) {
    const ProviderStateSnapshot providers = runtime_->providers()->Snapshot();
    status.Set("local_ready", providers.local_healthy);
    status.Set("cloud_ready", runtime_->providers()->cloud_available());
    status.Set("route", providers.local_healthy ? "local" : "cloud");
    status.Set("active_task_count",
               static_cast<int>(runtime_->tasks()->task_count()));
    const VoiceRuntimeSnapshot voice = runtime_->VoiceSnapshot();
    status.Set("voice_state", VoiceSessionStateToString(voice.state));
    status.Set("voice_error", VoiceErrorToString(voice.last_error));
    status.Set("voice_route", voice.route == SpeechRoute::kCloud ? "cloud"
                                                                 : "local");
    status.Set("voice_provider_available", voice.speech_provider_available);
    status.Set("voice_output_available", voice.speech_output_available);
    status.Set("voice_active_task_id", voice.active_task_id);
    const RealtimeVoiceAgentSnapshot realtime_voice =
        runtime_->RealtimeVoiceSnapshot();
    status.Set("voice_engine", "seoul_realtime_voice_agent");
    status.Set("voice_api_model", realtime_voice.api_model);
    status.Set("voice_product_target", realtime_voice.product_target);
    status.Set("voice_realtime_configured", realtime_voice.configured);
    status.Set("voice_realtime_creating",
               realtime_voice.creating_session);
    status.Set("voice_realtime_error", realtime_voice.last_error);
  } else {
    status.Set("local_ready", false);
    status.Set("cloud_ready", false);
    status.Set("voice_state", "failed");
    status.Set("voice_error", "provider_unavailable");
    status.Set("voice_provider_available", false);
    status.Set("voice_realtime_configured", false);
  }
  std::string json;
  base::JSONWriter::Write(status, &json);
  page_->SetStatus(json);
}

void SeoulCanvasPageHandler::PushPageContext() {
  if (!page_) {
    return;
  }
  base::DictValue context;
  context.Set("status", "unavailable");
  context.Set("tab_id", "");
  context.Set("title", "");
  context.Set("origin", "");
  context.Set("customizable", false);
  if (runtime_) {
    const std::optional<LiveWindowKey> window = ResolveBoundWindow();
    if (window.has_value()) {
      const std::optional<LiveTabDescriptor> active =
          runtime_->ActiveTabDescriptor(window.value());
      if (active.has_value()) {
        const bool is_web_page = IsValidOriginPattern(active->origin);
        context.Set("status", is_web_page ? "ready" : "unavailable");
        context.Set("tab_id", active->tab.value());
        context.Set("title", active->title);
        context.Set("origin", active->origin);
        context.Set("customizable", is_web_page);
      }
    }
  }
  page_->SetPageContext(WriteJson(std::move(context)));
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
