// Project Seoul Development Harness - background service worker (ES module).
//
// Responsibilities:
//  - open the side panel from the toolbar action;
//  - inject the content script only after an explicit user gesture (Start);
//  - own the in-memory live-session map and the trust boundary;
//  - validate every protocol request and reject untrusted / stale / mismatched
//    messages;
//  - recover a still-valid session after a worker restart by probing the
//    content script, rather than stopping every session on restart;
//  - invalidate sessions on external navigation and tab closure;
//  - drive navigation through the tabs API and forward snapshot-bound DOM
//    actions to the content script.
//
// A web page sending a message can never cause an action: only the extension's
// own side panel page is accepted as a request sender.

import { makeSuccess, makeError, errorOf } from './protocol.ts';
import type {
  ProtocolResponse,
  ErrorCode,
  TaskState,
  TaskRecord,
  StructuredError,
  PageSnapshot,
  ContentRequest,
  ContentResult,
  ProbeData,
  SessionStartRequest,
  SessionStopRequest,
  ObservePageRequest,
  ExecuteActionRequest,
  GetTaskStateRequest,
  GetPanelContextRequest,
  PanelContextResult,
  AttachmentInfo,
} from './protocol.ts';
import { parseRequest, validateNavigationUrl } from './validation.ts';
import { canTransition } from './task-machine.ts';
import {
  pickLatestSessionForTab,
  classifyActiveSession,
  evaluateProbe,
  planRecovery,
  resolveAccess,
} from './session.ts';
import * as store from './task-store.ts';

interface LiveSession {
  sessionId: string;
  tabId: number;
  documentToken?: string;
  origin?: string;
  invalidated?: boolean;
}

// In-memory map of live sessions, lost whenever the worker restarts. After a
// restart a stored session is a candidate that must be probed before it is
// trusted or stopped.
const live = new Map<string, LiveSession>();

function toMessage(e: unknown): string {
  if (e instanceof Error) return e.message;
  return String(e);
}

function safeOrigin(url: string | undefined): string {
  if (!url) return '';
  try {
    return new URL(url).origin;
  } catch {
    return '';
  }
}

// Disable auto-open so that clicking the toolbar action fires action.onClicked.
// That click is what confers the activeTab grant for the clicked tab; auto-open
// (openPanelOnActionClick) opens the panel without granting access, which is the
// bug this milestone fixes. We open the panel ourselves from onClicked instead.
async function configurePanelBehavior(): Promise<void> {
  try {
    if (chrome.sidePanel && typeof chrome.sidePanel.setPanelBehavior === 'function') {
      await chrome.sidePanel.setPanelBehavior({ openPanelOnActionClick: false });
    }
  } catch {
    // Non-fatal on builds without setPanelBehavior.
  }
}

// The tab from the most recent action invocation, used only to scope active-tab
// resolution to the right window for a global side panel. It is never trusted as
// the access grant itself (that is determined by URL readability).
let lastActionTab: { tabId: number; windowId: number } | null = null;

async function resolveActiveTab(): Promise<chrome.tabs.Tab | null> {
  // Prefer the window of the most recent action invocation so a global side
  // panel never resolves an unrelated window's active tab.
  if (lastActionTab) {
    const inWindow = await chrome.tabs
      .query({ active: true, windowId: lastActionTab.windowId })
      .catch((): chrome.tabs.Tab[] => []);
    if (inWindow[0]) return inWindow[0];
  }
  const tabs = await chrome.tabs
    .query({ active: true, lastFocusedWindow: true })
    .catch((): chrome.tabs.Tab[] => []);
  return tabs[0] ?? null;
}

async function restrictStorageAccess(): Promise<void> {
  try {
    const local = chrome.storage.local as unknown as {
      setAccessLevel?: (opts: { accessLevel: string }) => Promise<void>;
    };
    if (typeof local.setAccessLevel === 'function') {
      await local.setAccessLevel({ accessLevel: 'TRUSTED_CONTEXTS' });
    }
  } catch {
    // Older Chrome builds do not support storage access levels.
  }
}

// One-time setup. These settings persist across worker restarts, so they do not
// need to run on a plain idle-driven restart, and there is no boot-time
// reconcile: a worker restart must not stop valid sessions.
chrome.runtime.onInstalled.addListener(() => {
  void configurePanelBehavior();
  void restrictStorageAccess();
});
chrome.runtime.onStartup.addListener(() => {
  void configurePanelBehavior();
  void restrictStorageAccess();
});

// Explicit action invocation. The clicked tab arrives here with a fresh
// activeTab grant; we record exactly that tab and open the side panel for it.
chrome.action.onClicked.addListener((tab) => {
  if (tab.id == null) return;
  lastActionTab = { tabId: tab.id, windowId: tab.windowId };
  if (chrome.sidePanel && typeof chrome.sidePanel.open === 'function') {
    void chrome.sidePanel.open({ tabId: tab.id }).catch(() => {});
  }
});

