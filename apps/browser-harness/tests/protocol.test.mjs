import test from 'node:test';
import assert from 'node:assert/strict';

import { parseRequest, validateAction } from '../src/validation.ts';
import { makeSuccess, makeError, errorOf, REQUEST_KINDS } from '../src/protocol.ts';

const DOC = 'dtok-0123456789abcdef';
const SNAP = 'snap-0123456789abcdef';

test('parses a valid SESSION_START request', () => {
  const result = parseRequest({ kind: 'SESSION_START', id: 'r1', sessionId: 's1', tabId: 7 });
  assert.equal(result.ok, true);
  assert.equal(result.request.kind, 'SESSION_START');
  assert.equal(result.request.tabId, 7);
});

test('parses a valid OBSERVE_PAGE request', () => {
  const result = parseRequest({ kind: 'OBSERVE_PAGE', id: 'r2', sessionId: 's1' });
  assert.equal(result.ok, true);
  assert.equal(result.request.kind, 'OBSERVE_PAGE');
});

test('parses a valid SCROLL_PAGE action and clamps the amount', () => {
  const result = parseRequest({
    kind: 'EXECUTE_ACTION',
    id: 'r3',
    sessionId: 's1',
    action: { kind: 'SCROLL_PAGE', direction: 'down', amount: 9_999_999, documentToken: DOC },
  });
  assert.equal(result.ok, true);
  assert.equal(result.request.action.kind, 'SCROLL_PAGE');
  assert.equal(result.request.action.amount, 20_000);
  assert.equal(result.request.action.documentToken, DOC);
});

test('parses valid CLICK_ELEMENT and TYPE_TEXT actions carrying a binding', () => {
  const click = validateAction({ kind: 'CLICK_ELEMENT', elementId: 'seoul-1', documentToken: DOC, snapshotId: SNAP });
  assert.equal(click.ok, true);
  assert.equal(click.action.documentToken, DOC);
  assert.equal(click.action.snapshotId, SNAP);

  const type = validateAction({ kind: 'TYPE_TEXT', elementId: 'seoul-1', text: 'hi', documentToken: DOC, snapshotId: SNAP });
  assert.equal(type.ok, true);
  assert.equal(type.action.text, 'hi');
});

test('rejects element actions that are missing their snapshot binding', () => {
  assert.equal(validateAction({ kind: 'CLICK_ELEMENT', elementId: 'seoul-1', snapshotId: SNAP }).ok, false);
  assert.equal(validateAction({ kind: 'CLICK_ELEMENT', elementId: 'seoul-1', documentToken: DOC }).ok, false);
  assert.equal(validateAction({ kind: 'TYPE_TEXT', elementId: 'seoul-1', text: 'x', documentToken: DOC }).ok, false);
  assert.equal(validateAction({ kind: 'SCROLL_PAGE', direction: 'up', amount: 10 }).ok, false);
});

test('rejects an unknown request kind', () => {
  const result = parseRequest({ kind: 'DO_EVERYTHING', id: 'r4', sessionId: 's1' });
  assert.equal(result.ok, false);
  assert.equal(result.error.code, 'INVALID_MESSAGE');
});

test('rejects a request that is missing a required field', () => {
  const noSession = parseRequest({ kind: 'OBSERVE_PAGE', id: 'r5' });
  assert.equal(noSession.ok, false);
  assert.equal(noSession.error.code, 'INVALID_MESSAGE');

  const noTabId = parseRequest({ kind: 'SESSION_START', id: 'r6', sessionId: 's1' });
  assert.equal(noTabId.ok, false);
  assert.equal(noTabId.error.code, 'INVALID_MESSAGE');
});

test('rejects a request with a wrong field type', () => {
  assert.equal(parseRequest({ kind: 'OBSERVE_PAGE', id: 123, sessionId: 's1' }).ok, false);
  assert.equal(parseRequest({ kind: 'SESSION_START', id: 'r7', sessionId: 's1', tabId: '7' }).ok, false);
});

test('rejects an unsupported action', () => {
  const result = parseRequest({
    kind: 'EXECUTE_ACTION',
    id: 'r8',
    sessionId: 's1',
    action: { kind: 'EVAL', code: 'doThings()' },
  });
  assert.equal(result.ok, false);
  assert.equal(result.error.code, 'INVALID_MESSAGE');
  assert.equal(validateAction({ kind: 'RUN_SHELL', cmd: 'rm -rf /' }).ok, false);
});

test('rejects a non-object request', () => {
  assert.equal(parseRequest(null).ok, false);
  assert.equal(parseRequest('hello').ok, false);
});

test('parses a valid GET_PANEL_CONTEXT request', () => {
  const result = parseRequest({ kind: 'GET_PANEL_CONTEXT', id: 'r', sessionId: 's' });
  assert.equal(result.ok, true);
  assert.equal(result.request.kind, 'GET_PANEL_CONTEXT');
});

test('GET_PANEL_CONTEXT cannot supply an arbitrary tab id', () => {
  const result = parseRequest({ kind: 'GET_PANEL_CONTEXT', id: 'r', sessionId: 's', tabId: 999 });
  assert.equal(result.ok, true);
  assert.equal('tabId' in result.request, false);
});

test('rejects an invalid GET_PANEL_CONTEXT payload', () => {
  assert.equal(parseRequest({ kind: 'GET_PANEL_CONTEXT', id: 'r' }).ok, false);
  assert.equal(parseRequest({ kind: 'GET_PANEL_CONTEXT', sessionId: 's' }).ok, false);
});

test('every known request kind is recognized by the parser', () => {
  for (const kind of REQUEST_KINDS) {
    const base = { kind, id: 'r', sessionId: 's' };
    if (kind === 'SESSION_START') base.tabId = 1;
    if (kind === 'EXECUTE_ACTION') {
      base.action = { kind: 'SCROLL_PAGE', direction: 'up', amount: 100, documentToken: DOC };
    }
    const result = parseRequest(base);
    assert.equal(result.ok, true, `expected ${kind} to parse`);
  }
});

test('response builders produce the required response shape', () => {
  const success = makeSuccess({ id: 'r9', sessionId: 's1' }, { hello: 'world' }, 42);
  assert.equal(success.id, 'r9');
  assert.equal(success.success, true);
  assert.equal(success.sessionId, 's1');
  assert.equal(success.tabId, 42);
  assert.equal(typeof success.timestamp, 'number');
  assert.deepEqual(success.result, { hello: 'world' });

  const failure = makeError({ id: 'r10', sessionId: 's1' }, errorOf('ACTION_FAILED', 'nope'), 42);
  assert.equal(failure.success, false);
  assert.equal(failure.error.code, 'ACTION_FAILED');
  assert.equal(failure.error.message, 'nope');
  assert.equal(typeof failure.timestamp, 'number');
});
