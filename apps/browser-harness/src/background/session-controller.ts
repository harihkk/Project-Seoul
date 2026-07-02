// Project Seoul Development Harness - session controller.
//
// Owns the protocol operations (start/stop/observe/execute/state/context) and
// the shared execute-observe flow. It holds no free module state: the live
// bindings and the panel-invocation context are injected, so the controller is
// independently testable with fakes. Action dispatch goes through the typed
// handler registry (action-handlers.ts); there is no action if/else chain and
// no fallback that reinterprets an unknown action.

import { errorOf, makeError, makeSuccess } from '../protocol.ts';
import type {
  AttachmentInfo,
  ControlSessionRecord,
  ControlSessionState,
  ExecuteActionRequest,
  GetPanelContextRequest,
  GetSessionStateRequest,
  ObservePageRequest,
  PageSnapshot,
  PanelContextResult,
  ProtocolResponse,
  SessionStartRequest,
  SessionStopRequest,
  StructuredError,
} from '../protocol.ts';
import { canTransition } from '../control-session-machine.ts';
import {
  classifyActiveSession,
  pickLatestSessionForTab,
  resolveAccess,
} from '../session.ts';
import * as store from '../control-session-store.ts';
import { resolveActionHandler } from './action-handlers.ts';
import { resolveActiveTab } from './active-tab.ts';
import { sendToContent } from './content-transport.ts';
import type { LiveSessionStore } from './live-sessions.ts';
import type { PanelInvocationContextStore } from './panel-invocation-context.ts';
import { ensureActiveSession, tryRecover } from './session-recovery.ts';

function toMessage(e: unknown): string {
  return e instanceof Error ? e.message : String(e);
}

function safeOrigin(url: string | undefined): string {
  if (!url) return '';
  try {
    return new URL(url).origin;
  } catch {
    return '';
  }
}

export class SessionController {
  readonly #live: LiveSessionStore;
  readonly #invocations: PanelInvocationContextStore;

  constructor(live: LiveSessionStore, invocations: PanelInvocationContextStore) {
    this.#live = live;
    this.#invocations = invocations;
  }

