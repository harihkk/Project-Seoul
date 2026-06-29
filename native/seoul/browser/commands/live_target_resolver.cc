// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/live_target_resolver.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "seoul/browser/lifecycle/tab_strip_bridge.h"

namespace seoul {
namespace {

BrowserWindowInterface* FindBrowser(Profile* profile, LiveWindowKey window) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(profile);
  if (!collection) {
    return nullptr;
  }
  const SessionID desired = SessionID::FromSerializedValue(window.session_id());
  BrowserWindowInterface* browser = collection->FindBrowserWithID(desired);
  if (!browser || browser->GetProfile() != profile ||
      browser->IsDeleteScheduled() ||
      browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return nullptr;
  }
  return browser;
}

}  // namespace

LiveTargetResolver::LiveTargetResolver() = default;
LiveTargetResolver::~LiveTargetResolver() = default;

CommandResult<ResolvedWindowTarget> LiveTargetResolver::ResolveWindow(
    Profile* profile,
    LiveWindowKey window) {
  if (!profile || !window.is_valid()) {
    return base::unexpected(CommandError::kInvalidCommand);
  }
  if (!FindBrowser(profile, window)) {
    return base::unexpected(CommandError::kWindowNotFound);
  }
  ResolvedWindowTarget target;
  target.window = window;
  return target;
}

CommandResult<ResolvedTabTarget> LiveTargetResolver::ResolveTab(
    Profile* profile,
    LiveWindowKey window,
    LiveTabKey tab) {
  if (!profile || !window.is_valid() || !tab.is_valid()) {
    return base::unexpected(CommandError::kInvalidCommand);
  }
  BrowserWindowInterface* browser = FindBrowser(profile, window);
  if (!browser) {
    return base::unexpected(CommandError::kWindowNotFound);
  }
  TabStripModel* strip = browser->GetTabStripModel();
  if (!strip) {
    return base::unexpected(CommandError::kTargetDisappeared);
  }
  for (int index = 0; index < strip->count(); ++index) {
    tabs::TabInterface* candidate = strip->GetTabAtIndex(index);
    if (TabStripBridge::KeyForTab(candidate) == tab) {
      ResolvedTabTarget target;
      target.window = window;
      target.tab = tab;
      target.current_index = index;
      target.already_active = strip->active_index() == index;
      target.already_pinned = strip->IsTabPinned(index);
      return target;
    }
  }
  return base::unexpected(CommandError::kStaleTabReference);
}

CommandResult<ResolvedSplitTarget> LiveTargetResolver::ResolveSplitPanes(
    Profile* profile,
    LiveWindowKey window,
    LiveTabKey pane_a,
    LiveTabKey pane_b) {
  if (!profile || !window.is_valid() || !pane_a.is_valid() ||
      !pane_b.is_valid() || pane_a == pane_b) {
    return base::unexpected(CommandError::kSplitPreconditionFailure);
  }
  auto resolved_a = ResolveTab(profile, window, pane_a);
  auto resolved_b = ResolveTab(profile, window, pane_b);
  if (!resolved_a.has_value() || !resolved_b.has_value()) {
    return base::unexpected(CommandError::kSplitPreconditionFailure);
  }
  BrowserWindowInterface* browser = FindBrowser(profile, window);
  TabStripModel* strip = browser ? browser->GetTabStripModel() : nullptr;
  if (!strip) {
    return base::unexpected(CommandError::kSplitPreconditionFailure);
  }
  tabs::TabInterface* tab_a = strip->GetTabAtIndex(resolved_a->current_index);
  tabs::TabInterface* tab_b = strip->GetTabAtIndex(resolved_b->current_index);
  if (!tab_a || !tab_b || tab_a->IsSplit() || tab_b->IsSplit()) {
    return base::unexpected(CommandError::kSplitPreconditionFailure);
  }
  ResolvedSplitTarget target;
  target.window = window;
  target.pane_a = pane_a;
  target.pane_b = pane_b;
  target.pane_a_index = resolved_a->current_index;
  target.pane_b_index = resolved_b->current_index;
  return target;
}

}  // namespace seoul
