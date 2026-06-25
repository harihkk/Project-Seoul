import test from 'node:test';
import assert from 'node:assert/strict';

import {
  isHttpUrl,
  validateNavigationUrl,
  clampScroll,
  validateTypeText,
  classifyTypeTarget,
  truncateTimeline,
  checkSnapshotBinding,
  isValidDocumentToken,
  LIMITS,
} from '../src/validation.ts';

function field(overrides = {}) {
  return {
    tag: 'input',
    type: 'text',
    disabled: false,
    readOnly: false,
    connected: true,
    visible: true,
    valueLength: 0,
    ...overrides,
  };
}

test('accepts absolute http and https URLs', () => {
  assert.equal(isHttpUrl('http://example.com'), true);
  assert.equal(isHttpUrl('https://example.com/path?q=1'), true);

  const http = validateNavigationUrl('http://example.com');
  assert.equal(http.ok, true);
  const https = validateNavigationUrl('https://example.com/x');
  assert.equal(https.ok, true);
});

test('rejects dangerous and non-absolute URL schemes', () => {
  for (const bad of [
    'javascript:alert(1)',
    'data:text/html,<h1>x</h1>',
    'file:///etc/passwd',
    'chrome://settings',
    'about:blank',
    '/relative/path',
    'example.com',
    '',
  ]) {
    const result = validateNavigationUrl(bad);
    assert.equal(result.ok, false, `expected ${JSON.stringify(bad)} to be rejected`);
    assert.equal(result.error.code, 'NAVIGATION_REJECTED');
    assert.equal(isHttpUrl(bad), false);
  }
});

test('validates typed text and enforces a maximum length', () => {
  assert.equal(validateTypeText('hello').ok, true);
  assert.equal(validateTypeText('').ok, true);
  assert.equal(validateTypeText(123).ok, false);
  assert.equal(validateTypeText({}).ok, false);
  assert.equal(validateTypeText('x'.repeat(LIMITS.MAX_TYPE_TEXT_CHARS + 1)).ok, false);
});

test('clamps scroll amounts to a safe range', () => {
  assert.equal(clampScroll(-100), LIMITS.SCROLL_MIN);
  assert.equal(clampScroll(9_999_999), LIMITS.SCROLL_MAX);
  assert.equal(clampScroll(500), 500);
  assert.equal(clampScroll(Number.NaN), LIMITS.SCROLL_DEFAULT);
  assert.equal(clampScroll('not a number'), LIMITS.SCROLL_DEFAULT);
  assert.equal(clampScroll(123.7), 124);
});

test('classifies sensitive and non-typeable fields', () => {
  assert.equal(classifyTypeTarget(field({ type: 'text' })).allowed, true);
  assert.equal(classifyTypeTarget(field({ tag: 'textarea', type: '' })).allowed, true);

  const password = classifyTypeTarget(field({ type: 'password' }));
  assert.equal(password.allowed, false);
  assert.equal(password.error.code, 'SENSITIVE_FIELD');

  const file = classifyTypeTarget(field({ type: 'file' }));
  assert.equal(file.error.code, 'SENSITIVE_FIELD');

  const hidden = classifyTypeTarget(field({ type: 'hidden' }));
  assert.equal(hidden.error.code, 'ELEMENT_NOT_INTERACTABLE');

  const checkbox = classifyTypeTarget(field({ type: 'checkbox' }));
  assert.equal(checkbox.error.code, 'ELEMENT_NOT_INTERACTABLE');

  const disabled = classifyTypeTarget(field({ disabled: true }));
  assert.equal(disabled.error.code, 'ELEMENT_NOT_INTERACTABLE');

  const readonly = classifyTypeTarget(field({ readOnly: true }));
  assert.equal(readonly.error.code, 'ELEMENT_NOT_INTERACTABLE');

  const detached = classifyTypeTarget(field({ connected: false }));
  assert.equal(detached.error.code, 'ELEMENT_NOT_FOUND');

  const invisible = classifyTypeTarget(field({ visible: false }));
  assert.equal(invisible.error.code, 'ELEMENT_NOT_INTERACTABLE');

  const nonEmpty = classifyTypeTarget(field({ valueLength: 5 }));
  assert.equal(nonEmpty.allowed, false);
  assert.equal(nonEmpty.error.code, 'ACTION_FAILED');

  const notField = classifyTypeTarget(field({ tag: 'div', type: '' }));
  assert.equal(notField.error.code, 'ELEMENT_NOT_INTERACTABLE');
});

