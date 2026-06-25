import test from 'node:test';
import assert from 'node:assert/strict';

import {
  pickLatestSessionForTab,
  classifyActiveSession,
  wasReconciled,
  evaluateProbe,
  planRecovery,
  resolveAccess,
} from '../src/session.ts';

const RECOVER = { outcome: 'recover' };
const STOP = { outcome: 'stop', reason: 'content gone' };

const ORIGIN = 'http://localhost:8765';
const DOC = 'dtok-0123456789abcdef';

function rec(over = {}) {
  return {
    sessionId: 's',
    tabId: 1,
    origin: ORIGIN,
    state: 'READY',
    createdAt: 0,
    updatedAt: 0,
    timeline: [],
    lastError: null,
    ...over,
  };
}

function probe(over = {}) {
  return {
    sessionId: 's',
    documentToken: DOC,
    url: `${ORIGIN}/interactive-page.html`,
    initialized: true,
    snapshotId: null,
    ...over,
  };
}

test('no stored session for the tab yields no selection (IDLE-equivalent)', () => {
  assert.equal(pickLatestSessionForTab([], 1), null);
  const c = classifyActiveSession(null, false);
  assert.equal(c.session, null);
  assert.equal(c.live, false);
  assert.equal(c.needsProbe, false);
});

test('a live matching non-terminal session is restored as live without a probe', () => {
  const c = classifyActiveSession(rec({ state: 'READY' }), true);
  assert.equal(c.live, true);
  assert.equal(c.needsProbe, false);
});

test('a non-terminal session without live state needs a probe', () => {
  const c = classifyActiveSession(rec({ state: 'OBSERVING' }), false);
  assert.equal(c.live, false);
  assert.equal(c.needsProbe, true);
});

test('a terminal session is never probed and stays terminal', () => {
  for (const state of ['STOPPED', 'FAILED']) {
    const c = classifyActiveSession(rec({ state }), false);
    assert.equal(c.needsProbe, false);
    assert.equal(c.live, false);
    assert.equal(c.session.state, state);
  }
});

test('the most recently updated matching session is selected, deterministically', () => {
  const older = rec({ sessionId: 'a', tabId: 1, updatedAt: 100 });
  const newer = rec({ sessionId: 'b', tabId: 1, updatedAt: 200 });
  assert.equal(pickLatestSessionForTab([older, newer], 1).sessionId, 'b');
  assert.equal(pickLatestSessionForTab([newer, older], 1).sessionId, 'b');

  const t1 = rec({ sessionId: 'a', tabId: 1, updatedAt: 200 });
  const t2 = rec({ sessionId: 'b', tabId: 1, updatedAt: 200 });
  assert.equal(pickLatestSessionForTab([t1, t2], 1).sessionId, 'b');
  assert.equal(pickLatestSessionForTab([t2, t1], 1).sessionId, 'b');
});

test('a session belonging to another tab is never returned', () => {
  const other = rec({ sessionId: 'x', tabId: 2, updatedAt: 999 });
  assert.equal(pickLatestSessionForTab([other], 1), null);
  const mixed = pickLatestSessionForTab([other, rec({ sessionId: 'y', tabId: 1, updatedAt: 1 })], 1);
  assert.equal(mixed.sessionId, 'y');
});

test('separate sessions are not merged', () => {
  const a = rec({ sessionId: 'a', tabId: 1, updatedAt: 100, timeline: [{ at: 1, type: 'A' }] });
  const b = rec({ sessionId: 'b', tabId: 1, updatedAt: 200, timeline: [{ at: 2, type: 'B' }] });
  const picked = pickLatestSessionForTab([a, b], 1);
  assert.equal(picked.sessionId, 'b');
  assert.deepEqual(picked.timeline, [{ at: 2, type: 'B' }]);
});

test('a restored session record carries no ephemeral element ids or snapshot', () => {
  const c = classifyActiveSession(rec({ state: 'READY' }), true);
  assert.equal('elements' in c.session, false);
  assert.equal('snapshot' in c.session, false);
  assert.equal('textBlocks' in c.session, false);
});

test('a valid probe recovers the session', () => {
  const verdict = evaluateProbe(probe(), rec());
  assert.equal(verdict.outcome, 'recover');
});

test('a missing probe response stops the session', () => {
  const verdict = evaluateProbe(null, rec());
  assert.equal(verdict.outcome, 'stop');
  assert.match(verdict.reason, /did not respond/i);
});

test('an uninitialized content script stops the session', () => {
  assert.equal(evaluateProbe(probe({ initialized: false }), rec()).outcome, 'stop');
});

test('a mismatched session id stops the session', () => {
  assert.equal(evaluateProbe(probe({ sessionId: 'other' }), rec()).outcome, 'stop');
});

test('a malformed document token stops the session', () => {
  assert.equal(evaluateProbe(probe({ documentToken: '' }), rec()).outcome, 'stop');
  assert.equal(evaluateProbe(probe({ documentToken: 'short' }), rec()).outcome, 'stop');
});

test('navigation to an incompatible origin stops the session', () => {
  const verdict = evaluateProbe(probe({ url: 'https://evil.example.com/page' }), rec());
  assert.equal(verdict.outcome, 'stop');
  assert.match(verdict.reason, /origin/i);
});

test('wasReconciled reflects a reconciliation event in the timeline', () => {
  assert.equal(wasReconciled(rec({ timeline: [] })), false);
  assert.equal(wasReconciled(rec({ timeline: [{ at: 1, type: 'RECONCILED_ON_STARTUP' }] })), true);
  assert.equal(wasReconciled(null), false);
});

