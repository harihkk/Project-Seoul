// Project Seoul Development Harness - side panel controller.
//
// Separates panel view state, page attachment, and runtime task. On (re)open the
// panel renders a neutral surface unless a genuinely active runtime task exists
// (none do in this harness). It never restores a prior selection, snapshot,
// typed draft, error, or the previous operation log merely because an internal
// page-control attachment exists. The attachment is kept internally so the user
// can continue on the same granted tab without starting another session.
//
// Page-derived text is rendered only with textContent / safe DOM creation
// (never innerHTML).

import { validateNavigationUrl } from '../validation.ts';
import type {
  ProtocolRequest,
  ProtocolResponse,
  TaskState,
  InteractiveElement,
  PageSnapshot,
  TimelineEvent,
  StructuredError,
  PanelContextResult,
} from '../protocol.ts';

function byId<T extends HTMLElement>(id: string): T {
  const el = document.getElementById(id);
  if (!el) throw new Error(`Missing element #${id}`);
  return el as T;
}

const ui = {
  tabTitle: byId<HTMLParagraphElement>('tab-title'),
  tabOrigin: byId<HTMLParagraphElement>('tab-origin'),
  stateIndicator: byId<HTMLSpanElement>('state-indicator'),
  btnStart: byId<HTMLButtonElement>('btn-start'),
  btnStop: byId<HTMLButtonElement>('btn-stop'),
  btnInspect: byId<HTMLButtonElement>('btn-inspect'),
  btnClick: byId<HTMLButtonElement>('btn-click'),
  btnScrollUp: byId<HTMLButtonElement>('btn-scroll-up'),
  btnScrollDown: byId<HTMLButtonElement>('btn-scroll-down'),
  btnType: byId<HTMLButtonElement>('btn-type'),
  btnNavigate: byId<HTMLButtonElement>('btn-navigate'),
  typeInput: byId<HTMLInputElement>('type-input'),
  navInput: byId<HTMLInputElement>('nav-input'),
  staleWarning: byId<HTMLParagraphElement>('stale-warning'),
  accessNotice: byId<HTMLParagraphElement>('access-notice'),
  errorBox: byId<HTMLDivElement>('error-box'),
  errorCode: byId<HTMLSpanElement>('error-code'),
  errorMessage: byId<HTMLSpanElement>('error-message'),
  selectedValue: byId<HTMLSpanElement>('selected-value'),
  elementsList: byId<HTMLUListElement>('elements-list'),
  elementsCount: byId<HTMLSpanElement>('elements-count'),
  textList: byId<HTMLUListElement>('text-list'),
  textCount: byId<HTMLSpanElement>('text-count'),
  timelineList: byId<HTMLOListElement>('timeline-list'),
};

interface SelectedElement {
  id: string;
  role: string;
  name?: string;
}

// Panel view state is separate from the page attachment and from any runtime
// task. `attached` is the internal page-control binding for the active tab.
const session = {
  tabId: null as number | null,
  sessionId: null as string | null,
  attached: false,
  selected: null as SelectedElement | null,
};

let initToken = 0;
let accessGranted = false;

// Binding for the current observation; obtained only from OBSERVE_PAGE and
// cleared on every (re)open, tab change, navigation, or detach.
const snap = {
  documentToken: null as string | null,
  snapshotId: null as string | null,
};

function newId(): string {
  return crypto.randomUUID();
}

async function send<T = unknown>(req: ProtocolRequest): Promise<ProtocolResponse<T>> {
  return (await chrome.runtime.sendMessage(req)) as ProtocolResponse<T>;
}

function showError(err: StructuredError): void {
  // Never render the error bar without text (avoids a blank red strip).
  if (!err || !err.message) {
    clearError();
    return;
  }
  ui.errorBox.hidden = false;
  ui.errorCode.textContent = err.code;
  ui.errorMessage.textContent = err.message;
}

function clearError(): void {
  ui.errorBox.hidden = true;
  ui.errorCode.textContent = '';
  ui.errorMessage.textContent = '';
}

function showAccessNotice(text: string): void {
  ui.accessNotice.hidden = false;
  ui.accessNotice.textContent = text;
}

function hideAccessNotice(): void {
  ui.accessNotice.hidden = true;
  ui.accessNotice.textContent = '';
}

function setIndicator(label: string, cls: string): void {
  ui.stateIndicator.textContent = label;
  ui.stateIndicator.className = `state ${cls}`;
  updateControls();
}