test('truncates the timeline to the most recent events', () => {
  const events = Array.from({ length: LIMITS.MAX_TIMELINE_EVENTS + 1 }, (_, i) => ({
    at: i,
    type: 'E',
  }));
  const truncated = truncateTimeline(events);
  assert.equal(truncated.length, LIMITS.MAX_TIMELINE_EVENTS);
  // The oldest event (at: 0) is dropped; the newest is kept.
  assert.equal(truncated[0].at, 1);
  assert.equal(truncated[truncated.length - 1].at, LIMITS.MAX_TIMELINE_EVENTS);

  const short = truncateTimeline([{ at: 1, type: 'E' }]);
  assert.equal(short.length, 1);
});

test('exposes the centralized observation limits', () => {
  assert.equal(LIMITS.MAX_INTERACTIVE_ELEMENTS, 200);
  assert.equal(LIMITS.MAX_TEXT_BLOCKS, 100);
  assert.equal(LIMITS.MAX_TOTAL_TEXT_CHARS, 30_000);
  assert.equal(LIMITS.MAX_TIMELINE_EVENTS, 200);
});

test('validates document token shape', () => {
  assert.equal(isValidDocumentToken('dtok-0123456789abcdef'), true);
  assert.equal(isValidDocumentToken(''), false);
  assert.equal(isValidDocumentToken('short'), false);
  assert.equal(isValidDocumentToken('x'.repeat(200)), false);
  assert.equal(isValidDocumentToken(42), false);
});

test('snapshot binding accepts a matching document token and snapshot id', () => {
  const current = { documentToken: 'doc-1', snapshotId: 'snap-1' };
  assert.equal(checkSnapshotBinding(current, { documentToken: 'doc-1', snapshotId: 'snap-1' }, true).ok, true);
});

test('snapshot binding rejects a stale document token', () => {
  const current = { documentToken: 'doc-1', snapshotId: 'snap-1' };
  const r = checkSnapshotBinding(current, { documentToken: 'doc-OTHER', snapshotId: 'snap-1' }, true);
  assert.equal(r.ok, false);
  assert.equal(r.error.code, 'STALE_DOCUMENT');
});

test('snapshot binding rejects a stale snapshot id', () => {
  const current = { documentToken: 'doc-1', snapshotId: 'snap-NEW' };
  const r = checkSnapshotBinding(current, { documentToken: 'doc-1', snapshotId: 'snap-OLD' }, true);
  assert.equal(r.ok, false);
  assert.equal(r.error.code, 'STALE_SNAPSHOT');
});

test('snapshot binding rejects an element action when no snapshot exists', () => {
  const current = { documentToken: 'doc-1', snapshotId: null };
  const r = checkSnapshotBinding(current, { documentToken: 'doc-1', snapshotId: 'snap-1' }, true);
  assert.equal(r.ok, false);
  assert.equal(r.error.code, 'STALE_SNAPSHOT');
});

test('scroll binding checks only the document token, not the snapshot', () => {
  const current = { documentToken: 'doc-1', snapshotId: null };
  assert.equal(checkSnapshotBinding(current, { documentToken: 'doc-1' }, false).ok, true);
  assert.equal(checkSnapshotBinding(current, { documentToken: 'doc-2' }, false).error.code, 'STALE_DOCUMENT');
});