  async #transition(
    sessionId: string,
    to: ControlSessionState,
    eventType: string,
    detail?: string,
    pending?: { action: string | null },
  ): Promise<{ ok: true; rec: ControlSessionRecord } | { ok: false; error: StructuredError }> {
    const rec = await store.getRecord(sessionId);
    if (!rec) return { ok: false, error: errorOf('SESSION_NOT_FOUND', 'No such session.') };
    if (!canTransition(rec.state, to)) {
      return { ok: false, error: errorOf('INVALID_STATE', `Cannot move ${rec.state} -> ${to}.`) };
    }
    const updated = await store.setState(sessionId, to, eventType, detail, pending);
    return { ok: true, rec: updated as ControlSessionRecord };
  }

  async startSession(req: SessionStartRequest): Promise<ProtocolResponse> {
    const tab = await resolveActiveTab(this.#invocations);
    if (!tab || tab.id == null) {
      return makeError(req, errorOf('TAB_MISMATCH', 'No active tab to start a session on.'));
    }
    // The panel echoes the tab id it was shown; it must match the trusted active
    // tab. The side panel cannot nominate an arbitrary tab.
    if (tab.id !== req.tabId) {
      return makeError(
        req,
        errorOf('TAB_MISMATCH', 'The active tab changed; reopen the panel on the page you want.'),
        tab.id,
      );
    }

    const access = resolveAccess({ tabId: tab.id, url: tab.url, title: tab.title });
    if (access.access === 'ACCESS_REQUIRED') {
      return makeError(req, errorOf('ACCESS_REQUIRED', access.reason ?? 'Access required.'), tab.id);
    }
    if (access.access !== 'GRANTED') {
      return makeError(req, errorOf('UNSUPPORTED_PAGE', access.reason ?? 'Unsupported page.'), tab.id);
    }

    const url = tab.url ?? '';
    const origin = safeOrigin(url);
    await store.createRecord(req.sessionId, tab.id, origin);

    try {
      await chrome.scripting.executeScript({ target: { tabId: tab.id }, files: ['content.js'] });
      const initResult = await sendToContent(tab.id, { type: 'SEOUL_INIT', sessionId: req.sessionId });
      if (!initResult.ok) throw new Error(initResult.message);
      const documentToken = (initResult.data as { documentToken?: string } | undefined)?.documentToken;
      this.#live.set({ sessionId: req.sessionId, tabId: tab.id, documentToken, origin, invalidated: false });
      const t = await this.#transition(req.sessionId, 'READY', 'SESSION_READY', `Injected into tab ${tab.id}.`);
      if (!t.ok) return makeError(req, t.error, tab.id);
      return makeSuccess(
        req,
        { state: 'READY' as ControlSessionState, tabId: tab.id, url, title: tab.title ?? '' },
        tab.id,
      );
    } catch (e) {
      const err = errorOf('INJECTION_FAILED', toMessage(e));
      this.#live.delete(req.sessionId);
      await store.setState(req.sessionId, 'FAILED', 'INJECTION_FAILED', err.message);
      await store.recordError(req.sessionId, err);
      return makeError(req, err, tab.id);
    }
  }

  async stopSession(req: SessionStopRequest): Promise<ProtocolResponse> {
    const rec = await store.getRecord(req.sessionId);
    if (!rec) return makeError(req, errorOf('SESSION_NOT_FOUND', 'No such session.'));
    this.#live.delete(req.sessionId);
    if (rec.state !== 'STOPPED' && rec.state !== 'FAILED') {
      await store.setState(req.sessionId, 'STOPPED', 'SESSION_STOPPED', 'Stopped by user.');
    }
    return makeSuccess(req, { state: 'STOPPED' as ControlSessionState }, rec.tabId);
  }

  async observePage(req: ObservePageRequest): Promise<ProtocolResponse> {
    const s = await ensureActiveSession(this.#live, req.sessionId);
    if (!s.ok) {
      await store.recordError(req.sessionId, s.error);
      return makeError(req, s.error);
    }
    const t = await this.#transition(req.sessionId, 'OBSERVING', 'OBSERVE_BEGIN');
    if (!t.ok) return makeError(req, t.error, s.tabId);

    const cr = await sendToContent(s.tabId, { type: 'SEOUL_OBSERVE', sessionId: req.sessionId });
    await store.setState(req.sessionId, 'READY', 'OBSERVE_END');

    if (!cr.ok) {
      const err = errorOf(cr.code, cr.message);
      await store.recordError(req.sessionId, err);
      return makeError(req, err, s.tabId);
    }
    // The snapshot is returned to the side panel but never persisted.
    return makeSuccess(req, { snapshot: cr.data as PageSnapshot }, s.tabId);
  }

  async executeAction(req: ExecuteActionRequest): Promise<ProtocolResponse> {
    const s = await ensureActiveSession(this.#live, req.sessionId);
    if (!s.ok) {
      await store.recordError(req.sessionId, s.error);
      return makeError(req, s.error);
    }
    const action = req.action;

    // Typed dispatch: resolve the handler by kind. An unregistered kind is an
    // explicit unsupported-action error, never silently reinterpreted.
    const handler = resolveActionHandler(action);
    if (!handler) {
      const err = errorOf('ACTION_FAILED', `Unsupported action kind: ${String(action.kind)}.`);
      await store.recordError(req.sessionId, err);
      return makeError(req, err, s.tabId);
    }

    // Persist only the non-sensitive action kind, atomically with EXECUTING, so
    // an interrupted action can be reported on recovery.
    const t = await this.#transition(req.sessionId, 'EXECUTING', 'EXECUTE_BEGIN', action.kind, {
      action: action.kind,
    });
    if (!t.ok) return makeError(req, t.error, s.tabId);

    const plan = handler.plan(req.sessionId, action);

    if (plan.mode === 'rejected') {
      await store.setState(req.sessionId, 'READY', 'EXECUTE_END', action.kind, { action: null });
      await store.recordError(req.sessionId, plan.error);
      return makeError(req, plan.error, s.tabId);
    }

    if (plan.mode === 'navigate') {
      const target = safeOrigin(plan.url) || 'the target origin';
      await chrome.tabs.update(s.tabId, { url: plan.url });
      // Navigation invalidates the injected content script: end the session.
      this.#live.delete(req.sessionId);
      await store.setState(
        req.sessionId,
        'STOPPED',
        'NAVIGATED',
        `Navigated to ${target}; session ended, restart required.`,
        { action: null },
      );
      return makeSuccess(
        req,
        { navigated: true, state: 'STOPPED' as ControlSessionState, restartRequired: true },
        s.tabId,
      );
    }

    // mode === 'content': one content-script step. Record only the action kind
    // in the timeline (never typed text) and clear pending atomically with the
    // completion transition.
    const cr = await sendToContent(s.tabId, plan.request);
    await store.setState(req.sessionId, 'READY', 'EXECUTE_END', action.kind, { action: null });

    if (!cr.ok) {
      const err = errorOf(cr.code, cr.message);
      await store.recordError(req.sessionId, err);
      return makeError(req, err, s.tabId);
    }
    return makeSuccess(req, { result: cr.data }, s.tabId);
  }

  async getSessionState(req: GetSessionStateRequest): Promise<ProtocolResponse> {
    const rec = await store.getRecord(req.sessionId);
    if (!rec) return makeError(req, errorOf('SESSION_NOT_FOUND', 'No such session.'));
    return makeSuccess(
      req,
      {
        state: rec.state,
        timeline: rec.timeline,
        lastError: rec.lastError,
        createdAt: rec.createdAt,
        updatedAt: rec.updatedAt,
        tabId: rec.tabId,
      },
      rec.tabId,
    );
  }

  // The background determines the active tab itself and reports its access
  // state. Sessions are only resolved/recovered for a granted, supported tab.
  async getPanelContext(req: GetPanelContextRequest): Promise<ProtocolResponse> {
    const tab = await resolveActiveTab(this.#invocations);
    const tabId = tab?.id ?? null;
    const access = resolveAccess({ tabId, url: tab?.url, title: tab?.title });

    let attachment: AttachmentInfo = { sessionId: null, attached: false };
    if (access.access === 'GRANTED' && tabId != null) {
      const records = await store.listRecords();
      const picked = pickLatestSessionForTab(records, tabId);
      if (picked) {
        const isLive = this.#live.isLiveOnTab(picked.sessionId, tabId);
        const classification = classifyActiveSession(picked, isLive);
        if (classification.live) {
          attachment = { sessionId: picked.sessionId, attached: true };
        } else if (classification.needsProbe) {
          const rc = await tryRecover(this.#live, picked, tabId);
          if (rc.recovered) attachment = { sessionId: picked.sessionId, attached: true };
        }
      }
    }

    const result: PanelContextResult = {
      tabId,
      title: access.title,
      origin: access.origin,
      access: access.access,
      accessGranted: access.accessGranted,
      supportedScheme: access.supportedScheme,
      reason: access.reason,
      attachment,
    };
    return makeSuccess(req, result, tabId ?? undefined);
  }
}
