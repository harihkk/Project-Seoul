// Project Seoul Development Harness - session recovery.
//
// After a service-worker restart the in-memory live binding is gone but the
// durable control-session record may still be active. Recovery probes the
// content script and either rebuilds the live binding (recover) or reconciles
// the record to STOPPED. This module owns no state; it operates on the injected
// LiveSessionStore and the control-session store, and reuses the pure recovery
// policy in session.ts.

import { errorOf } from '../protocol.ts';
import type { ControlSessionRecord, StructuredError } from '../protocol.ts';
import { evaluateProbe, planRecovery } from '../session.ts';
import * as store from '../control-session-store.ts';
import type { LiveSessionStore } from './live-sessions.ts';
import { probeContent, sendToContent } from './content-transport.ts';

export type RecoveryOutcome =
  | { recovered: true; rec: ControlSessionRecord }
  | { recovered: false; rec: ControlSessionRecord | null; error: StructuredError };

// Probe a stored session's content script and apply the recovery plan. Shared
// by panel rehydration and by any request whose live backing was lost, so a
// still-open panel recovers exactly as a reopened one does.
export async function tryRecover(
  live: LiveSessionStore,
  rec: ControlSessionRecord,
  tabId: number,
): Promise<RecoveryOutcome> {
  const probe = await probeContent(tabId, rec.sessionId);
  const verdict = evaluateProbe(probe, rec);
  const plan = planRecovery(rec, verdict);

  // OBSERVING recovery: drop the content script's snapshot/registry without
  // re-injecting, so the interrupted observation's snapshot cannot be reused.
  if (plan.clearSnapshot) {
    await sendToContent(tabId, { type: 'SEOUL_INIT', sessionId: rec.sessionId });
  }

  if (plan.outcome === 'recover') {
    live.set({
      sessionId: rec.sessionId,
      tabId,
      documentToken: probe?.documentToken,
      origin: rec.origin,
      invalidated: false,
    });
    const updated = await store.applyRecovery(
      rec.sessionId,
      plan.toState,
      plan.event,
      plan.detail,
      plan.error,
    );
    return { recovered: true, rec: updated ?? rec };
  }

  const stopped = await store.applyRecovery(
    rec.sessionId,
    plan.toState,
    plan.event,
    plan.detail,
    plan.error,
  );
  return {
    recovered: false,
    rec: stopped,
    error: plan.error ?? errorOf('STALE_SESSION', plan.detail),
  };
}

export type EnsureActiveOutcome =
  | { ok: true; rec: ControlSessionRecord; tabId: number }
  | { ok: false; error: StructuredError };

// Confirms a session is active, bound to the current active tab, and has a live
// backing - recovering it after a worker restart when needed. Never a grant
// check: the tab's activeTab access is enforced separately by the caller.
export async function ensureActiveSession(
  live: LiveSessionStore,
  sessionId: string,
): Promise<EnsureActiveOutcome> {
  const rec = await store.getRecord(sessionId);
  if (!rec) return { ok: false, error: errorOf('SESSION_NOT_FOUND', 'No such session.') };
  if (rec.state === 'STOPPED' || rec.state === 'FAILED') {
    return { ok: false, error: errorOf('STALE_SESSION', 'Session has ended. Start a new session.') };
  }
  const tab = await chrome.tabs.get(rec.tabId).catch(() => null);
  if (!tab || tab.id == null) {
    return { ok: false, error: errorOf('TAB_MISMATCH', 'Bound tab no longer exists.') };
  }
  if (!tab.active) {
    return { ok: false, error: errorOf('TAB_MISMATCH', 'Bound tab is not the active tab.') };
  }

  let ls = live.get(sessionId);
  if (!ls || ls.invalidated) {
    const rc = await tryRecover(live, rec, rec.tabId);
    if (!rc.recovered) return { ok: false, error: rc.error };
    ls = live.get(sessionId);
    if (!ls) return { ok: false, error: errorOf('STALE_SESSION', 'Session could not be recovered.') };
  }
  return { ok: true, rec, tabId: ls.tabId };
}
