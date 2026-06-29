// Project Seoul Development Harness - control-session state machine.
//
// Pure module describing the legal control-session lifecycle. Invalid transitions throw a
// typed error so both runtime code and tests can rely on the same guarantees.

import type { ControlSessionState } from './protocol.ts';

export const VALID_TRANSITIONS: Record<ControlSessionState, readonly ControlSessionState[]> = {
  IDLE: ['STARTING'],
  STARTING: ['READY', 'FAILED', 'STOPPED'],
  READY: ['OBSERVING', 'EXECUTING', 'STOPPED', 'FAILED'],
  OBSERVING: ['READY', 'STOPPED', 'FAILED'],
  EXECUTING: ['READY', 'STOPPED', 'FAILED'],
  STOPPED: [],
  FAILED: [],
};

export class InvalidTransitionError extends Error {
  from: ControlSessionState;
  to: ControlSessionState;
  constructor(from: ControlSessionState, to: ControlSessionState) {
    super(`Invalid control-session transition: ${from} -> ${to}`);
    this.name = 'InvalidTransitionError';
    this.from = from;
    this.to = to;
  }
}

export function canTransition(from: ControlSessionState, to: ControlSessionState): boolean {
  return VALID_TRANSITIONS[from].includes(to);
}

export function assertTransition(from: ControlSessionState, to: ControlSessionState): void {
  if (!canTransition(from, to)) {
    throw new InvalidTransitionError(from, to);
  }
}

export function isTerminal(state: ControlSessionState): boolean {
  return VALID_TRANSITIONS[state].length === 0;
}

// After an extension restart the injected page state cannot be trusted, so any
// session that was not already terminal is forced to STOPPED.
export function reconcileOnStartup(state: ControlSessionState): ControlSessionState {
  if (state === 'STOPPED' || state === 'FAILED') return state;
  return 'STOPPED';
}

export class ControlSessionMachine {
  state: ControlSessionState;
  constructor(initial: ControlSessionState = 'IDLE') {
    this.state = initial;
  }
  can(to: ControlSessionState): boolean {
    return canTransition(this.state, to);
  }
  to(next: ControlSessionState): ControlSessionState {
    assertTransition(this.state, next);
    this.state = next;
    return this.state;
  }
  isTerminal(): boolean {
    return isTerminal(this.state);
  }
}
