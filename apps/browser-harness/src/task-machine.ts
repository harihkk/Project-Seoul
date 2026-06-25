// Project Seoul Development Harness - task state machine.
//
// Pure module describing the legal task lifecycle. Invalid transitions throw a
// typed error so both runtime code and tests can rely on the same guarantees.

import type { TaskState } from './protocol.ts';

export const VALID_TRANSITIONS: Record<TaskState, readonly TaskState[]> = {
  IDLE: ['STARTING'],
  STARTING: ['READY', 'FAILED', 'STOPPED'],
  READY: ['OBSERVING', 'EXECUTING', 'STOPPED', 'FAILED'],
  OBSERVING: ['READY', 'STOPPED', 'FAILED'],
  EXECUTING: ['READY', 'STOPPED', 'FAILED'],
  STOPPED: [],
  FAILED: [],
};

export class InvalidTransitionError extends Error {
  from: TaskState;
  to: TaskState;
  constructor(from: TaskState, to: TaskState) {
    super(`Invalid task transition: ${from} -> ${to}`);
    this.name = 'InvalidTransitionError';
    this.from = from;
    this.to = to;
  }
}

export function canTransition(from: TaskState, to: TaskState): boolean {
  return VALID_TRANSITIONS[from].includes(to);
}

export function assertTransition(from: TaskState, to: TaskState): void {
  if (!canTransition(from, to)) {
    throw new InvalidTransitionError(from, to);
  }
}

export function isTerminal(state: TaskState): boolean {
  return VALID_TRANSITIONS[state].length === 0;
}

// After an extension restart the injected page state cannot be trusted, so any
// session that was not already terminal is forced to STOPPED.
export function reconcileOnStartup(state: TaskState): TaskState {
  if (state === 'STOPPED' || state === 'FAILED') return state;
  return 'STOPPED';
}

export class TaskMachine {
  state: TaskState;
  constructor(initial: TaskState = 'IDLE') {
    this.state = initial;
  }
  can(to: TaskState): boolean {
    return canTransition(this.state, to);
  }
  to(next: TaskState): TaskState {
    assertTransition(this.state, next);
    this.state = next;
    return this.state;
  }
  isTerminal(): boolean {
    return isTerminal(this.state);
  }
}
