import test from 'node:test';
import assert from 'node:assert/strict';

import { CONTROL_SESSION_STATES } from '../src/protocol.ts';
import {
  VALID_TRANSITIONS,
  canTransition,
  assertTransition,
  isTerminal,
  reconcileOnStartup,
  InvalidTransitionError,
  ControlSessionMachine,
} from '../src/control-session-machine.ts';

test('every declared valid transition is accepted by the machine', () => {
  for (const from of CONTROL_SESSION_STATES) {
    for (const to of VALID_TRANSITIONS[from]) {
      assert.equal(canTransition(from, to), true, `${from} -> ${to} should be valid`);
      const machine = new ControlSessionMachine(from);
      assert.equal(machine.to(to), to);
      assert.equal(machine.state, to);
    }
  }
});

test('every transition not declared valid is rejected and throws a typed error', () => {
  for (const from of CONTROL_SESSION_STATES) {
    for (const to of CONTROL_SESSION_STATES) {
      if (VALID_TRANSITIONS[from].includes(to)) continue;
      assert.equal(canTransition(from, to), false, `${from} -> ${to} should be invalid`);
      assert.throws(() => assertTransition(from, to), InvalidTransitionError);
      const machine = new ControlSessionMachine(from);
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
  const machine = new ControlSessionMachine('STOPPED');
  assert.throws(() => machine.to('READY'), InvalidTransitionError);
});

test('failure behavior: active states can fail, and FAILED is terminal', () => {
  for (const from of ['STARTING', 'READY', 'OBSERVING', 'EXECUTING']) {
    assert.equal(canTransition(from, 'FAILED'), true);
  }
  assert.equal(isTerminal('FAILED'), true);
  const machine = new ControlSessionMachine('FAILED');
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
  const machine = new ControlSessionMachine();
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
