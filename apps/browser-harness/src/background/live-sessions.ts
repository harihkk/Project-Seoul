// Project Seoul Development Harness - live session bindings.
//
// STATE OWNERSHIP (product-wide state rules):
//   owner:           this module (a single LiveSessionStore instance).
//   lifetime:        one service-worker generation.
//   persistence:     per-worker ephemeral, in-memory only. Deliberately NOT
//                    recovered: after a worker restart the map is empty and a
//                    stored session is re-probed before it is trusted, so a
//                    stale live binding can never be resurrected from storage.
//   recovery:        none by design (see session-recovery.ts, which rebuilds a
//                    binding only after a successful content-script probe).
//   teardown:        entries removed on navigation, tab close/replace, stop,
//                    and failed start.
//   bounds:          one entry per active session; sessions are user-created
//                    and bounded by open tabs.
//   observers:       none owned here.
//   isolation:       per worker; not shared across profiles (each profile runs
//                    its own worker).
//
// A LiveSession is the trusted in-memory backing for a session record: proof
// that this worker injected and initialized the content script for that tab.

export interface LiveSession {
  sessionId: string;
  tabId: number;
  documentToken?: string;
  origin?: string;
  invalidated?: boolean;
}

export class LiveSessionStore {
  readonly #live = new Map<string, LiveSession>();

  set(session: LiveSession): void {
    this.#live.set(session.sessionId, session);
  }

  get(sessionId: string): LiveSession | undefined {
    return this.#live.get(sessionId);
  }

  // True when a session has a non-invalidated live binding on the given tab.
  isLiveOnTab(sessionId: string, tabId: number): boolean {
    const ls = this.#live.get(sessionId);
    return !!ls && ls.tabId === tabId && !ls.invalidated;
  }

  delete(sessionId: string): void {
    this.#live.delete(sessionId);
  }

  // Marks and removes every live session bound to a tab, returning their ids so
  // the caller can reconcile the durable records. The store never touches the
  // control-session store itself (no cross-module state mutation).
  invalidateTab(tabId: number): string[] {
    const affected: string[] = [];
    for (const [sessionId, ls] of this.#live) {
      if (ls.tabId !== tabId) continue;
      ls.invalidated = true;
      this.#live.delete(sessionId);
      affected.push(sessionId);
    }
    return affected;
  }
}