// Neutral panel indicator: reflects access and attachment, never a task.
function setNeutral(): void {
  if (accessGranted && session.attached) {
    setIndicator('ATTACHED', 'state--ready');
  } else {
    setIndicator('IDLE', 'state--idle');
  }
}

function updateControls(): void {
  const granted = accessGranted;
  const attached = session.attached;
  const hasSelection = session.selected !== null;
  const hasDoc = snap.documentToken !== null;
  const hasSnapshot = hasDoc && snap.snapshotId !== null;

  // Start (attach) only when granted, a tab exists, and not already attached.
  ui.btnStart.disabled = !granted || session.tabId == null || attached;
  ui.btnStop.disabled = !attached;
  ui.btnInspect.disabled = !attached;
  ui.btnScrollUp.disabled = !attached || !hasDoc;
  ui.btnScrollDown.disabled = !attached || !hasDoc;
  ui.btnNavigate.disabled = !attached;
  ui.btnClick.disabled = !attached || !hasSelection || !hasSnapshot;
  ui.btnType.disabled = !attached || !hasSelection || !hasSnapshot;
}

function setSelected(sel: SelectedElement | null): void {
  session.selected = sel;
  if (sel) {
    const name = sel.name ? ` - ${sel.name}` : '';
    ui.selectedValue.textContent = `${sel.role} [${sel.id}]${name}`;
  } else {
    ui.selectedValue.textContent = 'none';
  }
  updateControls();
}

function renderElements(elements: InteractiveElement[]): void {
  ui.elementsList.replaceChildren();
  ui.elementsCount.textContent = `(${elements.length})`;
  for (const el of elements) {
    const li = document.createElement('li');
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'element-item';
    button.setAttribute('aria-pressed', 'false');

    const role = document.createElement('span');
    role.className = 'element-item__role';
    role.textContent = el.role;

    const name = document.createElement('span');
    name.className = 'element-item__name';
    name.textContent = el.name ?? '(no accessible name)';

    const meta = document.createElement('span');
    meta.className = 'element-item__meta';
    const bits = [el.tag];
    if (el.inputType) bits.push(`type=${el.inputType}`);
    if (el.disabled) bits.push('disabled');
    if (el.checked !== undefined) bits.push(`checked=${el.checked}`);
    meta.textContent = `${bits.join(' / ')} / ${el.id}`;

    button.append(role, name, meta);
    button.addEventListener('click', () => {
      for (const other of ui.elementsList.querySelectorAll('button[aria-pressed="true"]')) {
        other.setAttribute('aria-pressed', 'false');
      }
      button.setAttribute('aria-pressed', 'true');
      setSelected({ id: el.id, role: el.role, name: el.name });
    });

    li.append(button);
    ui.elementsList.append(li);
  }
}

function renderText(blocks: string[]): void {
  ui.textList.replaceChildren();
  ui.textCount.textContent = `(${blocks.length})`;
  for (const block of blocks) {
    const li = document.createElement('li');
    li.textContent = block; // textContent only - never innerHTML.
    ui.textList.append(li);
  }
}

function renderTimeline(events: TimelineEvent[]): void {
  ui.timelineList.replaceChildren();
  for (const ev of [...events].reverse()) {
    const li = document.createElement('li');
    const type = document.createElement('span');
    type.className = 'timeline__type';
    type.textContent = ev.type;
    const detail = document.createElement('span');
    detail.className = 'timeline__detail';
    detail.textContent = ev.detail ?? (ev.state ?? '');
    li.append(type, detail);
    ui.timelineList.append(li);
  }
}

// Refreshes the visible operation log of the current attachment after an action.
// It only renders the timeline; it never changes the panel indicator or resurrects
// a prior error.
async function refreshTaskState(): Promise<void> {
  if (!session.sessionId) return;
  const resp = await send<{ state: TaskState; timeline: TimelineEvent[] }>({
    kind: 'GET_TASK_STATE',
    id: newId(),
    sessionId: session.sessionId,
  });
  if (resp.success) {
    renderTimeline(resp.result.timeline);
  }
}

function clearSnapshot(): void {
  snap.documentToken = null;
  snap.snapshotId = null;
  ui.typeInput.value = ''; // clear the page-action text draft
  ui.elementsList.replaceChildren();
  ui.elementsCount.textContent = '';
  ui.textList.replaceChildren();
  ui.textCount.textContent = '';
  setSelected(null);
}

