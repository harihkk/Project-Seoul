# Native Chromium baseline

Reproducible, unmodified Chromium baseline for Project Seoul on macOS arm64, plus an
audit of the upstream vertical-tab and split-tab implementations.

**The native Chromium checkout is COMPLETE and verified. The build is intentionally
deferred to a stronger machine.** Status of each stage:

| Stage | Status |
|---|---|
| Harness freeze | COMPLETED (commit `3961226`) |
| Bootstrap preparation (scripts, lock, GN, docs) | COMPLETED |
| Upstream lock verification | COMPLETED (re-verified 2026-06-26) |
| Chromium checkout + dependency sync | COMPLETED (`gclient validate: SUCCESS`) |
| Upstream feature-symbol audit (source level) | COMPLETED (derived via local `git grep`) |
| GN generation | PENDING (deferred to a stronger build host) |
| Build (`chrome`) | PENDING (deferred: 8 GiB RAM / disk) |
| Launch + smoke test | PENDING (needs a built binary) |
| Runtime feature audit (vertical/split) | PENDING (needs a built binary) |

Honesty note: the source-level audit below was derived from the real pinned local
checkout with `git grep`. Anything requiring a built or launched binary (GN, build,
smoke, runtime behavior) is NOT done and is not claimed as passing.

## Pinned upstream (resolved)

| Field | Value |
|---|---|
| Product version | 149.0.7827.201 |
| Milestone | 149 |
| Chromium revision (full SHA) | `6a7b3dbec3b2ca25877c2553b5473b2f277ef644` |
| Source used to resolve | official stable release, channel `Stable`, platform `Mac` (Chromium Dash `fetch_releases`) |
| depot_tools revision (full SHA) | `1e85b18d8037bca45f9a9891ae9325729f3ebcd4` (branch `main`) |
| Resolved at | 2026-06-26 |

Machine-readable: `native/chromium.lock.json`. Both required upstream implementations
are present at this exact revision and were further confirmed by local `git grep`
after checkout (see audits).

## Checkout (completed)

- External checkout root: the sibling directory `seoul-chromium.noindex` next to the repo
  (outside the Project Seoul repo; configurable via `SEOUL_CHROMIUM_ROOT`).
- `depot_tools` cloned and pinned to the locked SHA (self-update disabled).
- `src` fetched with `fetch --no-history`, checked out detached at the locked SHA,
  then `gclient sync --no-history --with_branch_heads --with_tags` + `gclient runhooks`.
- Verification: `git rev-parse HEAD` == lock (`6a7b3db…`); no edits to tracked files
  (a gclient checkout normally shows modified submodule pointers + untracked
  dependency dirs, which are expected); **`gclient validate: SUCCESS`**.
- Size on disk: ~48 GB; free disk after checkout: ~72 GiB.
- The dependency sync was rate-limited (HTTP 429) by the upstream git server and
  completed on a retry; the checkout itself is complete and consistent.

External-SSD note: moving the checkout onto the available USB SSD (`MiniDeck`, APFS,
931 GiB free) was attempted and abandoned. A Chromium checkout is ~450k small files,
and the USB connection stalled on small-file writes (~1.5 GB in 35 min). The checkout
therefore lives on the internal disk. A USB SSD is also a poor build target for the
same small-file reason; a Thunderbolt/NVMe volume or a stronger Mac is preferable.

## Environment (observed)

| Item | Value |
|---|---|
| macOS | 26.5.1 (build 25F80) |
| Architecture | arm64 |
| RAM | 8 GiB |
| Free disk after checkout (`/`) | ~72 GiB |
| Filesystem | APFS (internal); USB SSD `MiniDeck` also APFS but too slow for this |
| Xcode | Xcode 26.6 (build 17F113), active at `/Applications/Xcode.app/Contents/Developer` |
| macOS SDK | `MacOSX.sdk` 26.5 via the active Xcode |
| git | 2.50.1 (Apple Git-155) |
| python3 | 3.14.6 |

