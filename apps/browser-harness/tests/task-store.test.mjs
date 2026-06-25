import test from 'node:test';
import assert from 'node:assert/strict';

// Minimal in-memory chrome.storage.local mock with failure injection, installed
// before importing the store. task-store only touches chrome.* inside its
// functions, so the global is read lazily at call time.
const memory = { data: {}, failNextSet: false };
globalThis.chrome = {
  storage: {
    local: {
      async get(key) {
        return key in memory.data ? { [key]: memory.data[key] } : {};
      },
      async set(obj) {
        if (memory.failNextSet) {
          memory.failNextSet = false;
          throw new Error('storage set failed');
        }
        Object.assign(memory.data, obj);
      },
    },
  },
};

const store = await import('../src/task-store.ts');

function reset() {
  memory.data = {};
  memory.failNextSet = false;
}

test('createRecord stores the session origin and starts at STARTING', async () => {
  reset();
  const r = await store.createRecord('s', 5, 'http://localhost:8765');
  assert.equal(r.origin, 'http://localhost:8765');
  assert.equal(r.tabId, 5);
  assert.equal(r.state, 'STARTING');
});

test('applyRecovery recovers to READY, appends one event, and clears pending action', async () => {
  reset();
  await store.createRecord('s', 1);
  await store.setState('s', 'EXECUTING', 'EXECUTE_BEGIN', 'CLICK_ELEMENT', { action: 'CLICK_ELEMENT' });
  const r = await store.applyRecovery('s', 'READY', 'SESSION_RECOVERED', 'recovered', null);
  assert.equal(r.state, 'READY');
  assert.equal('pendingAction' in r, false);
  assert.equal(r.timeline.filter((e) => e.type === 'SESSION_RECOVERED').length, 1);
});

test('applyRecovery records a structured error when stopping an interrupted action', async () => {
  reset();
  await store.createRecord('s', 1);
  await store.setState('s', 'EXECUTING', 'EXECUTE_BEGIN', 'TYPE_TEXT', { action: 'TYPE_TEXT' });
  const err = { code: 'ACTION_OUTCOME_UNKNOWN', message: 'outcome unknown' };
  const r = await store.applyRecovery('s', 'STOPPED', 'ACTION_OUTCOME_UNKNOWN', 'interrupted', err);
  assert.equal(r.state, 'STOPPED');
  assert.deepEqual(r.lastError, err);
  assert.equal(r.timeline.filter((e) => e.type === 'ACTION_OUTCOME_UNKNOWN').length, 1);
  assert.equal('pendingAction' in r, false);
});

test('applyRecovery returns null for an unknown session', async () => {
  reset();
  assert.equal(await store.applyRecovery('missing', 'READY', 'SESSION_RECOVERED', 'x', null), null);
});

test('only the action kind is persisted while EXECUTING, never the payload', async () => {
  reset();
  await store.createRecord('s', 1);
  // The store is only ever handed the kind; typed text and selectors are not passed.
  await store.setState('s', 'EXECUTING', 'EXECUTE_BEGIN', 'TYPE_TEXT', { action: 'TYPE_TEXT' });
  const rec = await store.getRecord('s');
  assert.equal(rec.pendingAction, 'TYPE_TEXT');
  const raw = JSON.stringify(memory.data);
  assert.equal(raw.includes('secret-typed-value'), false);
  assert.equal(raw.includes('value'), false);
  // Completion clears the pending action atomically with the transition.
  await store.setState('s', 'READY', 'EXECUTE_END', 'TYPE_TEXT', { action: null });
  assert.equal('pendingAction' in (await store.getRecord('s')), false);
});

test('storage mutations are serialized so concurrent writes do not lose updates', async () => {
  reset();
  await store.createRecord('s', 1);
  await Promise.all([
    store.appendEvent('s', 'A'),
    store.appendEvent('s', 'B'),
    store.appendEvent('s', 'C'),
  ]);
  const rec = await store.getRecord('s');
  const types = rec.timeline.map((e) => e.type);
  assert.ok(types.includes('A'), 'A present');
  assert.ok(types.includes('B'), 'B present');
  assert.ok(types.includes('C'), 'C present');
});

test('a failed storage write is surfaced, not swallowed, and the queue recovers', async () => {
  reset();
  await store.createRecord('s', 1);
  memory.failNextSet = true;
  await assert.rejects(() => store.setState('s', 'READY', 'SESSION_READY'));
  // The serialized queue keeps working after a failure.
  const ok = await store.setState('s', 'READY', 'SESSION_READY');
  assert.equal(ok.state, 'READY');
});

test('persisted records never carry page text, input values, or snapshots', async () => {
  reset();
  await store.createRecord('s', 1, 'http://localhost:8765');
  await store.setState('s', 'READY', 'SESSION_READY');
  await store.appendEvent('s', 'EXECUTE_END', 'TYPE_TEXT');
  const raw = JSON.stringify(memory.data);
  assert.equal(raw.includes('value'), false);
  const rec = await store.getRecord('s');
  for (const key of Object.keys(rec)) {
    assert.equal(['snapshot', 'text', 'textBlocks', 'html', 'password'].includes(key), false);
  }
});

test('the timeline is bounded to the configured maximum', async () => {
  reset();
  await store.createRecord('s', 1);
  for (let i = 0; i < 260; i++) {
    await store.appendEvent('s', `E${i}`);
  }
  const rec = await store.getRecord('s');
  assert.ok(rec.timeline.length <= 200, `expected <= 200, got ${rec.timeline.length}`);
});
