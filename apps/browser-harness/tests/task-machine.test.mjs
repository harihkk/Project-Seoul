import test from 'node:test';
import assert from 'node:assert/strict';

import { TASK_STATES } from '../src/protocol.ts';
import {
  VALID_TRANSITIONS,
  canTransition,
  assertTransition,
  isTerminal,
  reconcileOnStartup,
  InvalidTransitionError,
  TaskMachine,
} from '../src/task-machine.ts';

test('every declared valid transition is accepted by the machine', () => {
  for (const from of TASK_STATES) {
    for (const to of VALID_TRANSITIONS[from]) {
      assert.equal(canTransition(from, to), true, `${from} -> ${to} should be valid`);
      const machine = new TaskMachine(from);
      assert.equal(machine.to(to), to);
      assert.equal(machine.state, to);
    }
  }
});

test('every transition not declared valid is rejected and throws a typed error', () => {
  for (const from of TASK_STATES) {
    for (const to of TASK_STATES) {
      if (VALID_TRANSITIONS[from].includes(to)) continue;
      assert.equal(canTransition(from, to), false, `${from} -> ${to} should be invalid`);
      assert.throws(() => assertTransition(from, to), InvalidTransitionError);
      const machine = new TaskMachine(from);
      assert.throws(() => machine.to(to), (err) => {
        assert.ok(err instanceof InvalidTransitionError);
        assert.equal(err.from, from);
        assert.equal(err.to, to);
        return true;
      });
    }
  }
});

test('stop behavior: active states can stop, and STOPPED is terminal', () => {
  for (const from of ['STARTING', 'READY', 'OBSERVING', 'EXECUTING']) {
    assert.equal(canTransition(from, 'STOPPED'), true);
  }
  assert.equal(isTerminal('STOPPED'), true);
  const machine = new TaskMachine('STOPPED');
  assert.throws(() => machine.to('READY'), InvalidTransitionError);
});

test('failure behavior: active states can fail, and FAILED is terminal', () => {
  for (const from of ['STARTING', 'READY', 'OBSERVING', 'EXECUTING']) {
    assert.equal(canTransition(from, 'FAILED'), true);
  }
  assert.equal(isTerminal('FAILED'), true);
  const machine = new TaskMachine('FAILED');
  assert.throws(() => machine.to('STOPPED'), InvalidTransitionError);
});

test('restored active sessions are reconciled to STOPPED on startup', () => {
  assert.equal(reconcileOnStartup('READY'), 'STOPPED');
  assert.equal(reconcileOnStartup('OBSERVING'), 'STOPPED');
  assert.equal(reconcileOnStartup('EXECUTING'), 'STOPPED');
  assert.equal(reconcileOnStartup('STARTING'), 'STOPPED');
  assert.equal(reconcileOnStartup('IDLE'), 'STOPPED');
  // Already-terminal states are preserved.
  assert.equal(reconcileOnStartup('STOPPED'), 'STOPPED');
  assert.equal(reconcileOnStartup('FAILED'), 'FAILED');
});

test('a fresh machine starts IDLE and follows the happy path', () => {
  const machine = new TaskMachine();
  assert.equal(machine.state, 'IDLE');
  machine.to('STARTING');
  machine.to('READY');
  machine.to('OBSERVING');
  machine.to('READY');
  machine.to('EXECUTING');
  machine.to('READY');
  machine.to('STOPPED');
  assert.equal(machine.isTerminal(), true);
});
