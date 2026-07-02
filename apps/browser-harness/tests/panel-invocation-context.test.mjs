import test from 'node:test';
import assert from 'node:assert/strict';

import {
  INVOCATION_TTL_MS,
  PanelInvocationContextStore,
} from '../src/background/panel-invocation-context.ts';

// Section D: the panel invocation context survives a service-worker restart,
// expires when stale, is cleared on tab/window lifecycle, is revalidated
// against live Chrome state, and is never treated as an access grant. The store
// depends only on an injected InvocationChrome, so it is fully unit-testable.

function makeEnv(startTime = 1_000_000) {
  const backing = { data: {} };
  const clock = { now: startTime };
  const env = {
    async storageSessionGet(key) {
      return key in backing.data ? { [key]: backing.data[key] } : {};
    },
    async storageSessionSet(items) {
      Object.assign(backing.data, items);
    },
    async storageSessionRemove(key) {
      delete backing.data[key];
    },
    now() {
      return clock.now;
    },
  };
  return { env, backing, clock };
}

test('a recorded invocation survives a simulated worker restart', async () => {
  const { env, backing } = makeEnv();
  // First worker generation records the invocation.
  const first = new PanelInvocationContextStore(env);
  await first.record({ tabId: 7, windowId: 3 });

  // Simulate a service-worker restart: a brand new store instance over the same
  // storage.session backing (module state is gone; storage persists).
  const restarted = new PanelInvocationContextStore(env);
  const record = await restarted.peek();
  assert.ok(record);
  assert.equal(record.tabId, 7);
  assert.equal(record.windowId, 3);
  assert.ok('seoul.panelInvocation.v1' in backing.data);
});

test('only non-secret routing fields are persisted', async () => {
  const { env, backing } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 1, windowId: 1, correlationId: 'abc' });
  const stored = backing.data['seoul.panelInvocation.v1'];
  assert.deepEqual(Object.keys(stored).sort(), [
    'correlationId',
    'invokedAt',
    'tabId',
    'windowId',
  ]);
});

test('a stale invocation past the TTL is expired and cleared', async () => {
  const { env, clock } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 4, windowId: 2 });
  clock.now += INVOCATION_TTL_MS + 1;
  assert.equal(await store.peek(), null);
  // And it was cleared, not merely ignored.
  clock.now = 0;
  assert.equal(await store.peek(), null);
});

test('resolveValid clears context when the tab no longer exists', async () => {
  const { env } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 9, windowId: 5 });
  const resolved = await store.resolveValid(async () => null); // tab gone
  assert.equal(resolved, null);
  assert.equal(await store.peek(), null);
});

test('resolveValid clears context when the tab moved to another window', async () => {
  const { env } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 9, windowId: 5 });
  const resolved = await store.resolveValid(async () => ({ windowId: 99 }));
  assert.equal(resolved, null);
  assert.equal(await store.peek(), null);
});

test('resolveValid returns the record when the tab is still in its window', async () => {
  const { env } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 9, windowId: 5 });
  const resolved = await store.resolveValid(async () => ({ windowId: 5 }));
  assert.ok(resolved);
  assert.equal(resolved.tabId, 9);
});

test('closing the referenced tab clears the context', async () => {
  const { env } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 11, windowId: 6 });
  await store.onTabRemoved(999); // unrelated tab
  assert.ok(await store.peek());
  await store.onTabRemoved(11);
  assert.equal(await store.peek(), null);
});

test('closing the referenced window clears the context', async () => {
  const { env } = makeEnv();
  const store = new PanelInvocationContextStore(env);
  await store.record({ tabId: 11, windowId: 6 });
  await store.onWindowRemoved(1); // unrelated window
  assert.ok(await store.peek());
  await store.onWindowRemoved(6);
  assert.equal(await store.peek(), null);
});

test('a malformed stored record is rejected and cleared', async () => {
  const { env, backing } = makeEnv();
  backing.data['seoul.panelInvocation.v1'] = { tabId: 'not-a-number', windowId: 1 };
  const store = new PanelInvocationContextStore(env);
  assert.equal(await store.peek(), null);
  assert.equal('seoul.panelInvocation.v1' in backing.data, false);
});
