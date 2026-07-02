// Project Seoul Canvas - Mojo page handler (browser side).
//
// STATE OWNERSHIP (product-wide state rules):
//   owner:        the SeoulCanvasUI that created it (one per Canvas WebUI).
//   lifetime:     the WebUI document; destroyed on teardown/reconnect.
//   persistence:  per-window ephemeral (the Mojo remote and observer
//                 registrations); no durable state of its own.
//   recovery:     the renderer re-requests initial state on reconnect.
//   teardown:     receiver/remote reset and observers removed in the
//                 destructor; no observer leak, no callback after teardown.
//   bounds:       events are bounded on receipt; surfaces come pre-validated.
//   isolation:    one profile.
//
// It observes the profile runtime's surface and task services and pushes
// validated SAUI surfaces and status to the renderer; it validates typed
// component events and turns and routes them into the runtime (planner/task/
// surface). It never sends arbitrary HTML to the renderer and never receives
// DOM or coordinates from it.

#ifndef SEOUL_BROWSER_CANVAS_SEOUL_CANVAS_PAGE_HANDLER_H_
#define SEOUL_BROWSER_CANVAS_SEOUL_CANVAS_PAGE_HANDLER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "seoul/browser/canvas/canvas.mojom.h"
#include "seoul/browser/product/surface_service.h"
#include "seoul/browser/product/task_service.h"
#include "seoul/browser/lifecycle/lifecycle_identity.h"
#include "seoul/browser/tasks/task_types.h"

class Profile;
class BrowserWindowInterface;

namespace seoul {

class SeoulRuntimeService;

class SeoulCanvasPageHandler : public canvas::mojom::PageHandler,
                               public SurfaceServiceObserver,
                               public TaskServiceObserver {
 public:
  SeoulCanvasPageHandler(
      mojo::PendingReceiver<canvas::mojom::PageHandler> receiver,
      mojo::PendingRemote<canvas::mojom::Page> page,
      Profile* profile,
      BrowserWindowInterface* browser_window);
  SeoulCanvasPageHandler(const SeoulCanvasPageHandler&) = delete;
  SeoulCanvasPageHandler& operator=(const SeoulCanvasPageHandler&) = delete;
  ~SeoulCanvasPageHandler() override;

  // canvas::mojom::PageHandler:
  void RequestInitialState() override;
  void NotifyComponentEvent(canvas::mojom::ComponentEventPtr event) override;
  void SubmitTurn(canvas::mojom::TurnInputPtr input) override;
  void StartVoice() override;
  void StopVoice() override;
  void CancelActiveTask(const std::string& task_id) override;

  // SurfaceServiceObserver:
  void OnSurfaceUpdated(const SurfaceId& id,
                        const std::string& surface_json) override;
  void OnSurfaceRemoved(const SurfaceId& id) override;

  // TaskServiceObserver:
  void OnTaskUpdated(const TaskId& task_id) override;
  void OnTaskNeedsApproval(const TaskId& task_id,
                           const std::string& step_id,
                           const std::string& prompt) override;
  void OnTaskFinished(const TaskId& task_id) override;

 private:
  // Pushes a compact status document (provider/voice/task state) to the
  // renderer so it never shows a blank page when the runtime is initializing.
  void PushStatus(const std::string& detail);
  std::optional<LiveWindowKey> ResolveBoundWindow() const;
  TaskId StartBoundGoal(const std::string& goal);

  mojo::Receiver<canvas::mojom::PageHandler> receiver_;
  mojo::Remote<canvas::mojom::Page> page_;
  raw_ptr<Profile> profile_;
  raw_ptr<SeoulRuntimeService> runtime_;  // null when the profile is ineligible
  base::UnguessableToken window_binding_token_;
  bool observing_ = false;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_CANVAS_SEOUL_CANVAS_PAGE_HANDLER_H_
