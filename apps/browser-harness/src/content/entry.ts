// Project Seoul Development Harness - content script entry (bundled).
//
// Injected only after an explicit user gesture (Start session) via
// chrome.scripting into Chrome's isolated world. It shares the DOM but not the
// page's JavaScript context, never listens for page postMessage, and never
// exposes privileged functions to the page.
//
// This entry and the ./content modules it imports are bundled by esbuild into a
// single classic content-script artifact (content.js). Bundling is what lets
// the content script consume the shared validation source of truth
// (../validation.ts) directly instead of mirroring it, so there is exactly one
// tested implementation of the limits, field-safety, and snapshot-binding rules.

import type { ContentRequest, ContentResult } from '../protocol.ts';
import { getState } from './document-session.ts';
import { handleContentRequest } from './router.ts';

const state = getState();

function onMessage(
  message: unknown,
  sender: chrome.runtime.MessageSender,
  sendResponse: (response: ContentResult) => void,
): boolean {
  // Accept only messages from this extension's own runtime (the background
  // worker). Reject anything originating from a tab/content context.
  if (!sender || sender.id !== chrome.runtime.id || sender.tab) {
    sendResponse({ ok: false, code: 'INVALID_MESSAGE', message: 'Untrusted sender.' });
    return true;
  }
  if (
    !message ||
    typeof message !== 'object' ||
    typeof (message as { type?: unknown }).type !== 'string'
  ) {
    sendResponse({ ok: false, code: 'INVALID_MESSAGE', message: 'Malformed content request.' });
    return true;
  }
  try {
    sendResponse(handleContentRequest(state, message as ContentRequest));
  } catch (e) {
    sendResponse({
      ok: false,
      code: 'ACTION_FAILED',
      message: e instanceof Error ? e.message : String(e),
    });
  }
  return true;
}

if (!state.listenerInstalled) {
  chrome.runtime.onMessage.addListener(onMessage);
  state.listenerInstalled = true;
}