// --- Transient-state recovery policy (planRecovery) ---

test('READY recovers to READY with SESSION_RECOVERED', () => {
  const plan = planRecovery(rec({ state: 'READY' }), RECOVER);
  assert.equal(plan.outcome, 'recover');
  assert.equal(plan.toState, 'READY');
  assert.equal(plan.event, 'SESSION_RECOVERED');
  assert.equal(plan.clearSnapshot, false);
  assert.equal(plan.error, null);
});

test('STARTING with a valid probe recovers to READY', () => {
  const plan = planRecovery(rec({ state: 'STARTING' }), RECOVER);
  assert.equal(plan.outcome, 'recover');
  assert.equal(plan.toState, 'READY');
  assert.equal(plan.event, 'SESSION_RECOVERED');
  assert.match(plan.detail, /initialization was interrupted/i);
});

test('STARTING without a valid probe becomes STOPPED', () => {
  const plan = planRecovery(rec({ state: 'STARTING' }), STOP);
  assert.equal(plan.outcome, 'stop');
  assert.equal(plan.toState, 'STOPPED');
  assert.equal(plan.event, 'RECONCILED_ON_STARTUP');
});

test('OBSERVING recovers to READY and invalidates its snapshot', () => {
  const plan = planRecovery(rec({ state: 'OBSERVING' }), RECOVER);
  assert.equal(plan.outcome, 'recover');
  assert.equal(plan.toState, 'READY');
  assert.equal(plan.event, 'OBSERVATION_INTERRUPTED');
  assert.equal(plan.clearSnapshot, true);
});

test('EXECUTING becomes STOPPED with ACTION_OUTCOME_UNKNOWN regardless of probe', () => {
  for (const verdict of [RECOVER, STOP]) {
    const plan = planRecovery(rec({ state: 'EXECUTING', pendingAction: 'CLICK_ELEMENT' }), verdict);
    assert.equal(plan.outcome, 'stop');
    assert.equal(plan.toState, 'STOPPED');
    assert.equal(plan.event, 'ACTION_OUTCOME_UNKNOWN');
    assert.equal(plan.error.code, 'ACTION_OUTCOME_UNKNOWN');
  }
});

test('an interrupted action is reported by kind only and never replayed', () => {
  const plan = planRecovery(rec({ state: 'EXECUTING', pendingAction: 'TYPE_TEXT' }), RECOVER);
  // Stopped, not recovered to READY: the background does not re-run the action.
  assert.equal(plan.outcome, 'stop');
  assert.match(plan.detail, /TYPE_TEXT/);
  assert.match(plan.error.message, /not retry/i);
  // The plan carries no payload, value, or replay instruction.
  const serialized = JSON.stringify(plan);
  assert.equal(serialized.includes('replay'), false);
  assert.equal(serialized.includes('value'), false);
});

test('repeated recovery is gated: a recovered or stopped session is not probed again', () => {
  // After recovery the session is live -> no further probe.
  assert.equal(classifyActiveSession(rec({ state: 'READY' }), true).needsProbe, false);
  // After an interrupted EXECUTING it is terminal -> no further probe.
  assert.equal(classifyActiveSession(rec({ state: 'STOPPED' }), false).needsProbe, false);
});

// --- Active-tab access resolution (resolveAccess) ---

test('a granted HTTP/HTTPS tab returns real title and origin', () => {
  const http = resolveAccess({ tabId: 7, url: 'http://localhost:8765/interactive-page.html', title: 'Fixture' });
  assert.equal(http.access, 'GRANTED');
  assert.equal(http.accessGranted, true);
  assert.equal(http.supportedScheme, true);
  assert.equal(http.title, 'Fixture');
  assert.equal(http.origin, 'http://localhost:8765');

  const https = resolveAccess({ tabId: 8, url: 'https://example.com/x', title: 'Ex' });
  assert.equal(https.access, 'GRANTED');
  assert.equal(https.origin, 'https://example.com');
});

test('no readable URL is ACCESS_REQUIRED, not UNSUPPORTED_PAGE', () => {
  const undef = resolveAccess({ tabId: 7, url: undefined, title: undefined });
  assert.equal(undef.access, 'ACCESS_REQUIRED');
  assert.equal(undef.accessGranted, false);
  assert.equal(undef.title, null);
  assert.equal(undef.origin, null);
  assert.match(undef.reason, /toolbar/i);

  const empty = resolveAccess({ tabId: 7, url: '', title: '' });
  assert.equal(empty.access, 'ACCESS_REQUIRED');
});

test('genuinely unsupported schemes are UNSUPPORTED_PAGE with the sanitized scheme', () => {
  for (const [url, scheme] of [
    ['chrome://settings', 'chrome'],
    ['chrome-extension://abcd/page.html', 'chrome-extension'],
    ['file:///etc/passwd', 'file'],
    ['about:blank', 'about'],
  ]) {
    const r = resolveAccess({ tabId: 7, url, title: 'x' });
    assert.equal(r.access, 'UNSUPPORTED_PAGE', `expected ${url} unsupported`);
    assert.equal(r.accessGranted, true);
    assert.equal(r.supportedScheme, false);
    assert.equal(r.origin, null);
    assert.match(r.reason, new RegExp(scheme));
  }
});

test('no active tab is NO_TAB', () => {
  const r = resolveAccess({ tabId: null, url: undefined, title: undefined });
  assert.equal(r.access, 'NO_TAB');
  assert.equal(r.accessGranted, false);
});
