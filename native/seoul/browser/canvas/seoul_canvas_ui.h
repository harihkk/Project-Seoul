// Project Seoul Canvas - WebUI controller.
//
// Serves the first-party Canvas WebUI at chrome://seoul-canvas inside a native
// Views side panel and binds the Mojo PageHandlerFactory. The data source is
// packaged, in-process resources with a strict CSP: no remote scripts, no
// eval, script-src 'self' only. This is Chromium-facing glue; it compiles only
// on a capable host. The WebUIConfig is registered by the integration patch,
// and a per-window SidePanelEntry (id kSeoulCanvas) shows it.

#ifndef SEOUL_BROWSER_CANVAS_SEOUL_CANVAS_UI_H_
#define SEOUL_BROWSER_CANVAS_SEOUL_CANVAS_UI_H_

#include <memory>

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "seoul/browser/canvas/canvas.mojom.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace seoul {

class SeoulCanvasPageHandler;

inline constexpr char kSeoulCanvasHost[] = "seoul-canvas";

// WebUIConfig so the WebUI system can construct the controller for the host.
class SeoulCanvasUIConfig : public content::WebUIConfig {
 public:
  SeoulCanvasUIConfig();
  ~SeoulCanvasUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class SeoulCanvasUI : public ui::MojoWebUIController,
                      public canvas::mojom::PageHandlerFactory {
 public:
  explicit SeoulCanvasUI(content::WebUI* web_ui);
  SeoulCanvasUI(const SeoulCanvasUI&) = delete;
  SeoulCanvasUI& operator=(const SeoulCanvasUI&) = delete;
  ~SeoulCanvasUI() override;

  // Instantiated by the mojo bindings when the renderer requests the factory.
  void BindInterface(
      mojo::PendingReceiver<canvas::mojom::PageHandlerFactory> receiver);

  // canvas::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<canvas::mojom::Page> page,
      mojo::PendingReceiver<canvas::mojom::PageHandler> handler) override;

 private:
  std::unique_ptr<SeoulCanvasPageHandler> page_handler_;
  mojo::Receiver<canvas::mojom::PageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_CANVAS_SEOUL_CANVAS_UI_H_
