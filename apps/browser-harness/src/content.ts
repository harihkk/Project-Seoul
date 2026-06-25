// Project Seoul Development Harness - content script (isolated world).
//
// Injected only after an explicit user gesture (Start session) via
// chrome.scripting. It runs in Chrome's isolated extension world: it shares the
// DOM but not the page's JavaScript context. It never listens for page
// postMessage commands and never exposes privileged functions to the page.
//
// It owns the session identity for its document: a session id supplied by the
// background, a random document token minted for this document, and the current
// observation snapshot id. This state lives only in isolated-world memory and is
// never written into the page DOM. Because it survives a service-worker restart,
// the background can probe it to recover a still-valid session.
//
// NOTE: content scripts cannot use ES module imports, so the safety-critical
// limits, field classification, and snapshot-binding logic below are a
// deliberate local mirror of `src/validation.ts`, which remains the single
// source of truth and is the version covered by the Node tests. Keep them in
// sync.

(() => {
  // --- Local protocol mirror (no imports allowed in content scripts) ---
  type ScrollDirection = 'up' | 'down';

  type ContentRequest =
    | { type: 'SEOUL_INIT'; sessionId: string }
    | { type: 'SEOUL_PROBE'; sessionId: string }
    | { type: 'SEOUL_OBSERVE'; sessionId: string }
    | { type: 'SEOUL_CLICK'; sessionId: string; elementId: string; documentToken: string; snapshotId: string }
    | { type: 'SEOUL_TYPE'; sessionId: string; elementId: string; text: string; documentToken: string; snapshotId: string }
    | { type: 'SEOUL_SCROLL'; sessionId: string; direction: ScrollDirection; amount: number; documentToken: string };

  type ErrorCode =
    | 'ELEMENT_NOT_FOUND'
    | 'ELEMENT_NOT_INTERACTABLE'
    | 'SENSITIVE_FIELD'
    | 'STALE_DOCUMENT'
    | 'STALE_SNAPSHOT'
    | 'ACTION_FAILED'
    | 'INVALID_MESSAGE';

  type ContentResult =
    | { ok: true; data: unknown }
    | { ok: false; code: ErrorCode; message: string };

  interface ElementRect {
    x: number;
    y: number;
    width: number;
    height: number;
  }
  interface InteractiveElement {
    id: string;
    role: string;
    name?: string;
    tag: string;
    inputType?: string;
    disabled: boolean;
    checked?: boolean;
    selected?: boolean;
    visible: boolean;
    rect: ElementRect;
  }
  interface PageSnapshot {
    url: string;
    title: string;
    lang: string;
    viewport: { width: number; height: number };
    elements: InteractiveElement[];
    textBlocks: string[];
    timestamp: number;
    truncated: boolean;
    documentToken: string;
    snapshotId: string;
  }

  const LIMITS = {
    MAX_INTERACTIVE_ELEMENTS: 200,
    MAX_TEXT_BLOCKS: 100,
    MAX_TOTAL_TEXT_CHARS: 30_000,
    MAX_TEXT_BLOCK_CHARS: 2_000,
    SCROLL_MIN: 0,
    SCROLL_MAX: 20_000,
    SCROLL_DEFAULT: 600,
  };

  const TEXT_CAPABLE_INPUT_TYPES = ['', 'text', 'search', 'url', 'tel', 'email', 'number'];

  // Random, unpredictable token (never a counter). crypto.randomUUID requires a
  // secure context; fall back to getRandomValues, which is available everywhere.
  function randomToken(): string {
    if (typeof crypto !== 'undefined' && typeof crypto.randomUUID === 'function') {
      return crypto.randomUUID();
    }
    const bytes = new Uint8Array(16);
    crypto.getRandomValues(bytes);
    let hex = '';
    for (const b of bytes) hex += b.toString(16).padStart(2, '0');
    return hex;
  }

  function clampScroll(amount: unknown): number {
    const n = typeof amount === 'number' ? amount : Number(amount);
    if (!Number.isFinite(n)) return LIMITS.SCROLL_DEFAULT;
    return Math.min(Math.max(Math.round(n), LIMITS.SCROLL_MIN), LIMITS.SCROLL_MAX);
  }

  interface FieldDescriptor {
    tag: string;
    type: string;
    disabled: boolean;
    readOnly: boolean;
    connected: boolean;
    visible: boolean;
    valueLength: number;
  }

  function classifyTypeTarget(
    f: FieldDescriptor,
  ): { allowed: true } | { allowed: false; code: ErrorCode; message: string } {
    const tag = f.tag.toLowerCase();
    if (tag !== 'input' && tag !== 'textarea') {
      return { allowed: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Only <input> and <textarea> accept typed text.' };
    }
    const type = (f.type || '').toLowerCase();
    if (tag === 'input') {
      if (type === 'password') {
        return { allowed: false, code: 'SENSITIVE_FIELD', message: 'Refusing to type into a password field.' };
      }
      if (type === 'file') {
        return { allowed: false, code: 'SENSITIVE_FIELD', message: 'Refusing to interact with a file input.' };
      }
      if (type === 'hidden') {
        return { allowed: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Hidden inputs cannot receive typed text.' };
      }
    }
    if (!f.connected) {
      return { allowed: false, code: 'ELEMENT_NOT_FOUND', message: 'Element is no longer connected.' };
    }
    if (!f.visible) {
      return { allowed: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is not visible.' };
    }
    if (f.disabled) {
      return { allowed: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is disabled.' };
    }
    if (f.readOnly) {
      return { allowed: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is read-only.' };
    }
    if (tag === 'input' && !TEXT_CAPABLE_INPUT_TYPES.includes(type)) {
      return { allowed: false, code: 'ELEMENT_NOT_INTERACTABLE', message: `Input type "${type}" does not accept free text.` };
    }
    if (f.valueLength > 0) {
      return {
        allowed: false,
        code: 'ACTION_FAILED',
        message: 'Field already contains a value; clearing is not supported in this milestone.',
      };
    }
    return { allowed: true };
  }

  // --- Per-document state and ephemeral element registry ---
  interface Registry {
    idToEl: Map<string, Element>;
    elToId: WeakMap<Element, string>;
    counter: number;
  }
  interface HarnessState {
    registry: Registry;
    listenerInstalled: boolean;
    sessionId: string | null;
    documentToken: string;
    snapshotId: string | null;
    initializedAt: number;
  }

  function freshRegistry(): Registry {
    return { idToEl: new Map(), elToId: new WeakMap(), counter: 0 };
  }

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
    // Reinjection into the same live document: reset element identity but keep
    // the document token (the document has not changed). The session id and
    // snapshot are reset by the SEOUL_INIT that follows injection.
    holder.__seoulHarness.registry = freshRegistry();
  }
  const state = holder.__seoulHarness;

  function assignId(reg: Registry, el: Element): string {
    const existing = reg.elToId.get(el);
    if (existing) return existing;
    const id = `seoul-${reg.counter++}`;
    reg.elToId.set(el, id);
    reg.idToEl.set(id, el);
    return id;
  }

  // Local mirror of validation.checkSnapshotBinding.
  function checkBinding(
    documentToken: string,
    snapshotId: string | undefined,
    requireSnapshot: boolean,
  ): ContentResult | null {
    if (!state.documentToken || state.documentToken !== documentToken) {
      return { ok: false, code: 'STALE_DOCUMENT', message: 'Document token does not match the current document.' };
    }
    if (requireSnapshot) {
      if (!state.snapshotId) {
        return { ok: false, code: 'STALE_SNAPSHOT', message: 'No current observation snapshot; observe the page first.' };
      }
      if (snapshotId === undefined || state.snapshotId !== snapshotId) {
        return { ok: false, code: 'STALE_SNAPSHOT', message: 'Snapshot id does not match the latest observation.' };
      }
    }
    return null;
  }

  // --- Visibility / semantics ---

  function isVisible(el: Element): boolean {
    if (!el.isConnected) return false;
    if (el.getClientRects().length === 0) return false;
    const style = getComputedStyle(el);
    if (style.display === 'none') return false;
    if (style.visibility === 'hidden' || style.visibility === 'collapse') return false;
    if (parseFloat(style.opacity || '1') === 0) return false;
    const r = el.getBoundingClientRect();
    return r.width > 0 && r.height > 0;
  }

  function isDisabled(el: Element): boolean {
    const anyEl = el as unknown as { disabled?: boolean };
    if (typeof anyEl.disabled === 'boolean' && anyEl.disabled) return true;
    return el.getAttribute('aria-disabled') === 'true';
  }

  function clip(text: string, max: number): string {
    const t = text.replace(/\s+/g, ' ').trim();
    return t.length > max ? t.slice(0, max) : t;
  }

  function accessibleName(el: Element): string | undefined {
    const aria = el.getAttribute('aria-label');
    if (aria && aria.trim()) return clip(aria, 120);

    const labelledby = el.getAttribute('aria-labelledby');
    if (labelledby) {
      const text = labelledby
        .split(/\s+/)
        .map((id) => document.getElementById(id)?.textContent ?? '')
        .join(' ')
        .trim();
      if (text) return clip(text, 120);
    }

    const labelled = el as unknown as { labels?: ArrayLike<Element> | null };
    if (labelled.labels && labelled.labels.length > 0) {
      const text = Array.from(labelled.labels)
        .map((l) => l.textContent ?? '')
        .join(' ')
        .trim();
      if (text) return clip(text, 120);
    }

    const placeholder = el.getAttribute('placeholder');
    if (placeholder && placeholder.trim()) return clip(placeholder, 120);

    const title = el.getAttribute('title');
    if (title && title.trim()) return clip(title, 120);

    const alt = el.getAttribute('alt');
    if (alt && alt.trim()) return clip(alt, 120);

    const tag = el.tagName.toLowerCase();
    const role = el.getAttribute('role');
    if (tag === 'a' || tag === 'button' || role === 'button' || role === 'link') {
      const t = (el.textContent ?? '').trim();
      if (t) return clip(t, 120);
    }
    return undefined;
  }

  function semanticRole(el: Element): string {
    const explicit = el.getAttribute('role');
    if (explicit && explicit.trim()) return explicit.trim().toLowerCase();
    const tag = el.tagName.toLowerCase();
    if (tag === 'a') return 'link';
    if (tag === 'button') return 'button';
    if (tag === 'select') return el.hasAttribute('multiple') ? 'listbox' : 'combobox';
    if (tag === 'textarea') return 'textbox';
    if (tag === 'input') {
      const type = (el as HTMLInputElement).type.toLowerCase();
      if (type === 'checkbox') return 'checkbox';
      if (type === 'radio') return 'radio';
      if (type === 'button' || type === 'submit' || type === 'reset') return 'button';
      if (type === 'range') return 'slider';
      if (type === 'search') return 'searchbox';
      return 'textbox';
    }
    return 'generic';
  }

  const INTERACTIVE_ROLE_ATTRS = new Set([
    'button',
    'link',
    'checkbox',
    'radio',
    'textbox',
    'combobox',
    'listbox',
    'menuitem',
    'tab',
    'switch',
    'option',
    'slider',
    'searchbox',
  ]);

  function collectInteractive(reg: Registry): { elements: InteractiveElement[]; truncated: boolean } {
    const selector = 'a[href], button, input, textarea, select, [role], [tabindex]';
    const seen = new Set<Element>();
    const out: InteractiveElement[] = [];
    let truncated = false;

    const candidates = Array.from(document.querySelectorAll(selector));
    for (const el of candidates) {
      if (seen.has(el)) continue;
      seen.add(el);

      const tag = el.tagName.toLowerCase();
      // Never surface hidden inputs.
      if (tag === 'input' && (el as HTMLInputElement).type.toLowerCase() === 'hidden') continue;

      const role = semanticRole(el);
      const roleAttr = el.getAttribute('role');
      const tabindexAttr = el.getAttribute('tabindex');

      const isNativeInteractive =
        tag === 'a' || tag === 'button' || tag === 'input' || tag === 'textarea' || tag === 'select';
      const hasInteractiveRole = roleAttr ? INTERACTIVE_ROLE_ATTRS.has(role) : false;
      const hasNonNegativeTabIndex = tabindexAttr !== null && Number(tabindexAttr) >= 0;

      if (!isNativeInteractive && !hasInteractiveRole && !hasNonNegativeTabIndex) continue;
      if (!isVisible(el)) continue;

      if (out.length >= LIMITS.MAX_INTERACTIVE_ELEMENTS) {
        truncated = true;
        break;
      }

      const rect = el.getBoundingClientRect();
      const item: InteractiveElement = {
        id: assignId(reg, el),
        role,
        tag,
        disabled: isDisabled(el),
        visible: true,
        rect: {
          x: Math.round(rect.x),
          y: Math.round(rect.y),
          width: Math.round(rect.width),
          height: Math.round(rect.height),
        },
      };

      const name = accessibleName(el);
      if (name) item.name = name;

      if (tag === 'input') {
        const input = el as HTMLInputElement;
        item.inputType = input.type.toLowerCase();
        if (input.type === 'checkbox' || input.type === 'radio') item.checked = input.checked;
      }
      if (tag === 'option') item.selected = (el as HTMLOptionElement).selected;

      out.push(item);
    }
    return { elements: out, truncated };
  }

  const SKIP_TEXT_TAGS = new Set(['SCRIPT', 'STYLE', 'NOSCRIPT', 'TEMPLATE']);
  const TEXT_BLOCK_SELECTOR =
    'p, h1, h2, h3, h4, h5, h6, li, blockquote, td, th, figcaption, summary, label, dt, dd';

  function collectText(): { blocks: string[]; truncated: boolean } {
    const blocks: string[] = [];
    const seen = new Set<string>();
    let total = 0;
    let truncated = false;

    const nodes = Array.from(document.body?.querySelectorAll(TEXT_BLOCK_SELECTOR) ?? []);
    for (const el of nodes) {
      if (SKIP_TEXT_TAGS.has(el.tagName)) continue;
      if (!isVisible(el)) continue;

      const normalized = clip(el.textContent ?? '', LIMITS.MAX_TEXT_BLOCK_CHARS);
      if (!normalized) continue;
      if (seen.has(normalized)) continue;
      seen.add(normalized);

      if (blocks.length >= LIMITS.MAX_TEXT_BLOCKS) {
        truncated = true;
        break;
      }
      if (total + normalized.length > LIMITS.MAX_TOTAL_TEXT_CHARS) {
        truncated = true;
        break;
      }
      blocks.push(normalized);
      total += normalized.length;
    }
    return { blocks, truncated };
  }

  function observe(): PageSnapshot {
    // A new observation replaces the snapshot: regenerate the snapshot id and
    // reset element identity so prior ephemeral ids cannot be reused.
    state.registry = freshRegistry();
    state.snapshotId = randomToken();
    const interactive = collectInteractive(state.registry);
    const text = collectText();
    return {
      url: location.href,
      title: document.title,
      lang: document.documentElement.lang || '',
      viewport: { width: window.innerWidth, height: window.innerHeight },
      elements: interactive.elements,
      textBlocks: text.blocks,
      timestamp: Date.now(),
      truncated: interactive.truncated || text.truncated,
      documentToken: state.documentToken,
      snapshotId: state.snapshotId,
    };
  }

  // --- Actions ---

  function resolve(elementId: string): Element | null {
    return state.registry.idToEl.get(elementId) ?? null;
  }

  function clickElement(elementId: string, documentToken: string, snapshotId: string): ContentResult {
    const binding = checkBinding(documentToken, snapshotId, true);
    if (binding) return binding;
    const el = resolve(elementId);
    if (!el || !el.isConnected) {
      return { ok: false, code: 'ELEMENT_NOT_FOUND', message: 'Unknown or detached element id.' };
    }
    if (!isVisible(el)) {
      return { ok: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is not visible.' };
    }
    if (isDisabled(el)) {
      return { ok: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is disabled.' };
    }
    el.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    (el as HTMLElement).click();
    return { ok: true, data: { clicked: true, id: elementId } };
  }

  function setNativeValue(el: HTMLInputElement | HTMLTextAreaElement, value: string): void {
    const proto =
      el instanceof HTMLTextAreaElement ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
    const desc = Object.getOwnPropertyDescriptor(proto, 'value');
    if (desc && typeof desc.set === 'function') {
      desc.set.call(el, value);
    } else {
      el.value = value;
    }
  }

  function typeText(
    elementId: string,
    text: string,
    documentToken: string,
    snapshotId: string,
  ): ContentResult {
    const binding = checkBinding(documentToken, snapshotId, true);
    if (binding) return binding;
    const el = resolve(elementId);
    if (!el) {
      return { ok: false, code: 'ELEMENT_NOT_FOUND', message: 'Unknown element id.' };
    }
    const tag = el.tagName.toLowerCase();
    const typed = el as HTMLInputElement | HTMLTextAreaElement;
    const descriptor: FieldDescriptor = {
      tag,
      type: tag === 'input' ? (el as HTMLInputElement).type.toLowerCase() : '',
      disabled: isDisabled(el),
      readOnly: 'readOnly' in typed ? Boolean(typed.readOnly) : false,
      connected: el.isConnected,
      visible: isVisible(el),
      // Length only - the field's existing value is never read into a string we
      // keep, transmit, or persist.
      valueLength: 'value' in typed ? String(typed.value).length : 0,
    };
    const verdict = classifyTypeTarget(descriptor);
    if (!verdict.allowed) {
      return { ok: false, code: verdict.code, message: verdict.message };
    }
    el.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    setNativeValue(typed, text);
    typed.dispatchEvent(new Event('input', { bubbles: true }));
    typed.dispatchEvent(new Event('change', { bubbles: true }));
    return { ok: true, data: { typed: true, id: elementId, length: text.length } };
  }

  function scrollPage(direction: ScrollDirection, amount: number, documentToken: string): ContentResult {
    const binding = checkBinding(documentToken, undefined, false);
    if (binding) return binding;
    const px = clampScroll(amount);
    window.scrollBy({ top: direction === 'down' ? px : -px, left: 0, behavior: 'auto' });
    return {
      ok: true,
      data: { x: Math.round(window.scrollX), y: Math.round(window.scrollY), applied: px, direction },
    };
  }

  function initSession(sessionId: string): ContentResult {
    state.sessionId = sessionId;
    state.snapshotId = null;
    state.registry = freshRegistry();
    state.initializedAt = Date.now();
    return { ok: true, data: { documentToken: state.documentToken } };
  }

  function probe(): ContentResult {
    return {
      ok: true,
      data: {
        sessionId: state.sessionId,
        documentToken: state.documentToken,
        url: location.href,
        initialized: state.sessionId !== null,
        snapshotId: state.snapshotId,
      },
    };
  }

  // --- Message handling (extension runtime only) ---

  function handle(message: ContentRequest): ContentResult {
    if (message.type === 'SEOUL_INIT') return initSession(message.sessionId);
    if (message.type === 'SEOUL_PROBE') return probe();

    // All other operations require the content script to be initialized for the
    // exact session the background is addressing.
    if (state.sessionId === null) {
      return { ok: false, code: 'ACTION_FAILED', message: 'Content script is not initialized.' };
    }
    if (message.sessionId !== state.sessionId) {
      return { ok: false, code: 'ACTION_FAILED', message: 'Session id does not match this content script.' };
    }

    switch (message.type) {
      case 'SEOUL_OBSERVE':
        return { ok: true, data: observe() };
      case 'SEOUL_CLICK':
        return clickElement(message.elementId, message.documentToken, message.snapshotId);
      case 'SEOUL_TYPE':
        return typeText(message.elementId, message.text, message.documentToken, message.snapshotId);
      case 'SEOUL_SCROLL':
        return scrollPage(message.direction, message.amount, message.documentToken);
      default:
        return { ok: false, code: 'INVALID_MESSAGE', message: 'Unsupported content request.' };
    }
  }

  function onMessage(
    message: unknown,
    sender: chrome.runtime.MessageSender,
    sendResponse: (response: ContentResult) => void,
  ): boolean {
    // Accept only messages from this extension's own runtime (the background
    // worker). Reject anything originating from a tab/content context, and never
    // respond to page postMessage (which does not arrive here anyway).
    if (!sender || sender.id !== chrome.runtime.id || sender.tab) {
      sendResponse({ ok: false, code: 'INVALID_MESSAGE', message: 'Untrusted sender.' });
      return true;
    }
    if (
      !message ||
      typeof message !== 'object' ||
      typeof (message as { type?: unknown }).type !== 'string'
    ) {
      sendResponse({ ok: false, code: 'INVALID_MESSAGE', message: 'Malformed content request.' });
      return true;
    }
    try {
      sendResponse(handle(message as ContentRequest));
    } catch (e) {
      sendResponse({ ok: false, code: 'ACTION_FAILED', message: e instanceof Error ? e.message : String(e) });
    }
    // Broadly compatible pattern: respond (synchronously here) and return true.
    return true;
  }

  if (!state.listenerInstalled) {
    chrome.runtime.onMessage.addListener(onMessage);
    state.listenerInstalled = true;
  }
})();
