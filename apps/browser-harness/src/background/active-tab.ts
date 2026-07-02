// Project Seoul Development Harness - active-tab resolution.
//
// Resolves the tab a global side panel should act on. It prefers the window of
// the most recent toolbar invocation (from the validated PanelInvocationContext
// store) so the panel never resolves an unrelated window's active tab. The
// stored context is only a WINDOW HINT: it is never treated as an access grant,
// and it is fully revalidated (tab exists, still in that window) before use.

import type { PanelInvocationContextStore } from './panel-invocation-context.ts';

async function tabStillInWindow(tabId: number): Promise<{ windowId: number } | null> {
  const tab = await chrome.tabs.get(tabId).catch((): chrome.tabs.Tab | null => null);
  if (!tab || tab.id == null || tab.windowId == null) return null;
  return { windowId: tab.windowId };
}

export async function resolveActiveTab(
  invocations: PanelInvocationContextStore,
): Promise<chrome.tabs.Tab | null> {
  const hint = await invocations.resolveValid(tabStillInWindow);
  if (hint) {
    const inWindow = await chrome.tabs
      .query({ active: true, windowId: hint.windowId })
      .catch((): chrome.tabs.Tab[] => []);
    if (inWindow[0]) return inWindow[0];
  }
  const tabs = await chrome.tabs
    .query({ active: true, lastFocusedWindow: true })
    .catch((): chrome.tabs.Tab[] => []);
  return tabs[0] ?? null;
}