// --- Navigation / tab lifecycle invalidation (no extra permission needed) ---

async function invalidateTabSessions(tabId: number, reason: string): Promise<void> {
  for (const [sessionId, ls] of live) {
    if (ls.tabId !== tabId) continue;
    ls.invalidated = true;
    live.delete(sessionId);
    try {
      const rec = await store.getRecord(sessionId);
      if (rec && rec.state !== 'STOPPED' && rec.state !== 'FAILED') {
        await store.setState(sessionId, 'STOPPED', 'NAVIGATED', reason);
      }
    } catch {
      // Best effort: the session is already removed from the live map.
    }
  }
}

// changeInfo.status is available without the "tabs" permission. A top-level load
// means the bound document is being replaced, so the live session ends.
chrome.tabs.onUpdated.addListener((tabId, changeInfo) => {
  if (changeInfo.status === 'loading') {
    void invalidateTabSessions(tabId, 'Tab began loading a new document; session invalidated.');
  }
});
chrome.tabs.onRemoved.addListener((tabId) => {
  void invalidateTabSessions(tabId, 'Tab was closed; session stopped.');
});
chrome.tabs.onReplaced.addListener((_addedTabId, removedTabId) => {
  void invalidateTabSessions(removedTabId, 'Tab was replaced; session stopped.');
});

// --- Trust boundary and request dispatch ---

// Only the extension's own side panel page may issue protocol requests.
function isTrustedSidePanelSender(sender: chrome.runtime.MessageSender): boolean {
  if (!sender || sender.id !== chrome.runtime.id) return false;
  if (sender.tab) return false; // came from a tab / content-script context
  const url = sender.url ?? '';
  return url.startsWith(chrome.runtime.getURL('sidepanel/'));
}

function staticError(code: ErrorCode, message: string): ProtocolResponse {
  return {
    id: 'unknown',
    success: false,
    timestamp: Date.now(),
    sessionId: 'unknown',
    error: { code, message },
  };
}

// Broadly compatible async response pattern: non-async listener, call
// sendResponse later, return literal true. Exactly one listener owns requests.
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (!isTrustedSidePanelSender(sender)) {
    sendResponse(staticError('INVALID_MESSAGE', 'Rejected request from an untrusted sender.'));
    return true;
  }
  handleRequest(message)
    .then(sendResponse)
    .catch((e: unknown) => sendResponse(staticError('ACTION_FAILED', toMessage(e))));
  return true;
});

async function handleRequest(message: unknown): Promise<ProtocolResponse> {
  const parsed = parseRequest(message);
  if (!parsed.ok) return staticError(parsed.error.code, parsed.error.message);
  const req = parsed.request;
  switch (req.kind) {
    case 'SESSION_START':
      return startSession(req);
    case 'SESSION_STOP':
      return stopSession(req);
    case 'OBSERVE_PAGE':
      return observePage(req);
    case 'EXECUTE_ACTION':
      return executeAction(req);
    case 'GET_TASK_STATE':
      return getTaskState(req);
    case 'GET_PANEL_CONTEXT':
      return getPanelContext(req);
  }
}

async function transition(
  sessionId: string,
  to: TaskState,
  eventType: string,
  detail?: string,
  pending?: { action: string | null },
): Promise<{ ok: true; rec: TaskRecord } | { ok: false; error: StructuredError }> {
  const rec = await store.getRecord(sessionId);
  if (!rec) return { ok: false, error: errorOf('SESSION_NOT_FOUND', 'No such session.') };
  if (!canTransition(rec.state, to)) {
    return { ok: false, error: errorOf('INVALID_STATE', `Cannot move ${rec.state} -> ${to}.`) };
  }
  const updated = await store.setState(sessionId, to, eventType, detail, pending);
  return { ok: true, rec: updated as TaskRecord };
}

function sendToContent(tabId: number, msg: ContentRequest): Promise<ContentResult> {
  return new Promise((resolve) => {
    chrome.tabs.sendMessage(tabId, msg, (resp: unknown) => {
      if (chrome.runtime.lastError) {
        resolve({
          ok: false,
          code: 'INJECTION_FAILED',
          message: chrome.runtime.lastError.message ?? 'Content script is not reachable.',
        });
        return;
      }
      if (!resp || typeof resp !== 'object') {
        resolve({ ok: false, code: 'ACTION_FAILED', message: 'Empty content-script response.' });
        return;
      }
      resolve(resp as ContentResult);
    });
  });
}

