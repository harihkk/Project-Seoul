# Browser organization audit (Arc, Zen, Aside, Chromium M149)

Audit grounding the Seoul Workspace and Tab Lifecycle Engine. Competitive behavior
is from official docs and reputable sources (resources.arc.net, docs.zen-browser.app
and its DeepWiki mirror + GitHub, plus secondary write-ups). Chromium integration
points are verified by `git grep` in the pinned checkout at the locked revision
`6a7b3dbec3b2ca25877c2553b5473b2f277ef644`. Anything not confirmed in the checkout
is marked RESEARCH REQUIRED. Plain ASCII; this is engineering input, not marketing.

Context: Arc is in maintenance mode (The Browser Company shifted to Dia; security
patches only, no new features), so its weaknesses are unlikely to be fixed and are
fair to improve on. Zen is an actively developed Firefox/Gecko fork (not Chromium),
so its behaviors are references, not portable code.

## Arc

| Capability | Reference behavior | User problem | Good | Bad / limitation | Seoul reproduces | Seoul changes |
|---|---|---|---|---|---|---|
| Spaces | Distinct browsing areas, each with pinned/unpinned sections, theme, icon; tied to a Profile (cookies shared within a Profile) | Context separation | One gesture re-themes + swaps the whole sidebar | Space-vs-Profile conflation: two Spaces share logins, surprising users | Workspaces as persistent activity areas | Make the profile boundary explicit; a workspace never silently changes identity isolation |
| Favorites | Global persistent tabs (icon grid) above Spaces, ~12 max | One-click access everywhere | Reachable from any Space | Overlaps Pinned; URL sometimes reverts after navigation | Global Essentials | Single identity, defined reset semantics, no accidental reverts |
| Pinned Tabs | Space-specific pinned tabs above a divider; exempt from auto-archive | Stable per-context set | Persist with the Space | Divider drag boundary is undiscoverable; revert bug | Workspace-pinned tabs with saved root url | Explicit pin/unpin ops, deterministic reset |
| Unpinned/temporary | Below divider; auto-archived after ~12h (timer resets on view) | Disposable, self-cleaning | Sidebar self-cleans | #1 complaint: "tabs disappeared"; cannot fully disable | Temporary tabs | Auto-archive is gated by protection conditions, never a blind timer; clearer recovery |
| Folders | Group tabs in a Space; tabs in folders do not auto-archive | Mid-level grouping | Protects a set from archive | Three overlapping layers (Space/folder/pin) with weak guidance | RESEARCH REQUIRED (folders deferred past v0; model supports ordering today) | Fewer overlapping concepts |
| Auto archive | Idle unpinned tabs archived on a cadence (12h-30d), not closed | Tab hygiene | Recoverable | Cannot be disabled; destroys perceived state | Recoverable archive with bounded retention | Disable-able; protection rules; explicit, bounded archive store |
| Tab restoration | Reopen from Archive | Recover closed work | Recoverable | Archive feels like a black box | Archive restore to original workspace | Restored tabs return retained (not instantly re-archived) |
| Peek | Lightweight preview of a link, promotable to a real tab | Glance without committing | No tab clutter | Ambiguous when a Peek becomes permanent | Preview state model (this milestone: state only) | A preview never silently becomes a retained tab |
| Split View | Side-by-side panes | Compare/reference | Native multitask | Limited pane management | Splits over Chromium MultiContentsView | Use upstream split model; clean workspace ownership |
| Air Traffic Control | Per-URL routing rules (open in a Space/app) | Right place automatically | Powerful | Per-site setup; opaque precedence | General routing engine | Deterministic priority, no hardcoded sites, testable, user override |
| Command bar | Keyboard-first navigation/search | Fast control | Reduces mouse | Discoverability | RESEARCH REQUIRED (command navigation is a later UI milestone) | Original Seoul command surface |
| Multiple windows | Windows show Spaces | Multi-monitor | Flexible | Cross-window consistency is implicit | Window projection model | Deliberate per-window active workspace + shared model |

