// Project Seoul Development Harness - content: request router.
//
// Routes a background-to-content request to the right handler over the current
// document state. SEOUL_INIT/SEOUL_PROBE are always available; all other
// operations require the content script to be initialized for the exact session
// the background addresses. The operation switch is exhaustive over the closed
// ContentRequest union with an explicit unsupported default - it never
// reinterprets an unknown request as another operation.

import type { ContentRequest, ContentResult } from '../protocol.ts';
import { freshRegistry, type HarnessState } from './document-session.ts';
import { observe } from './observation.ts';
import { clickElement, scrollPage, typeText } from './actions.ts';

function initSession(state: HarnessState, sessionId: string): ContentResult {
  state.sessionId = sessionId;
  state.snapshotId = null;
  state.registry = freshRegistry();
  state.initializedAt = Date.now();
  return { ok: true, data: { documentToken: state.documentToken } };
}

function probe(state: HarnessState): ContentResult {
  return {
    ok: true,
    data: {
      sessionId: state.sessionId,
      documentToken: state.documentToken,
      url: location.href,
      initialized: state.sessionId !== null,
      snapshotId: state.snapshotId,
    },
  };
}

export function handleContentRequest(
  state: HarnessState,
  message: ContentRequest,
): ContentResult {
  if (message.type === 'SEOUL_INIT') return initSession(state, message.sessionId);
  if (message.type === 'SEOUL_PROBE') return probe(state);

  if (state.sessionId === null) {
    return { ok: false, code: 'ACTION_FAILED', message: 'Content script is not initialized.' };
  }
  if (message.sessionId !== state.sessionId) {
    return { ok: false, code: 'ACTION_FAILED', message: 'Session id does not match this content script.' };
  }

  switch (message.type) {
    case 'SEOUL_OBSERVE':
      return { ok: true, data: observe(state) };
    case 'SEOUL_CLICK':
      return clickElement(state, message.elementId, message.documentToken, message.snapshotId);
    case 'SEOUL_TYPE':
      return typeText(state, message.elementId, message.text, message.documentToken, message.snapshotId);
    case 'SEOUL_SCROLL':
      return scrollPage(state, message.direction, message.amount, message.documentToken);
    default:
      return { ok: false, code: 'INVALID_MESSAGE', message: 'Unsupported content request.' };
  }
}
