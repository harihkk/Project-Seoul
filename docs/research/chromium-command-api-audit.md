# Chromium command API audit (pinned M149)

Pinned revision: `6a7b3dbec3b2ca25877c2553b5473b2f277ef644`.
Checkout: `/Users/hk/Documents/Projects/seoul-chromium/src`.
Desktop APIs only unless noted.

## Selected for command layer V0

| Operation | API | Path | Status |
| --- | --- | --- | --- |
| Window lookup | `ProfileBrowserCollection::GetForProfile` | `chrome/browser/ui/browser_window/public/profile_browser_collection.h` | selected |
| Window lookup | `BrowserCollection::FindBrowserWithID(SessionID)` | `chrome/browser/ui/browser_window/public/browser_collection.h` | selected |
| Window state | `BrowserWindowInterface::GetSessionID`, `IsDeleteScheduled`, `GetTabStripModel` | `chrome/browser/ui/browser_window/public/browser_window_interface.h` | selected |
| Tab identity | `SessionTabHelper::IdForTab` | `components/sessions/content/session_tab_helper.h` | selected |
| Tab object | `tabs::TabInterface::GetContents`, `Close` | `components/tabs/public/tab_interface.h` | selected |
| Tab index | `TabStripModel::GetIndexOfTab`, `GetTabAtIndex` | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Open tab | `Navigate(NavigateParams*)` | `chrome/browser/ui/navigator/browser_navigator.h` | selected |
| Activate | `TabStripModel::ActivateTabAt(int, TabStripUserGestureDetails)` | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Close | `TabStripModel::CloseWebContentsAt(int, uint32_t close_types)` | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Pin | `TabStripModel::SetTabPinned(int, bool)` → returns final index | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Move | `TabStripModel::MoveWebContentsAt(int, int, bool)` | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Split create | `TabStripModel::AddToNewSplit(vector<int>, SplitTabVisualData, SplitTabCreatedSource)` | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Split remove | `TabStripModel::RemoveSplit(SplitTabId)` | `chrome/browser/ui/tabs/tab_strip_model.h` | selected |
| Observers | `TabStripModelObserver::OnTabStripModelChanged`, `OnTabCloseCancelled`, `OnSplitTabChanged` | `chrome/browser/ui/tabs/tab_strip_model_observer.h` | selected |

## Rejected or deferred

| Operation | Reason |
| --- | --- |
| `BrowserWindowInterface::OpenGURL` | No centralized URL validation; `Navigate(NavigateParams*)` preferred |
| Cross-window tab move | Deferred V0 scope |
| Window create/close | Deferred V0 scope |
| `UpdateSplitRatio` / layout commands | Deferred unless explicit product need |
| `mojom::TabStripService::ActivateTabResult` | RESEARCH REQUIRED for exact Mojo enum values |

## API detail records

### Navigate(NavigateParams*)

```cpp
base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params);
```

- **Ownership:** Non-owning; fills `params->navigated_or_inserted_contents`.
- **Preconditions:** Profile not shutting down; browser not `IsDeleteScheduled()`.
- **Sync return:** `nullptr` = navigation not started / cancelled.
- **Observer completion:** `TabStripModelChange::kInserted` + optional selection change.
- **Cancellation:** Unload handlers, policy gates return `nullptr`.
- **Identity:** New tab receives new SessionID via `SessionTabHelper`.

### ActivateTabAt

```cpp
void ActivateTabAt(int index, TabStripUserGestureDetails gesture_detail = ...);
```

- **Preconditions:** `CHECK(ContainsIndex(index))`.
- **Already active:** May not emit `active_tab_changed` if same WebContents.
- **Observer:** `TabStripSelectionChange` via `OnTabStripModelChanged`.

### CloseWebContentsAt

```cpp
void CloseWebContentsAt(int index, uint32_t close_types);
```

- **Flags:** `CLOSE_USER_GESTURE`, `CLOSE_CREATE_HISTORICAL_TAB`.
- **Cancellation:** `OnTabCloseCancelled` when not closable or unload deferred.
- **Async:** Unload listener may defer removal.

### SetTabPinned

```cpp
int SetTabPinned(int index, bool pinned);
```

- **Returns:** Final index after pinned-region adjustment.

### AddToNewSplit

```cpp
split_tabs::SplitTabId AddToNewSplit(
    std::vector<int> indices,
    split_tabs::SplitTabVisualData visual_data,
    split_tabs::SplitTabCreatedSource source);
```

- **Preconditions:** Exactly one index in vector; active tab exists and differs; tabs not already split.
- **Observer:** `SplitTabChange::kAdded`.

### RemoveSplit

```cpp
void RemoveSplit(split_tabs::SplitTabId split_id);
```

- **Observer:** `SplitTabChange::kRemoved`; tabs remain live.

## RESEARCH REQUIRED

- `InsertWebContentsAt` return index under group/split constraints.
- `OpenGURL` failure paths through `OpenURL`.
- Unload user-cancel UI flow details.
- Android `TabListInterface` divergence.
