// Project Seoul Development Harness - background/content-script transport.
//
// The one place that talks to a content script. It owns no session state; it
// maps chrome.tabs.sendMessage into a typed ContentResult and normalizes the
// SEOUL_PROBE reply into ProbeData. Callers decide what a result means.

import type { ContentRequest, ContentResult, ProbeData } from '../protocol.ts';

export function sendToContent(tabId: number, msg: ContentRequest): Promise<ContentResult> {
  return new Promise((resolve) => {
    chrome.tabs.sendMessage(tabId, msg, (resp: unknown) => {
      if (chrome.runtime.lastError) {
        resolve({
          ok: false,
          code: 'INJECTION_FAILED',
          message: chrome.runtime.lastError.message ?? 'Content script is not reachable.',
        });
        return;
      }
      if (!resp || typeof resp !== 'object') {
        resolve({ ok: false, code: 'ACTION_FAILED', message: 'Empty content-script response.' });
        return;
      }
      resolve(resp as ContentResult);
    });
  });
}

export async function probeContent(tabId: number, sessionId: string): Promise<ProbeData | null> {
  const cr = await sendToContent(tabId, { type: 'SEOUL_PROBE', sessionId });
  if (!cr.ok) return null;
  const d = cr.data as Partial<ProbeData> | undefined;
  if (!d || typeof d !== 'object') return null;
  return {
    sessionId: typeof d.sessionId === 'string' ? d.sessionId : null,
    documentToken: typeof d.documentToken === 'string' ? d.documentToken : '',
    url: typeof d.url === 'string' ? d.url : '',
    initialized: d.initialized === true,
    snapshotId: typeof d.snapshotId === 'string' ? d.snapshotId : null,
  };
}