Sources: resources.arc.net help center (Spaces, Favorites, Pinned Tabs, Auto
Archive, Folders, Split View, Peek), plus secondary coverage of Arc's status.

## Zen Browser

| Capability | Reference behavior | User problem | Good | Bad / limitation | Seoul reproduces | Seoul changes |
|---|---|---|---|---|---|---|
| Workspaces | Per-task tab containers; per-workspace pinned tabs, theme, default container; active id persisted (`zen.workspaces.active`); inactive tabs hidden/unloadable | Tab overload | Instant themed swap | No per-workspace default search engine | Workspaces with persisted active state | Cleaner separation of view vs owned state |
| Essentials | Pinned, app-like tabs shown across workspaces (within a profile) | Global apps | Always available | Scope vs per-workspace pins can confuse | Global Essentials | Single identity, no duplicate live tab across workspaces |
| Pinned tabs | Workspace-scoped pins with a saved/reset URL | Stable set | Reset to a known page | Reset vs live-state nuance | Workspace-pinned tabs | Explicit saved root url + reset op |
| Regular tabs | Ordinary tabs in a workspace | Browsing | Familiar | - | Temporary/retained tabs | Explicit lifecycle roles |
| Workspace bookmarks | Per-workspace bookmark area | Scoped saves | Context-local | Distinct from pins is subtle | RESEARCH REQUIRED (bookmarks vs Essentials boundary; deferred) | Clear pins-vs-Essentials-vs-bookmarks distinction |
| Container assignment | Workspace can have a default Firefox container | Identity isolation | Per-context cookies | Firefox-specific; Chromium has no containers | Account/container isolation as a FUTURE capability | Map to Chromium profiles; never silently cross profiles |
| Container routing | Sites auto-open in a container/workspace | Auto-routing | Hands-off | Firefox-specific | Routing engine (container as a future predicate) | Deterministic, profile-aware routing |
| Split View | 2+ pane split | Compare | Native | Pane mgmt limits | Splits (2 panes in v0) | Upstream MultiContentsView, clean ownership |
| Glance | Modal preview overlay of a link | Quick look | Non-disruptive | Promotion semantics | Preview state model | Defined promotion + no silent retention |
| Compact Mode | Auto-hiding chrome/sidebar | Maximize content | Clean | Discoverability of hidden UI | Compact browsing (later UI; state-aware now) | Original compact behavior |
| Window sync/recovery | Windows recover workspace state | Reliability | Resilient | Mirroring ambiguity | Window projection + session restore | Deliberate Seoul semantics, not blind mirroring |
| Keyboard shortcuts | Configurable | Power use | Flexible | - | Configurable navigation (later) | Original mapping |

Sources: docs.zen-browser.app, the project DeepWiki mirror, the Zen GitHub repo, a
third-party feature guide.

## Aside (browser-state implications only)

Primary sources are thin; the relevant pattern (task-oriented browsing) is recorded
without inventing specifics. Seoul does NOT implement these in this milestone; the
engine only leaves room for them.

- Persistent tasks: long-lived units of work that survive restart. Seoul: future
  task state references workspaces/tabs by stable id (the engine provides those
  ids). Bad pattern to avoid: persisting live renderer/page content as "task state."
- Local task state: kept locally, not in the cloud by default. Seoul: organization
  metadata is local and bounded; no network dependency for ordinary browsing.
- Permission approvals / credential isolation / access logs: gated, auditable,
  isolated. Seoul: these map onto the permission/risk-policy, credential-mediation,
  and audit-trail subsystems (see native-architecture.md); harness lessons apply.
- Pause/resume, no-replay: an interrupted action with an unknown outcome is never
  replayed. Seoul: carried directly from the frozen harness (harness-lessons.md).

## Chromium M149 integration points (verified in the pinned checkout)