function showRestoring(): void {
  ui.stateIndicator.textContent = 'RESTORING';
  ui.stateIndicator.className = 'state state--starting';
  for (const b of [
    ui.btnStart,
    ui.btnStop,
    ui.btnInspect,
    ui.btnClick,
    ui.btnType,
    ui.btnScrollUp,
    ui.btnScrollDown,
    ui.btnNavigate,
  ]) {
    b.disabled = true;
  }
}

// Detach the panel from the current attachment and return to a neutral surface.
function detach(): void {
  session.sessionId = null;
  session.attached = false;
  clearSnapshot();
  ui.staleWarning.hidden = true;
  setNeutral();
}

// Surface an operation error. If the error means the attachment is gone, drop it
// and return to neutral so no stale interaction UI lingers.
function handleOpError(err: StructuredError): void {
  showError(err);
  if (
    err.code === 'STALE_SESSION' ||
    err.code === 'TAB_MISMATCH' ||
    err.code === 'SESSION_NOT_FOUND' ||
    err.code === 'ACCESS_REQUIRED' ||
    err.code === 'ACTION_OUTCOME_UNKNOWN'
  ) {
    detach();
  }
}

function applyPanelContext(result: PanelContextResult): void {
  session.tabId = result.tabId;
  accessGranted = result.access === 'GRANTED';

  // Page context: real values only when granted; never fake placeholders.
  if (result.tabId == null) {
    ui.tabTitle.textContent = 'No active tab';
    ui.tabOrigin.textContent = '(none)';
  } else if (result.access === 'GRANTED') {
    ui.tabTitle.textContent = result.title || '(untitled page)';
    ui.tabOrigin.textContent = result.origin || '(unknown origin)';
  } else {
    ui.tabTitle.textContent = '(no access)';
    ui.tabOrigin.textContent =
      result.access === 'UNSUPPORTED_PAGE' ? '(unsupported page)' : '(access required)';
  }

  // Access notice (a neutral notice, not the red error bar).
  if (result.access === 'ACCESS_REQUIRED') {
    showAccessNotice(
      result.reason ??
        'Access required. Click the Project Seoul toolbar icon on the page you want Seoul to access.',
    );
  } else if (result.access === 'UNSUPPORTED_PAGE') {
    showAccessNotice(`UNSUPPORTED_PAGE: ${result.reason ?? 'This page scheme is not supported.'}`);
  } else if (result.access === 'NO_TAB') {
    showAccessNotice('No active tab. Open a page, then click the Project Seoul toolbar icon.');
  } else {
    hideAccessNotice();
  }

  // Ephemeral panel state is always cleared on (re)open. A prior error,
  // selection, snapshot, typed draft, and the previous operation log never
  // carry over.
  clearError();
  ui.staleWarning.hidden = true;
  clearSnapshot();
  ui.timelineList.replaceChildren();

  // Keep a valid attachment internally so the user can inspect the same granted
  // tab without starting another control session.
  session.attached = result.attachment.attached;
  session.sessionId = result.attachment.attached ? result.attachment.sessionId : null;
  setNeutral();
}

// On (re)open and on active-tab change, ask the background for access and
// attachment state and render a fresh surface. The panel never reads
// chrome.storage directly and never supplies a tab id.
async function init(): Promise<void> {
  const token = ++initToken;
  clearError();
  hideAccessNotice();
  clearSnapshot();
  showRestoring();
  const resp = await send<PanelContextResult>({
    kind: 'GET_PANEL_CONTEXT',
    id: newId(),
    sessionId: newId(),
  });
  if (token !== initToken) return; // superseded by a newer init (for example a tab change)
  if (resp.success) {
    applyPanelContext(resp.result);
  } else {
    session.tabId = null;
    accessGranted = false;
    detach();
    showError(resp.error);
  }
}

// --- Actions ---

async function startSession(): Promise<void> {
  clearError();
  ui.staleWarning.hidden = true;
  if (!accessGranted || session.tabId == null) {
    await init();
    return;
  }
  clearSnapshot();
  const sessionId = newId();
  setIndicator('STARTING', 'state--starting');
  const resp = await send<{ state: TaskState }>({
    kind: 'SESSION_START',
    id: newId(),
    sessionId,
    tabId: session.tabId,
  });
  if (resp.success) {
    session.sessionId = sessionId;
    session.attached = true;
    setNeutral();
    await refreshTaskState();
  } else if (
    resp.error.code === 'ACCESS_REQUIRED' ||
    resp.error.code === 'UNSUPPORTED_PAGE' ||
    resp.error.code === 'TAB_MISMATCH'
  ) {
    // Not a failed task: re-resolve and render the real access state.
    detach();
    await init();
  } else {
    // A genuine attach failure (for example injection). Show the operation error.
    detach();
    showError(resp.error);
  }
}

