// Project Seoul Development Harness - pure session-selection and recovery logic.
//
// Decides which stored session (if any) belongs to a given active tab, whether
// it can be recovered after a worker restart, and how it should be presented.
// Pure and import-safe (no DOM, no extension APIs), so it is unit tested
// directly.

import type {
  ControlSessionRecord,
  ProbeData,
  ControlSessionState,
  StructuredError,
  AccessState,
} from './protocol.ts';
import { isTerminal } from './control-session-machine.ts';
import { isValidDocumentToken } from './validation.ts';

// --- Active-tab access resolution ---
//
// The active tab's URL/title are readable only when the extension has an
// activeTab grant for it (conferred by the user invoking the toolbar action).
// An unreadable URL therefore means "no grant" (ACCESS_REQUIRED), which is a
// distinct condition from a readable but unsupported scheme (UNSUPPORTED_PAGE).
// This function never invents placeholder metadata.

export interface TabAccessInput {
  tabId: number | null;
  url: string | undefined;
  title: string | undefined;
}
export interface AccessResolution {
  access: AccessState;
  accessGranted: boolean;
  supportedScheme: boolean;
  title: string | null;
  origin: string | null;
  reason: string | null;
}

function schemeOf(url: string): string {
  const m = /^([a-z][a-z0-9+.-]*):/i.exec(url);
  return m ? m[1].toLowerCase() : 'unknown';
}

export function resolveAccess(input: TabAccessInput): AccessResolution {
  if (input.tabId == null) {
    return {
      access: 'NO_TAB',
      accessGranted: false,
      supportedScheme: false,
      title: null,
      origin: null,
      reason: 'No active tab is available.',
    };
  }
  const url = typeof input.url === 'string' ? input.url : '';
  if (url.length === 0) {
    // No readable URL => no activeTab grant for this tab. This is not an
    // unsupported page; the user simply has not granted access yet.
    return {
      access: 'ACCESS_REQUIRED',
      accessGranted: false,
      supportedScheme: true,
      title: null,
      origin: null,
      reason: 'Click the Project Seoul toolbar icon while viewing the page you want Seoul to access.',
    };
  }
  if (!/^https?:\/\//i.test(url)) {
    const scheme = schemeOf(url);
    return {
      access: 'UNSUPPORTED_PAGE',
      accessGranted: true,
      supportedScheme: false,
      title: null,
      origin: null,
      reason: `Pages with the "${scheme}:" scheme are not supported by this harness.`,
    };
  }
  return {
    access: 'GRANTED',
    accessGranted: true,
    supportedScheme: true,
    title: input.title ?? null,
    origin: originOf(url),
    reason: null,
  };
}

// Pick the single most recently updated session bound to the given tab.
// Ordering is deterministic: newest updatedAt first, then sessionId descending
// as a stable tie-break. Sessions for other tabs are never returned, and
// timelines from separate sessions are never merged.
export function pickLatestSessionForTab(
  records: readonly ControlSessionRecord[],
  tabId: number,
): ControlSessionRecord | null {
  const matches = records.filter((r) => r.tabId === tabId);
  if (matches.length === 0) return null;
  const sorted = [...matches].sort((a, b) => {
    if (b.updatedAt !== a.updatedAt) return b.updatedAt - a.updatedAt;
    if (a.sessionId < b.sessionId) return 1;
    if (a.sessionId > b.sessionId) return -1;
    return 0;
  });
  return sorted[0];
}

export interface ActiveSessionClassification {
  session: ControlSessionRecord | null;
  // Non-terminal session with no trusted live backing: it must be probed before
  // it can be shown, then recovered or stopped based on the probe result.
  needsProbe: boolean;
  // Non-terminal session backed by a live, tab-matching binding.
  live: boolean;
}

export function classifyActiveSession(
  session: ControlSessionRecord | null,
  isLive: boolean,
): ActiveSessionClassification {
  if (!session) return { session: null, needsProbe: false, live: false };
  // Terminal sessions are restored as-is and are never probed as active.
  if (isTerminal(session.state)) {
    return { session, needsProbe: false, live: false };
  }
  if (isLive) {
    return { session, needsProbe: false, live: true };
  }
  return { session, needsProbe: true, live: false };
}

export function wasReconciled(session: ControlSessionRecord | null): boolean {
  return !!session && session.timeline.some((e) => e.type === 'RECONCILED_ON_STARTUP');
}

function originOf(url: string): string | null {
  try {
    return new URL(url).origin;
  } catch {
    return null;
  }
}

