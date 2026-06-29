// Project Seoul Development Harness - persisted control-session state.
//
// Stores only non-sensitive control-session metadata in chrome.storage.local. Page text,
// semantic snapshots, typed text, input values, page HTML and any secrets are
// never written here. The timeline is bounded to the most recent N events.
//
// All mutations run through a single serialized queue (withLock) so a
// read-modify-write cannot interleave with another and lose an update. A failed
// write rejects rather than being swallowed.

import type { StructuredError, ControlSessionRecord, ControlSessionState, SessionTimelineEvent } from './protocol.ts';
import { LIMITS, truncateTimeline } from './validation.ts';

const STORAGE_KEY = 'seoul.sessions.v1';

type SessionMap = Record<string, ControlSessionRecord>;

// Serialize every mutation. Each runs only after the previous settles, so reads
// inside a mutation always see the prior mutation's committed write.
let writeChain: Promise<unknown> = Promise.resolve();
function withLock<T>(fn: () => Promise<T>): Promise<T> {
  const run = writeChain.then(fn, fn);
  writeChain = run.then(
    () => undefined,
    () => undefined,
  );
  return run;
}

async function readAll(): Promise<SessionMap> {
  const got = await chrome.storage.local.get(STORAGE_KEY);
  const map = got[STORAGE_KEY];
  if (map && typeof map === 'object') return map as SessionMap;
  return {};
}

async function writeAll(map: SessionMap): Promise<void> {
  await chrome.storage.local.set({ [STORAGE_KEY]: map });
}

function pushEvent(rec: ControlSessionRecord, event: SessionTimelineEvent): void {
  rec.timeline.push(event);
  rec.timeline = truncateTimeline(rec.timeline, LIMITS.MAX_TIMELINE_EVENTS);
}

// Reads are single atomic get() calls and do not need the lock.
export async function getRecord(sessionId: string): Promise<ControlSessionRecord | null> {
  const all = await readAll();
  return all[sessionId] ?? null;
}

export async function listRecords(): Promise<ControlSessionRecord[]> {
  const all = await readAll();
  return Object.values(all);
}

export async function createRecord(
  sessionId: string,
  tabId: number,
  origin?: string,
  documentId?: string,
): Promise<ControlSessionRecord> {
  return withLock(async () => {
    const all = await readAll();
    const now = Date.now();
    const record: ControlSessionRecord = {
      sessionId,
      tabId,
      origin,
      documentId,
      state: 'STARTING',
      createdAt: now,
      updatedAt: now,
      timeline: [{ at: now, type: 'SESSION_CREATED', state: 'STARTING' }],
      lastError: null,
    };
    all[sessionId] = record;
    await writeAll(all);
    return record;
  });
}

// `pending.action` sets the persisted EXECUTING action kind (a string) or clears
// it (null), atomically with the state transition. Only the kind is stored.
export async function setState(
  sessionId: string,
  state: ControlSessionState,
  eventType: string,
  detail?: string,
  pending?: { action: string | null },
): Promise<ControlSessionRecord | null> {
  return withLock(async () => {
    const all = await readAll();
    const rec = all[sessionId];
    if (!rec) return null;
    rec.state = state;
    rec.updatedAt = Date.now();
    if (pending) {
      if (pending.action === null) delete rec.pendingAction;
      else rec.pendingAction = pending.action;
    }
    pushEvent(rec, { at: rec.updatedAt, type: eventType, detail, state });
    all[sessionId] = rec;
    await writeAll(all);
    return rec;
  });
}

export async function recordError(sessionId: string, error: StructuredError): Promise<void> {
  return withLock(async () => {
    const all = await readAll();
    const rec = all[sessionId];
    if (!rec) return;
    rec.lastError = error;
    rec.updatedAt = Date.now();
    pushEvent(rec, { at: rec.updatedAt, type: 'ERROR', detail: `${error.code}: ${error.message}` });
    all[sessionId] = rec;
    await writeAll(all);
  });
}

export async function appendEvent(
  sessionId: string,
  type: string,
  detail?: string,
): Promise<void> {
  return withLock(async () => {
    const all = await readAll();
    const rec = all[sessionId];
    if (!rec) return;
    rec.updatedAt = Date.now();
    pushEvent(rec, { at: rec.updatedAt, type, detail });
    all[sessionId] = rec;
    await writeAll(all);
  });
}

// Commit a state-aware recovery decision atomically: set the resulting state,
// record the structured error if any, clear any pending action, and append the
// single recovery event. The background decides the plan (per transient-state
// policy); this only writes it. Called once per recovery, gated by the live /
// terminal classification, so the event is not duplicated on panel refreshes.
export async function applyRecovery(
  sessionId: string,
  toState: ControlSessionState,
  eventType: string,
  detail: string,
  error: StructuredError | null,
): Promise<ControlSessionRecord | null> {
  return withLock(async () => {
    const all = await readAll();
    const rec = all[sessionId];
    if (!rec) return null;
    rec.state = toState;
    rec.updatedAt = Date.now();
    if (error) rec.lastError = error;
    delete rec.pendingAction;
    pushEvent(rec, { at: rec.updatedAt, type: eventType, detail, state: toState });
    all[sessionId] = rec;
    await writeAll(all);
    return rec;
  });
}