async function probeContent(tabId: number, sessionId: string): Promise<ProbeData | null> {
  const cr = await sendToContent(tabId, { type: 'SEOUL_PROBE', sessionId });
  if (!cr.ok) return null;
  const d = cr.data as Partial<ProbeData> | undefined;
  if (!d || typeof d !== 'object') return null;
  return {
    sessionId: typeof d.sessionId === 'string' ? d.sessionId : null,
    documentToken: typeof d.documentToken === 'string' ? d.documentToken : '',
    url: typeof d.url === 'string' ? d.url : '',
    initialized: d.initialized === true,
    snapshotId: typeof d.snapshotId === 'string' ? d.snapshotId : null,
  };
}

// Probe a stored session's content script and either reconstruct the live
// binding (recover) or reconcile the session to STOPPED. Shared by panel
// rehydration and by any request whose live backing was lost to a restart, so a
// still-open panel recovers the same way a reopened one does.
async function tryRecover(
  rec: TaskRecord,
  tabId: number,
): Promise<
  | { recovered: true; rec: TaskRecord }
  | { recovered: false; rec: TaskRecord | null; error: StructuredError }
> {
  const probe = await probeContent(tabId, rec.sessionId);
  const verdict = evaluateProbe(probe, rec);
  const plan = planRecovery(rec, verdict);

  // OBSERVING recovery: drop the content script's snapshot and registry without
  // re-injecting, so the interrupted observation's snapshot cannot be reused.
  if (plan.clearSnapshot) {
    await sendToContent(tabId, { type: 'SEOUL_INIT', sessionId: rec.sessionId });
  }

  if (plan.outcome === 'recover') {
    live.set(rec.sessionId, {
      sessionId: rec.sessionId,
      tabId,
      documentToken: probe?.documentToken,
      origin: rec.origin,
      invalidated: false,
    });
    const updated = await store.applyRecovery(rec.sessionId, plan.toState, plan.event, plan.detail, plan.error);
    return { recovered: true, rec: updated ?? rec };
  }

  const stopped = await store.applyRecovery(rec.sessionId, plan.toState, plan.event, plan.detail, plan.error);
  return { recovered: false, rec: stopped, error: plan.error ?? errorOf('STALE_SESSION', plan.detail) };
}

async function ensureActiveSession(
  sessionId: string,
): Promise<{ ok: true; rec: TaskRecord; tabId: number } | { ok: false; error: StructuredError }> {
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
    // Live backing lost (worker restart). Recover if the content script is still
    // the same valid session on a compatible document; otherwise it is stopped.
    const rc = await tryRecover(rec, rec.tabId);
    if (!rc.recovered) {
      return { ok: false, error: rc.error };
    }
    ls = live.get(sessionId);
    if (!ls) return { ok: false, error: errorOf('STALE_SESSION', 'Session could not be recovered.') };
  }
  return { ok: true, rec, tabId: ls.tabId };
}

async function startSession(req: SessionStartRequest): Promise<ProtocolResponse> {
  const tab = await resolveActiveTab();
  if (!tab || tab.id == null) {
    return makeError(req, errorOf('TAB_MISMATCH', 'No active tab to start a session on.'));
  }
  // The panel echoes back the tab id it was shown; it must match the trusted
  // active tab. The side panel cannot nominate an arbitrary tab.
  if (tab.id !== req.tabId) {
    return makeError(
      req,
      errorOf('TAB_MISMATCH', 'The active tab changed; reopen the panel on the page you want.'),
      tab.id,
    );
  }

  const access = resolveAccess({ tabId: tab.id, url: tab.url, title: tab.title });
  if (access.access === 'ACCESS_REQUIRED') {
    // No grant: never create a session record or a FAILED task for missing access.
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
    live.set(req.sessionId, {
      sessionId: req.sessionId,
      tabId: tab.id,
      documentToken,
      origin,
      invalidated: false,
    });
    const t = await transition(req.sessionId, 'READY', 'SESSION_READY', `Injected into tab ${tab.id}.`);
    if (!t.ok) return makeError(req, t.error, tab.id);
    return makeSuccess(
      req,
      { state: 'READY' as TaskState, tabId: tab.id, url, title: tab.title ?? '' },
      tab.id,
    );
  } catch (e) {
    const err = errorOf('INJECTION_FAILED', toMessage(e));
    live.delete(req.sessionId);
    await store.setState(req.sessionId, 'FAILED', 'INJECTION_FAILED', err.message);
    await store.recordError(req.sessionId, err);
    return makeError(req, err, tab.id);
  }
}

async function stopSession(req: SessionStopRequest): Promise<ProtocolResponse> {
  const rec = await store.getRecord(req.sessionId);
  if (!rec) return makeError(req, errorOf('SESSION_NOT_FOUND', 'No such session.'));
  live.delete(req.sessionId);
  if (rec.state !== 'STOPPED' && rec.state !== 'FAILED') {
    await store.setState(req.sessionId, 'STOPPED', 'SESSION_STOPPED', 'Stopped by user.');
  }
  return makeSuccess(req, { state: 'STOPPED' as TaskState }, rec.tabId);
}