export type ProbeOutcome = { outcome: 'recover' } | { outcome: 'stop'; reason: string };

// Decide whether a probed content script proves the stored session is still
// valid. Recovery succeeds only when the content script responds, is
// initialized, reports the same session id, returns a well-formed document
// token, and is still on a compatible origin.
export function evaluateProbe(probe: ProbeData | null, record: ControlSessionRecord): ProbeOutcome {
  if (!probe) {
    return { outcome: 'stop', reason: 'Content script did not respond to the recovery probe.' };
  }
  if (!probe.initialized) {
    return { outcome: 'stop', reason: 'Content script is not initialized.' };
  }
  if (probe.sessionId !== record.sessionId) {
    return { outcome: 'stop', reason: 'Probe session id does not match the stored session.' };
  }
  if (!isValidDocumentToken(probe.documentToken)) {
    return { outcome: 'stop', reason: 'Document token is missing or malformed.' };
  }
  const probeOrigin = originOf(probe.url);
  if (!probeOrigin) {
    return { outcome: 'stop', reason: 'Probe URL is not a valid absolute URL.' };
  }
  if (record.origin && record.origin !== probeOrigin) {
    return { outcome: 'stop', reason: 'Page navigated to an incompatible origin.' };
  }
  return { outcome: 'recover' };
}

// --- Transient-state recovery policy ---
//
// A successful probe proves the session and document are alive, but not the
// outcome of an operation that was mid-flight when the worker terminated. The
// stored control-session state therefore decides what is safe to restore:
//   READY      -> recover to READY (SESSION_RECOVERED)
//   STARTING   -> if the probe proves initialization, recover to READY; the
//                 verdict==='stop' path already covers "init not proven"
//   OBSERVING  -> recover to READY but invalidate the snapshot, since the
//                 observation response may have been lost (OBSERVATION_INTERRUPTED)
//   EXECUTING  -> stop with ACTION_OUTCOME_UNKNOWN regardless of probe outcome,
//                 because the action may have completed without the result being
//                 persisted; it is never replayed
// Terminal states are not probed and never reach here.

export interface RecoveryPlan {
  outcome: 'recover' | 'stop';
  toState: ControlSessionState;
  event: string;
  detail: string;
  clearSnapshot: boolean;
  error: StructuredError | null;
}

export function planRecovery(record: ControlSessionRecord, verdict: ProbeOutcome): RecoveryPlan {
  // EXECUTING is decided by the action being in-flight, not by liveness: the
  // outcome is unknown whether or not the content script still answers.
  if (record.state === 'EXECUTING') {
    const kind = record.pendingAction ?? 'action';
    return {
      outcome: 'stop',
      toState: 'STOPPED',
      event: 'ACTION_OUTCOME_UNKNOWN',
      detail: `Action "${kind}" was interrupted by a worker restart; its outcome is unknown.`,
      clearSnapshot: false,
      error: {
        code: 'ACTION_OUTCOME_UNKNOWN',
        message: `The previous action (${kind}) may or may not have completed. Seoul will not retry it. Start a new session to continue.`,
      },
    };
  }

  if (verdict.outcome === 'stop') {
    return {
      outcome: 'stop',
      toState: 'STOPPED',
      event: 'RECONCILED_ON_STARTUP',
      detail: verdict.reason,
      clearSnapshot: false,
      error: null,
    };
  }

  switch (record.state) {
    case 'READY':
      return {
        outcome: 'recover',
        toState: 'READY',
        event: 'SESSION_RECOVERED',
        detail: 'Session recovered after a worker restart.',
        clearSnapshot: false,
        error: null,
      };
    case 'STARTING':
      return {
        outcome: 'recover',
        toState: 'READY',
        event: 'SESSION_RECOVERED',
        detail: 'Initialization was interrupted by a worker restart; recovered to READY.',
        clearSnapshot: false,
        error: null,
      };
    case 'OBSERVING':
      return {
        outcome: 'recover',
        toState: 'READY',
        event: 'OBSERVATION_INTERRUPTED',
        detail: 'Observation response was lost; reobserve before any page action.',
        clearSnapshot: true,
        error: null,
      };
    default:
      // IDLE should not be stored, and terminal states are not probed.
      return {
        outcome: 'stop',
        toState: 'STOPPED',
        event: 'RECONCILED_ON_STARTUP',
        detail: `Unexpected state "${record.state}" during recovery.`,
        clearSnapshot: false,
        error: null,
      };
  }
}