async function stopSession(): Promise<void> {
  if (!session.sessionId) return;
  clearError();
  const resp = await send({ kind: 'SESSION_STOP', id: newId(), sessionId: session.sessionId });
  if (!resp.success) showError(resp.error);
  detach();
}

async function inspectPage(): Promise<void> {
  if (!session.sessionId || !session.attached) return;
  clearError();
  const resp = await send<{ snapshot: PageSnapshot }>({
    kind: 'OBSERVE_PAGE',
    id: newId(),
    sessionId: session.sessionId,
  });
  if (resp.success) {
    snap.documentToken = resp.result.snapshot.documentToken;
    snap.snapshotId = resp.result.snapshot.snapshotId;
    setSelected(null);
    renderElements(resp.result.snapshot.elements);
    renderText(resp.result.snapshot.textBlocks);
    updateControls();
  } else {
    handleOpError(resp.error);
  }
  await refreshTaskState();
}

async function clickSelected(): Promise<void> {
  if (!session.sessionId || !session.selected || !snap.documentToken || !snap.snapshotId) return;
  clearError();
  const resp = await send({
    kind: 'EXECUTE_ACTION',
    id: newId(),
    sessionId: session.sessionId,
    action: {
      kind: 'CLICK_ELEMENT',
      elementId: session.selected.id,
      documentToken: snap.documentToken,
      snapshotId: snap.snapshotId,
    },
  });
  if (!resp.success) handleOpError(resp.error);
  await refreshTaskState();
}

async function typeIntoSelected(): Promise<void> {
  if (!session.sessionId || !session.selected || !snap.documentToken || !snap.snapshotId) return;
  clearError();
  const resp = await send({
    kind: 'EXECUTE_ACTION',
    id: newId(),
    sessionId: session.sessionId,
    action: {
      kind: 'TYPE_TEXT',
      elementId: session.selected.id,
      text: ui.typeInput.value,
      documentToken: snap.documentToken,
      snapshotId: snap.snapshotId,
    },
  });
  if (!resp.success) handleOpError(resp.error);
  await refreshTaskState();
}

async function scroll(direction: 'up' | 'down'): Promise<void> {
  if (!session.sessionId || !snap.documentToken) return;
  clearError();
  const resp = await send({
    kind: 'EXECUTE_ACTION',
    id: newId(),
    sessionId: session.sessionId,
    action: { kind: 'SCROLL_PAGE', direction, amount: 600, documentToken: snap.documentToken },
  });
  if (!resp.success) handleOpError(resp.error);
  await refreshTaskState();
}

async function navigate(): Promise<void> {
  if (!session.sessionId || !session.attached) return;
  clearError();
  const check = validateNavigationUrl(ui.navInput.value);
  if (!check.ok) {
    showError(check.error);
    return;
  }
  const resp = await send({
    kind: 'EXECUTE_ACTION',
    id: newId(),
    sessionId: session.sessionId,
    action: { kind: 'NAVIGATE', url: check.url },
  });
  if (resp.success) {
    detach();
    ui.staleWarning.hidden = false;
    ui.staleWarning.textContent =
      'Navigated to a new page. Click the Project Seoul toolbar icon to attach again.';
  } else {
    handleOpError(resp.error);
  }
}

function wire(): void {
  ui.btnStart.addEventListener('click', () => void startSession());
  ui.btnStop.addEventListener('click', () => void stopSession());
  ui.btnInspect.addEventListener('click', () => void inspectPage());
  ui.btnClick.addEventListener('click', () => void clickSelected());
  ui.btnType.addEventListener('click', () => void typeIntoSelected());
  ui.btnScrollUp.addEventListener('click', () => void scroll('up'));
  ui.btnScrollDown.addEventListener('click', () => void scroll('down'));
  ui.btnNavigate.addEventListener('click', () => void navigate());
}

// Re-resolve when the active tab changes while the panel stays open: this clears
// the previous tab's selection and snapshot and shows the new tab's neutral state.
chrome.tabs.onActivated.addListener(() => {
  void init();
});

wire();
showRestoring();
void init();