| Concept | Path | Symbol | Ownership / note | Status |
|---|---|---|---|---|
| Browser / window | chrome/browser/ui/browser.h, .../browser_list.h | `Browser`, `BrowserList` | BrowserList owns the set of Browsers | confirmed |
| Tab strip | chrome/browser/ui/tabs/tab_strip_model.h | `TabStripModel`, `TabStripModelObserver` | Browser owns its TabStripModel | confirmed |
| Tab handle | components/tabs/public/tab_interface.h | `tabs::TabInterface`, fwd `SplitTabId` | stable tab handle for the bridge | confirmed |
| Profile | chrome/browser/profiles/profile.h | `Profile::IsRegularProfile/IsOffTheRecord/IsGuestSession/IsSystemProfile` | the data-isolation boundary | confirmed |
| Profile-keyed service | chrome/browser/profiles/profile_keyed_service_factory.h | `ProfileKeyedServiceFactory` | base for the Seoul factory | confirmed |
| Profile selection | chrome/browser/profiles/profile_selections.h | `ProfileSelections::Builder().WithRegular/WithGuest/WithSystem/WithAshInternals` | excludes incognito/guest/system | confirmed |
| Pref registration | components/keyed_service/content/browser_context_keyed_service_factory.h | virtual `RegisterProfilePrefs(PrefRegistrySyncable*)` | per-factory bounded prefs | confirmed |
| Factory registration | chrome/browser/profiles/chrome_browser_main_extra_parts_profiles.cc | `EnsureBrowserContextKeyedServiceFactoriesBuilt()` | the patch adds one GetInstance() here | confirmed |
| Stable id | base/uuid.h | `base::Uuid` (GenerateRandomV4 / ParseLowercase / AsLowercaseString) | Seoul id backing | confirmed |
| Persistence value | base/values.h, base/json/values_util.h | `base::DictValue`, `base::TimeToValue`/`ValueToTime` | Seoul store types | confirmed |
| Result type | base/types/expected.h | `base::expected`, `base::ok`, `base::unexpected` | mutation results | confirmed |
| Split tabs | chrome/browser/ui/views/frame/multi_contents_view.{h,cc} | `MultiContentsView` (max 2 panes) | BrowserView owns it | confirmed |
| Split model | components/tabs/public/split_tab_data.h | `split_tabs::SplitTabId`, `SplitTabData` | upstream split identity | confirmed |
| Split entry | chrome/browser/ui/accelerator_table.cc | `IDC_NEW_SPLIT_TAB` (EF_SHIFT_DOWN|EF_ALT_DOWN, N) | Shift+Alt/Option+N | confirmed |
| Vertical tabs | chrome/browser/ui/tabs/features.cc | `base::Feature kVerticalTabs` (default disabled) | chrome://flags/#vertical-tabs | confirmed |
| Session restore | chrome/browser/sessions/session_restore.cc | `RestoreSplitTabVisualData`, `RestoreSplit` | restores splits | confirmed |
| Saved tab groups | components/tabs (saved/grouped tab data) | tab-group data | relation to workspaces | RESEARCH REQUIRED (exact group API surface for Seoul use) |
| Window restoration | chrome/browser/sessions/ | session service | window/workspace restore wiring | RESEARCH REQUIRED (exact hook for window->workspace mapping) |
| GN cycle-free service placement | chrome/browser/BUILD.gn | n/a | service compiled with chrome/browser vs standalone lib | RESEARCH REQUIRED (resolve at build host) |

## Seoul direction summary

- Keep the Chromium profile as the isolation boundary; workspaces are a layer ON
  TOP, never a replacement.
- Reproduce: workspaces, global Essentials, workspace pins, temporary tabs with a
  recoverable archive, splits over the upstream model, link routing, preview state,
  window projection.
- Deliberately change: auto-archive is gated by protection (never a blind timer);
  Essentials are a single identity; routing is general/testable/no-hardcoding;
  previews never silently retain; window semantics are explicit, not mirrored.
- Defer (not in v0): folders, command navigation UI, compact UI, per-workspace
  search, account/container isolation, and all Aside task/AI behavior.
