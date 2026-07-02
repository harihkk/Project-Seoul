// Project Seoul Development Harness - content: per-document session state.
//
// STATE OWNERSHIP (product-wide state rules):
//   owner:           this module, via a single window.__seoulHarness holder.
//   lifetime:        one document (isolated world). Replaced when the document
//                    navigates (a fresh content script and token are minted).
//   persistence:     document-scoped ephemeral, isolated-world memory only.
//                    Never written into the page DOM; survives a service-worker
//                    restart, which is how the background can probe and recover.
//   recovery:        the background re-probes this state after a worker restart.
//   teardown:        discarded with the document; element identity is reset on
//                    every SEOUL_INIT and observation.
//   bounds:          element registry bounded by the observation element cap.
//   observers:       one runtime.onMessage listener, installed once.
//   isolation:       per document; never exposed to the page's JS context.
//
// The content script runs in Chrome's isolated world and is bundled to a single
// classic script, so it can import the shared validation source of truth
// (../validation.ts) instead of mirroring it.

// A random, unpredictable token (never a counter). crypto.randomUUID requires a
// secure context; fall back to getRandomValues, available everywhere.
export function randomToken(): string {
  if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
    return crypto.randomUUID();
  }
  const bytes = new Uint8Array(16);
  crypto.getRandomValues(bytes);
  let hex = '';
  for (const b of bytes) hex += b.toString(16).padStart(2, '0');
  return hex;
}

export interface Registry {
  idToEl: Map<string, Element>;
  elToId: WeakMap<Element, string>;
  counter: number;
}

export interface HarnessState {
  registry: Registry;
  listenerInstalled: boolean;
  sessionId: string | null;
  documentToken: string;
  snapshotId: string | null;
  initializedAt: number;
}

export function freshRegistry(): Registry {
  return { idToEl: new Map(), elToId: new WeakMap(), counter: 0 };
}

// Lazily initialize and return the per-document singleton. On re-injection into
// the same live document, reset element identity but keep the document token
// (the document has not changed); the session id and snapshot are reset by the
// SEOUL_INIT that follows injection.
export function getState(): HarnessState {
  const holder = window as unknown as { __seoulHarness?: HarnessState };
  if (!holder.__seoulHarness) {
    holder.__seoulHarness = {
      registry: freshRegistry(),
      listenerInstalled: false,
      sessionId: null,
      documentToken: randomToken(),
      snapshotId: null,
      initializedAt: Date.now(),
    };
  } else {
    holder.__seoulHarness.registry = freshRegistry();
  }
  return holder.__seoulHarness;
}

export function assignId(reg: Registry, el: Element): string {
  const existing = reg.elToId.get(el);
  if (existing) return existing;
  const id = `seoul-${reg.counter++}`;
  reg.elToId.set(el, id);
  reg.idToEl.set(id, el);
  return id;
}

export function resolveElement(state: HarnessState, elementId: string): Element | null {
  return state.registry.idToEl.get(elementId) ?? null;
}
