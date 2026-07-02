// Project Seoul Development Harness - panel invocation context store.
//
// STATE OWNERSHIP (product-wide state rules):
//   owner:           this module (single functional store; no free globals).
//   lifetime:        one browser session.
//   persistence:     browser-session state, chrome.storage.session (trusted
//                    contexts only). Survives service-worker suspension;
//                    cleared automatically on browser close and on extension
//                    update/reload.
//   recovery:        re-read from storage.session after a worker restart;
//                    every read is revalidated against current Chrome state.
//   teardown:        cleared on tab close, window close, or when the referenced
//                    tab moves to a window that no longer matches.
//   bounds:          exactly one record (the latest toolbar invocation).
//   observers:       none owned here.
//   isolation:       per extension installation; not exposed to any page.
//
// It replaces the former `lastActionTab` module-global, which was lost on every
// service-worker restart. It persists ONLY non-sensitive routing hints:
// tab id, window id, an invocation timestamp, and an optional non-secret
// correlation id. It never stores page content, activeTab grants, credentials,
// or DOM state, and a stored record is NEVER treated as an access grant: the
// access decision always comes from live Chrome state plus the user's explicit
// activeTab grant (see resolveAccess / resolveActiveTab).

const STORAGE_KEY = 'seoul.panelInvocation.v1';

// A recorded invocation older than this is stale and ignored on read. Short,
// because it only scopes active-tab resolution to the window the user just
// acted in; a genuinely current invocation is always well under a minute old.
export const INVOCATION_TTL_MS = 5 * 60 * 1000;

export interface PanelInvocationContext {
  tabId: number;
  windowId: number;
  invokedAt: number;
  correlationId?: string;
}

// The Chrome surface this store depends on, narrowed so it can be unit-tested
// with an in-memory fake and so it never reaches for anything beyond
// storage.session and read-only tab/window lookups.
export interface InvocationChrome {
  storageSessionGet(key: string): Promise<Record<string, unknown>>;
  storageSessionSet(items: Record<string, unknown>): Promise<void>;
  storageSessionRemove(key: string): Promise<void>;
  now(): number;
}

// Adapts the real chrome.* APIs to InvocationChrome. Guards storage.session
// availability (older builds lack it) so the store degrades to "no context"
// rather than throwing.
export function defaultInvocationChrome(): InvocationChrome {
  return {
    async storageSessionGet(key) {
      const session = chrome.storage?.session;
      if (!session) return {};
      return session.get(key);
    },
    async storageSessionSet(items) {
      const session = chrome.storage?.session;
      if (!session) return;
      await session.set(items);
    },
    async storageSessionRemove(key) {
      const session = chrome.storage?.session;
      if (!session) return;
      await session.remove(key);
    },
    now() {
      return Date.now();
    },
  };
}

function isValidRecord(value: unknown): value is PanelInvocationContext {
  if (!value || typeof value !== 'object') return false;
  const r = value as Record<string, unknown>;
  if (typeof r.tabId !== 'number' || !Number.isInteger(r.tabId)) return false;
  if (typeof r.windowId !== 'number' || !Number.isInteger(r.windowId)) return false;
  if (typeof r.invokedAt !== 'number' || !Number.isFinite(r.invokedAt)) return false;
  if (r.correlationId !== undefined && typeof r.correlationId !== 'string') return false;
  return true;
}

export class PanelInvocationContextStore {
  readonly #env: InvocationChrome;

  constructor(env: InvocationChrome = defaultInvocationChrome()) {
    this.#env = env;
  }

  // Records a toolbar invocation, overwriting any previous record. Called only
  // from the trusted action.onClicked handler.
  async record(context: {
    tabId: number;
    windowId: number;
    correlationId?: string;
  }): Promise<void> {
    const record: PanelInvocationContext = {
      tabId: context.tabId,
      windowId: context.windowId,
      invokedAt: this.#env.now(),
    };
    if (context.correlationId !== undefined) {
      record.correlationId = context.correlationId;
    }
    await this.#env.storageSessionSet({ [STORAGE_KEY]: record });
  }

  // Returns the raw stored record if present and well-formed and within TTL,
  // else clears it and returns null. Does no tab/window liveness check; use
  // resolveValid for the fully validated read.
  async peek(): Promise<PanelInvocationContext | null> {
    const got = await this.#env.storageSessionGet(STORAGE_KEY);
    const value = got[STORAGE_KEY];
    if (!isValidRecord(value)) {
      if (value !== undefined) await this.clear();
      return null;
    }
    if (this.#env.now() - value.invokedAt > INVOCATION_TTL_MS) {
      await this.clear();
      return null;
    }
    return value;
  }

  // Fully validated read: TTL, the referenced tab still exists, and it is still
  // in the recorded window. `verify` is the live-state probe (tab lookup); it
  // returns the tab's current windowId or null if the tab is gone. Any failure
  // clears the record and returns null, so stale context can never scope a
  // resolution or, worse, be mistaken for access.
  async resolveValid(
    verify: (tabId: number) => Promise<{ windowId: number } | null>,
  ): Promise<PanelInvocationContext | null> {
    const record = await this.peek();
    if (!record) return null;
    const live = await verify(record.tabId).catch(() => null);
    if (!live) {
      await this.clear();
      return null;
    }
    if (live.windowId !== record.windowId) {
      // The tab moved windows: the recorded window hint is no longer valid.
      await this.clear();
      return null;
    }
    return record;
  }

  async clear(): Promise<void> {
    await this.#env.storageSessionRemove(STORAGE_KEY);
  }

  // Lifecycle hooks: clear when the referenced tab or window goes away.
  async onTabRemoved(tabId: number): Promise<void> {
    const record = await this.peek();
    if (record && record.tabId === tabId) await this.clear();
  }

  async onWindowRemoved(windowId: number): Promise<void> {
    const record = await this.peek();
    if (record && record.windowId === windowId) await this.clear();
  }
}
