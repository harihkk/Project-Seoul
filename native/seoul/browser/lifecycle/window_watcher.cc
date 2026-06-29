// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/window_watcher.h"

#include <utility>

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/core/session_id.h"
#include "seoul/browser/lifecycle/lifecycle_coordinator.h"
#include "seoul/browser/lifecycle/lifecycle_events.h"
#include "seoul/browser/lifecycle/tab_strip_bridge.h"

namespace seoul {

WindowWatcher::WindowWatcher(Profile* profile,
                             LifecycleCoordinator* coordinator)
    : profile_(profile), coordinator_(coordinator) {}

WindowWatcher::~WindowWatcher() = default;

void WindowWatcher::StartObserving() {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile_);
  if (!collection) {
    return;
  }
  if (!observation_.IsObserving()) {
    observation_.Observe(collection);
  }
  // Discover already-open eligible windows exactly once (bounded single pass).
  collection->ForEach([this](BrowserWindowInterface* browser) {
    if (IsEligible(browser)) {
      Track(browser);
    }
    return true;  // continue iterating
  });
}

// static
bool WindowWatcher::IsEligible(BrowserWindowInterface* browser) {
  return browser && browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->IsDeleteScheduled();
}

void WindowWatcher::OnBrowserCreated(BrowserWindowInterface* browser) {
  if (IsEligible(browser)) {
    Track(browser);
  }
}

void WindowWatcher::OnBrowserClosed(BrowserWindowInterface* browser) {
  Untrack(browser);
}

void WindowWatcher::Track(BrowserWindowInterface* browser) {
  const SessionID& sid = browser->GetSessionID();
  if (!sid.is_valid()) {
    return;
  }
  const LiveWindowKey key = LiveWindowKey::FromSessionId(sid.id());
  if (bridges_.count(key)) {
    return;  // Already tracked; do not duplicate the window.
  }
  TabStripModel* model = browser->GetTabStripModel();
  if (!model) {
    return;
  }
  // Attach the per-window bridge before announcing the window.
  bridges_[key] = std::make_unique<TabStripBridge>(key, model, coordinator_);

  NormalizedEvent event;
  event.type = NormalizedEventType::kWindowDiscovered;
  event.window = key;
  event.persisted_window = PersistedWindowRef::FromSessionId(sid.id());
  coordinator_->OnNormalizedEvent(event);
}

void WindowWatcher::Untrack(BrowserWindowInterface* browser) {
  const SessionID& sid = browser->GetSessionID();
  if (!sid.is_valid()) {
    return;
  }
  const LiveWindowKey key = LiveWindowKey::FromSessionId(sid.id());
  auto it = bridges_.find(key);
  if (it == bridges_.end()) {
    return;
  }
  // Announce destruction before tearing down the bridge so the coordinator sees
  // a deterministic order; the bridge destructor detaches the tab-strip
  // observer.
  NormalizedEvent event;
  event.type = NormalizedEventType::kWindowDestroyed;
  event.window = key;
  coordinator_->OnNormalizedEvent(event);
  bridges_.erase(it);
}

}  // namespace seoul
