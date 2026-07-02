import test from 'node:test';
import assert from 'node:assert/strict';

import { handleContentRequest } from '../src/content/router.ts';

// Section A/G at the content boundary: the operation switch is exhaustive with
// an explicit unsupported default and never reinterprets an unknown request.
// These paths are exercised with a fake per-document state, so no DOM is
// needed; the click/type/scroll dispatch itself is proven at the background
// boundary in action-handlers.test.mjs (each kind plans only its own request).

function fakeState(overrides = {}) {
  return {
    registry: { idToEl: new Map(), elToId: new WeakMap(), counter: 0 },
    listenerInstalled: false,
    sessionId: null,
    documentToken: 'd'.repeat(32),
    snapshotId: null,
    initializedAt: 0,
    ...overrides,
  };
}

test('operations before init are refused', () => {
  const state = fakeState();
  const r = handleContentRequest(state, { type: 'SEOUL_OBSERVE', sessionId: 's' });
  assert.equal(r.ok, false);
  assert.equal(r.code, 'ACTION_FAILED');
  assert.match(r.message, /not initialized/);
});

test('SEOUL_INIT sets the session and returns the document token', () => {
  const state = fakeState();
  const r = handleContentRequest(state, { type: 'SEOUL_INIT', sessionId: 'sess-1' });
  assert.equal(r.ok, true);
  assert.equal(state.sessionId, 'sess-1');
  assert.equal(r.data.documentToken, state.documentToken);
});

test('a request for a different session is refused', () => {
  const state = fakeState({ sessionId: 'sess-1' });
  const r = handleContentRequest(state, { type: 'SEOUL_OBSERVE', sessionId: 'other' });
  assert.equal(r.ok, false);
  assert.equal(r.code, 'ACTION_FAILED');
  assert.match(r.message, /does not match/);
});

test('an unsupported content request type is rejected, never reinterpreted', () => {
  const state = fakeState({ sessionId: 'sess-1' });
  const r = handleContentRequest(state, { type: 'SEOUL_FROB', sessionId: 'sess-1' });
  assert.equal(r.ok, false);
  assert.equal(r.code, 'INVALID_MESSAGE');
});