async function observePage(req: ObservePageRequest): Promise<ProtocolResponse> {
  const s = await ensureActiveSession(req.sessionId);
  if (!s.ok) {
    await store.recordError(req.sessionId, s.error);
    return makeError(req, s.error);
  }
  const t = await transition(req.sessionId, 'OBSERVING', 'OBSERVE_BEGIN');
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

async function executeAction(req: ExecuteActionRequest): Promise<ProtocolResponse> {
  const s = await ensureActiveSession(req.sessionId);
  if (!s.ok) {
    await store.recordError(req.sessionId, s.error);
    return makeError(req, s.error);
  }
  const action = req.action;
  // Persist only the non-sensitive action kind, atomically with the EXECUTING
  // transition, so an interrupted action can be reported on recovery.
  const t = await transition(req.sessionId, 'EXECUTING', 'EXECUTE_BEGIN', action.kind, {
    action: action.kind,
  });
  if (!t.ok) return makeError(req, t.error, s.tabId);

  if (action.kind === 'NAVIGATE') {
    const v = validateNavigationUrl(action.url);
    if (!v.ok) {
      await store.setState(req.sessionId, 'READY', 'EXECUTE_END', 'NAVIGATE', { action: null });
      await store.recordError(req.sessionId, v.error);
      return makeError(req, v.error, s.tabId);
    }
    const target = safeOrigin(v.url) || 'the target origin';
    await chrome.tabs.update(s.tabId, { url: v.url });
    // Navigation invalidates the injected content script: end the session.
    live.delete(req.sessionId);
    await store.setState(
      req.sessionId,
      'STOPPED',
      'NAVIGATED',
      `Navigated to ${target}; session ended, restart required.`,
      { action: null },
    );
    return makeSuccess(
      req,
      { navigated: true, state: 'STOPPED' as TaskState, restartRequired: true },
      s.tabId,
    );
  }

  let cr: ContentResult;
  if (action.kind === 'CLICK_ELEMENT') {
    cr = await sendToContent(s.tabId, {
      type: 'SEOUL_CLICK',
      sessionId: req.sessionId,
      elementId: action.elementId,
      documentToken: action.documentToken,
      snapshotId: action.snapshotId,
    });
  } else if (action.kind === 'TYPE_TEXT') {
    cr = await sendToContent(s.tabId, {
      type: 'SEOUL_TYPE',
      sessionId: req.sessionId,
      elementId: action.elementId,
      text: action.text,
      documentToken: action.documentToken,
      snapshotId: action.snapshotId,
    });
  } else {
    cr = await sendToContent(s.tabId, {
      type: 'SEOUL_SCROLL',
      sessionId: req.sessionId,
      direction: action.direction,
      amount: action.amount,
      documentToken: action.documentToken,
    });
  }

  // Record only the action kind in the timeline - never the typed text - and
  // clear the pending action atomically with the completion transition.
  await store.setState(req.sessionId, 'READY', 'EXECUTE_END', action.kind, { action: null });

  if (!cr.ok) {
    const err = errorOf(cr.code, cr.message);
    await store.recordError(req.sessionId, err);
    return makeError(req, err, s.tabId);
  }
  return makeSuccess(req, { result: cr.data }, s.tabId);
}

async function getTaskState(req: GetTaskStateRequest): Promise<ProtocolResponse> {
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

// The background determines the active tab itself and reports its access state.
// Sessions are only resolved/recovered for a granted, supported tab. The side
// panel passes no tab id, so it can never restore a session for another tab.
async function getPanelContext(req: GetPanelContextRequest): Promise<ProtocolResponse> {
  const tab = await resolveActiveTab();
  const tabId = tab?.id ?? null;
  const access = resolveAccess({ tabId, url: tab?.url, title: tab?.title });

  // Compute the internal page-control attachment. Recovery (probe) happens here
  // and records SESSION_RECOVERED / RECONCILED into the control-session history,
  // but the historical record is never exposed to the panel. A terminal record
  // stays detached (history).
  let attachment: AttachmentInfo = { sessionId: null, attached: false };
  if (access.access === 'GRANTED' && tabId != null) {
    const records = await store.listRecords();
    const picked = pickLatestSessionForTab(records, tabId);
    if (picked) {
      const ls = live.get(picked.sessionId);
      const isLive = !!ls && ls.tabId === tabId && !ls.invalidated;
      const classification = classifyActiveSession(picked, isLive);
      if (classification.live) {
        attachment = { sessionId: picked.sessionId, attached: true };
      } else if (classification.needsProbe) {
        const rc = await tryRecover(picked, tabId);
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
