import test from 'node:test';
import assert from 'node:assert/strict';

import { ACTION_KINDS } from '../src/protocol.ts';
import {
  ACTION_HANDLERS,
  REGISTERED_ACTION_KINDS,
  resolveActionHandler,
} from '../src/background/action-handlers.ts';

// Section A: typed action dispatch. Every action kind has exactly one handler,
// dispatch is by kind (not an if/else chain), and there is no fallback that
// reinterprets an unknown action as a valid one.

test('every action kind has exactly one registered handler', () => {
  for (const kind of ACTION_KINDS) {
    const handler = ACTION_HANDLERS[kind];
    assert.ok(handler, `missing handler for ${kind}`);
    assert.equal(handler.kind, kind, `handler for ${kind} declares kind ${handler.kind}`);
  }
  // The registry has no extra handlers beyond the closed union.
  assert.equal(REGISTERED_ACTION_KINDS.length, ACTION_KINDS.length);
  assert.deepEqual([...REGISTERED_ACTION_KINDS].sort(), [...ACTION_KINDS].sort());
});

test('each handler owns risk, approval, and retry metadata', () => {
  for (const kind of ACTION_KINDS) {
    const h = ACTION_HANDLERS[kind];
    assert.ok(['read_only', 'reversible', 'external_side_effect'].includes(h.risk));
    assert.ok(['never', 'first_use', 'always'].includes(h.approval));
    assert.equal(typeof h.retriableOnUnknownOutcome, 'boolean');
  }
});

test('a click action plans only a click content request', () => {
  const handler = resolveActionHandler({
    kind: 'CLICK_ELEMENT',
    elementId: 'seoul-1',
    documentToken: 'a'.repeat(32),
    snapshotId: 'snap-1',
  });
  assert.ok(handler);
  const plan = handler.plan('sess', {
    kind: 'CLICK_ELEMENT',
    elementId: 'seoul-1',
    documentToken: 'a'.repeat(32),
    snapshotId: 'snap-1',
  });
  assert.equal(plan.mode, 'content');
  assert.equal(plan.request.type, 'SEOUL_CLICK');
});

test('a type action plans only a type content request', () => {
  const plan = ACTION_HANDLERS.TYPE_TEXT.plan('sess', {
    kind: 'TYPE_TEXT',
    elementId: 'seoul-2',
    text: 'hello',
    documentToken: 'b'.repeat(32),
    snapshotId: 'snap-2',
  });
  assert.equal(plan.mode, 'content');
  assert.equal(plan.request.type, 'SEOUL_TYPE');
  assert.equal(plan.request.text, 'hello');
});

test('a scroll action plans only a scroll content request', () => {
  const plan = ACTION_HANDLERS.SCROLL_PAGE.plan('sess', {
    kind: 'SCROLL_PAGE',
    direction: 'down',
    amount: 600,
    documentToken: 'c'.repeat(32),
  });
  assert.equal(plan.mode, 'content');
  assert.equal(plan.request.type, 'SEOUL_SCROLL');
});

test('a navigate action plans a navigate step and rejects a bad url', () => {
  const good = ACTION_HANDLERS.NAVIGATE.plan('sess', {
    kind: 'NAVIGATE',
    url: 'https://example.test/',
  });
  assert.equal(good.mode, 'navigate');
  assert.equal(good.url, 'https://example.test/');

  const bad = ACTION_HANDLERS.NAVIGATE.plan('sess', {
    kind: 'NAVIGATE',
    url: 'javascript:alert(1)',
  });
  assert.equal(bad.mode, 'rejected');
  assert.equal(bad.error.code, 'NAVIGATION_REJECTED');
});

test('an unknown action kind resolves to no handler (explicit unsupported)', () => {
  // A kind outside the closed union has no handler; the controller turns this
  // into an explicit unsupported-action error rather than guessing.
  const handler = resolveActionHandler({ kind: 'DELETE_ACCOUNT' });
  assert.equal(handler, null);
});

test('a content mutation is not auto-retriable on unknown outcome; a read is', () => {
  assert.equal(ACTION_HANDLERS.CLICK_ELEMENT.retriableOnUnknownOutcome, false);
  assert.equal(ACTION_HANDLERS.TYPE_TEXT.retriableOnUnknownOutcome, false);
  assert.equal(ACTION_HANDLERS.NAVIGATE.retriableOnUnknownOutcome, false);
  assert.equal(ACTION_HANDLERS.SCROLL_PAGE.retriableOnUnknownOutcome, true);
});
