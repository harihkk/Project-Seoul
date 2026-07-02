// Project Seoul Development Harness - tab/window lifecycle invalidation.
//
// Reconciles live bindings, durable records, and the panel-invocation context
// when tabs load, close, or are replaced and when windows close. It owns no
// state; it mutates the injected LiveSessionStore, the control-session store,
// and the invocation-context store, then leaves. changeInfo.status and the
// tab/window removal events need no extra permission.

import * as store from '../control-session-store.ts';
import type { LiveSessionStore } from './live-sessions.ts';
import type { PanelInvocationContextStore } from './panel-invocation-context.ts';

async function reconcileInvalidated(
  affectedSessionIds: readonly string[],
  reason: string,
): Promise<void> {
  for (const sessionId of affectedSessionIds) {
    try {
      const rec = await store.getRecord(sessionId);
      if (rec && rec.state !== 'STOPPED' && rec.state !== 'FAILED') {
        await store.setState(sessionId, 'STOPPED', 'NAVIGATED', reason);
      }
    } catch {
      // Best effort: the live binding is already gone.
    }
  }
}

async function invalidateTabSessions(
  live: LiveSessionStore,
  tabId: number,
  reason: string,
): Promise<void> {
  const affected = live.invalidateTab(tabId);
  await reconcileInvalidated(affected, reason);
}

// Registers the lifecycle listeners against the injected stores.
export function registerTabLifecycle(
  live: LiveSessionStore,
  invocations: PanelInvocationContextStore,
): void {
  chrome.tabs.onUpdated.addListener((tabId, changeInfo) => {
    // A top-level load means the bound document is being replaced.
    if (changeInfo.status === 'loading') {
      void invalidateTabSessions(live, tabId, 'Tab began loading a new document; session invalidated.');
    }
  });
  chrome.tabs.onRemoved.addListener((tabId) => {
    void invalidateTabSessions(live, tabId, 'Tab was closed; session stopped.');
    void invocations.onTabRemoved(tabId);
  });
  chrome.tabs.onReplaced.addListener((_addedTabId, removedTabId) => {
    void invalidateTabSessions(live, removedTabId, 'Tab was replaced; session stopped.');
    void invocations.onTabRemoved(removedTabId);
  });
  if (chrome.windows && chrome.windows.onRemoved) {
    chrome.windows.onRemoved.addListener((windowId) => {
      void invocations.onWindowRemoved(windowId);
    });
  }
}