## Build readiness / why the build is deferred

Checkout readiness (`doctor.sh`) and build readiness are now separate.
`doctor.sh` covers the checkout. Build readiness is a hard gate,
`build-host-check.sh`, which `gen.sh` and `build.sh` require before running. On this
machine `build-host-check.sh` FAILS, so a build cannot start here. Observed reasons:

1. **8 GiB RAM.** Below the 16 GiB build minimum (32 GiB comfortable). The `chrome`
   link step is memory-hungry and will thrash or OOM. RAM is a hard gate, not a
   warning.
2. **Disk.** ~72 GiB free after the checkout; a component `out/SeoulBaseline` adds
   ~50-100 GiB, which does not fit safely (build minimum default 150 GiB).
3. **Storage speed.** The only external volume is a USB SSD that stalls on
   Chromium's small-file workload (checkout and build alike).

On a machine with >= 16 GiB RAM (ideally 32) and >= 250 GiB free fast storage, the
build runs unchanged (full procedure in `remote-build-runbook.md`):

```
native/scripts/verify-checkout.sh   # read-only checkout verification
native/scripts/build-host-check.sh  # hard build gate (RAM/disk/Xcode/checkout)
native/scripts/gen.sh               # gn gen out/SeoulBaseline
native/scripts/build.sh             # build chrome (SEOUL_NINJA_JOBS configurable)
node native/scripts/smoke.mjs
```

Planned GN args (`native/gn/macos-arm64-baseline.gn` + gated `target_cpu`):
```
is_debug = false
is_component_build = true
symbol_level = 0
use_lld = false
target_cpu = "arm64"
```
Build target: `chrome` only. Binary: `out/SeoulBaseline/Chromium.app/Contents/MacOS/Chromium`.

## Smoke test (designed, pending a binary)

`native/scripts/smoke.mjs` uses the repo's pinned Puppeteer with an explicit
`executablePath` to the built binary: fresh temp profile; launch; `data:` URL (no
internet); JS + DOM check; open and activate a second tab; assert connected with no
crash; collect `browser.version()`; clean close; remove the temp profile; fail on
disconnect or page crash. It exits with a clear error if the binary is absent (the
current state).

## Vertical-tab audit (source level, from the pinned local checkout)

**Feature flag:** `base::Feature kVerticalTabs` -- defined
`chrome/browser/ui/tabs/features.cc:18`, declared `chrome/browser/ui/tabs/features.h:26`.
Default: **`FEATURE_DISABLED_BY_DEFAULT`**. Feature string `"VerticalTabs"` (the 2-arg
`BASE_FEATURE` macro derives the name from the identifier).

**Related features** (all disabled by default): `kVerticalTabsLaunch`,
`kVerticalTabsPreviewBadge`, `kVerticalTabsNewBadge`, `kVerticalTabsExpandOnHover`
(`chrome/browser/ui/tabs/features.cc`); `kVerticalTabsGrabHandleRemoval`
(`chrome/browser/ui/ui_features.cc:471`).

**chrome://flags exposure:** entry `{"vertical-tabs", ...}` at
`chrome/browser/about_flags.cc:7001` -> `chrome://flags/#vertical-tabs`, `kOsDesktop`.

**Preferences** (`chrome/common/pref_names.h`): `kVerticalTabsEnabled =
"vertical_tabs.enabled"` (orientation/enable), plus `kVerticalTabsExpandOnHoverEnabled`,
`kVerticalTabsEnabledFirstTime`, `kVerticalTabsCollapsedState`,
`kVerticalTabsUncollapsedWidth`. State controller:
`chrome/browser/ui/tabs/vertical_tab_strip_state_controller.{cc,h}`.

**Views** (`chrome/browser/ui/views/tabs/vertical/`): `VerticalTabStripView`,
`VerticalTabStripController`, `VerticalTabView`, pinned/unpinned container views,
group + group-header views, drag/drop handlers, expand-on-hover lock,
`vertical_split_tab_view`, tab-collection nodes.

