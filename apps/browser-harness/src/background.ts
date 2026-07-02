// Project Seoul Development Harness - background service worker entry.
//
// This is the composition root and event-registration surface only. Each
// responsibility lives in a focused module under ./background/: one-time setup,
// the panel-invocation context store (browser-session state), live session
// bindings (per-worker ephemeral state), content-script transport, session
// recovery, the typed action handler registry, the session controller, the
// trusted request router, and tab/window lifecycle. The entry constructs the
// stores once, wires the listeners, and enforces the trust boundary; it holds
// no free mutable state of its own.
//
// A web page sending a message can never cause an action: only the extension's
// own side panel page is accepted as a request sender.

import type { ErrorCode, ProtocolResponse } from './protocol.ts';
import { LiveSessionStore } from './background/live-sessions.ts';
import { PanelInvocationContextStore } from './background/panel-invocation-context.ts';
import { SessionController } from './background/session-controller.ts';
import { routeRequest } from './background/request-router.ts';
import { registerTabLifecycle } from './background/tab-lifecycle.ts';
import { configurePanelBehavior, restrictStorageAccess } from './background/setup.ts';

// Owned state (see each module header for the full ownership contract):
//   live         - per-worker ephemeral session bindings, not recovered.
//   invocations  - browser-session panel-invocation context, storage.session.
const live = new LiveSessionStore();
const invocations = new PanelInvocationContextStore();
const controller = new SessionController(live, invocations);

function toMessage(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

function staticError(code: ErrorCode, message: string): ProtocolResponse {
  return {
    id: 'unknown',
    success: false,
    timestamp: Date.now(),
    sessionId: 'unknown',
    error: { code, message },
  };
}

// Only the extension's own side panel page may issue protocol requests.
function isTrustedSidePanelSender(sender: chrome.runtime.MessageSender): boolean {
  if (!sender || sender.id !== chrome.runtime.id) return false;
  if (sender.tab) return false; // came from a tab / content-script context
  const url = sender.url ?? '';
  return url.startsWith(chrome.runtime.getURL('sidepanel/'));
}

// One-time setup (persists across restarts).
chrome.runtime.onInstalled.addListener(() => {
  void configurePanelBehavior();
  void restrictStorageAccess();
});
chrome.runtime.onStartup.addListener(() => {
  void configurePanelBehavior();
  void restrictStorageAccess();
});

// Explicit action invocation. The clicked tab arrives with a fresh activeTab
// grant; record exactly that tab (window hint only, never a grant) and open the
// side panel for it.
chrome.action.onClicked.addListener((tab) => {
  if (tab.id == null || tab.windowId == null) return;
  void invocations.record({ tabId: tab.id, windowId: tab.windowId });
  if (chrome.sidePanel && typeof chrome.sidePanel.open === 'function') {
    void chrome.sidePanel.open({ tabId: tab.id }).catch(() => {});
  }
});

// Navigation / tab / window lifecycle invalidation.
registerTabLifecycle(live, invocations);

// Trust boundary and request dispatch. Broadly compatible async pattern:
// non-async listener, call sendResponse later, return literal true. Exactly one
// listener owns requests.
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (!isTrustedSidePanelSender(sender)) {
    sendResponse(staticError('INVALID_MESSAGE', 'Rejected request from an untrusted sender.'));
    return true;
  }
  routeRequest(controller, message)
    .then(sendResponse)
    .catch((e: unknown) => sendResponse(staticError('ACTION_FAILED', toMessage(e))));
  return true;
});
