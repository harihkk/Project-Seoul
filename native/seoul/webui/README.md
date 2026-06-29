# native/seoul/webui

Seoul-owned WebUI surfaces (HTML/TypeScript/CSS) served from the browser process,
for Seoul's original interface (workspace switcher, command navigation, settings,
and similar) where WebUI is the right tool.

Contract:
- Materialized to `$SEOUL_CHROMIUM_ROOT/src/seoul/webui/`.
- Whether a given surface is WebUI or Views/C++ is a per-surface decision recorded
  in `docs/product/native-architecture.md`; some are marked "research required".
- Build wiring lives in `../config/`; host registration (WebUIController, URL data
  source) is done via integration patches in `../../patches/chromium/`.
- Original Seoul UI only. No proprietary layouts or assets copied from any browser.

Empty in this milestone by design. Do not add placeholder source.