**Tests:** `vertical_tab_strip_controller_{browsertest,interactive_uitest}.cc`,
`vertical_tab_view_browsertest.cc`, `vertical_tab_group_view_browsertest.cc`,
`vertical_tab_strip_state_controller_unittest.cc`, and more.

**Runtime behavior: PENDING a build.** Once built: enable via
`chrome://flags/#vertical-tabs` (or `--enable-features=VerticalTabs`) and verify strip
appears, tab switch/new/close, pin/unpin, groups, resize/collapse, and that horizontal
mode remains available.

## Split-tab audit (source level, from the pinned local checkout)

**Feature gating:** there is **no single master `base::Feature`** for split tabs at
this revision (no `kSideBySide`). Split tabs is **built-in / on by default at M149**:
`MultiContentsView` is created unconditionally by `BrowserView`
(`chrome/browser/ui/views/frame/browser_view.cc:888`, owned as `multi_contents_view_`).
Only refinement sub-features exist: `kSplitViewTabDraggingUpdates` (ENABLED,
`chrome/browser/ui/ui_features.cc:121`), `kSplitViewDragAndDropVelocity` (ENABLED,
`ui_features.cc:128`); flag `split-view-link-open` (`about_flags.cc:4928`).

**Model:** namespace `split_tabs`; `SplitTabId` (fwd-declared
`components/tabs/public/tab_interface.h:33`), with classes under
`components/tabs/public/`: `split_tab_collection.h`, `split_tab_data.h`,
`split_tab_util.*`, `split_tab_menu_model.*`, `split_tab_swap_menu_model.*`,
`split_tab_highlight_controller.*`, `split_view_iph_controller.*`.
`TabStripModel::AddToNewSplit()` / `RestoreSplit()`.

**View integration:** `chrome/browser/ui/views/frame/multi_contents_view.{h,cc}`
(`MultiContentsView`) + `MultiContentsViewDelegate`, `multi_contents_resize_area.*`
(divider), `multi_contents_view_mini_toolbar.*`, drop-target views; per-pane
`contents_container_view.*` / `contents_web_view.*`.

**Entry points:** `IDC_NEW_SPLIT_TAB` (accelerator Shift+Alt+N,
`chrome/browser/ui/accelerator_table.cc:321`), `IDC_SPLIT_TAB` (action
`kActionSplitTab`, `chrome/browser/ui/actions/chrome_action_id.h:617`), tab context
menu (`split_tab_menu_model`), bookmark bar `IDC_BOOKMARK_BAR_OPEN_SPLIT_VIEW`.

**Max pane count: 2** -- `multi_contents_view.h:101` ("we currently only support two
contents") and `chrome/browser/sessions/session_restore.cc:960` ("The number of
supported split tabs is 2").

**Session restoration: supported** -- `session_restore.cc` (`RestoreSplitTabVisualData`,
`RestoreSplit`, `tabs_by_split_id`, `window->split_tabs`); sessions depends on
`//components/split_tabs`.

**Tests:** `multi_contents_view_{browsertest,interactive_uitest}.cc`,
`multi_contents_view_tab_drag_interactive_uitest.cc`,
`multi_contents_view_drop_target_controller_{browsertest,unittest}.cc`,
`multi_contents_drop_target_view_unittest.cc`, `session_restore_browsertest.cc`,
`split_view_iph_controller_unittest.cc`.

**Runtime behavior: PENDING a build.** Once built: create a split (Shift+Alt+N or the
tab menu), load two pages, switch the active pane, resize the divider, close one side,
exit without losing tabs; confirm the 2-pane cap.

## Evidence

Operation logs live under `<checkout-root>/.seoul-logs/` (untracked, outside the repo).
Temporary screenshots/raw build evidence go under `native/evidence/` (gitignored).
Only the written findings above are committed; no Chromium source, build output,
profiles, or screenshots are tracked.
