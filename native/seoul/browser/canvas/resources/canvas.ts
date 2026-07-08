// Project Seoul Canvas - first-party WebUI renderer.
//
// Receives validated SAUI surface documents from the browser over Mojo and
// builds the interface from the trusted, domain-neutral component catalog using
// only safe DOM APIs (createElement + textContent + known-safe attributes).
// It NEVER uses innerHTML, eval, or Function, and it never trusts the payload
// as code - the browser already parsed and validated the document; the renderer
// only draws it. Interactions are reported back as typed component events with
// stable ids; no DOM state or coordinates cross the boundary.
//
// Mojo bindings (PageHandler/Page) are generated at build time from
// canvas.mojom; this module imports them from the generated path.

import {
  PageHandlerFactory,
  PageHandler,
  PageCallbackRouter,
  ComponentEventKind,
} from './canvas.mojom-webui.js';

// --- Trusted, structural rendering of the SAUI component catalog ---

interface SurfaceDoc {
  id: string;
  kind: string;
  title?: string;
  components: ComponentNode[];
  data?: Record<string, DataEntry>;
  actions?: SurfaceAction[];
}
interface ComponentNode {
  id: string;
  type: string;
  props?: Record<string, unknown>;
  bindings?: Record<string, string>;
  accessible_name?: string;
  state?: string;
  action_ids?: string[];
  children?: ComponentNode[];
}
interface DataEntry {
  kind: 'scalar' | 'record' | 'series' | 'table';
  value?: unknown;
  fields?: Record<string, unknown>;
  columns?: { key: string; label: string }[];
  rows?: unknown[][];
  points?: { t_ms?: number; x?: number; y: number }[];
}
interface SurfaceAction {
  id: string;
  label: string;
  kind: string;
  target: string;
}
interface RealtimeVoiceSessionDoc {
  status?: string;
  detail?: string;
  api_model?: string;
  product_target?: string;
  connect_url?: string;
  client_secret?: string;
  instructions?: string;
  tools?: unknown[];
}
interface RealtimeConnection {
  peer: RTCPeerConnection;
  dataChannel: RTCDataChannel;
  stream: MediaStream;
  audio: HTMLAudioElement;
}
interface RealtimeFunctionCall {
  key: string;
  callId: string;
  name: string;
  argumentsJson: string;
}

const handler = PageHandlerFactory.getRemote
  ? PageHandlerFactory.getRemote()
  : undefined;
let pageHandler: PageHandler | undefined;
const callbackRouter = new PageCallbackRouter();

const surfaceEl = document.getElementById('surface') as HTMLElement;
const idleEl = document.getElementById('idle') as HTMLElement;
const routeEl = document.getElementById('route-indicator') as HTMLElement;
let voiceToggleEl: HTMLButtonElement | undefined;
let realtimeConnection: RealtimeConnection | undefined;
let realtimeStarting = false;
let realtimeStartGeneration = 0;
const realtimeToolCalls: Map<string, { callId: string; name: string }> = new Map();
const completedRealtimeToolCalls: Set<string> = new Set();

// Current surface actions, so a component event can resolve its declared
// action id (the renderer reports the id; the browser authorizes it).
let currentActions: Map<string, SurfaceAction> = new Map();

function el(tag: string, className?: string, text?: string): HTMLElement {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}

function propString(props: Record<string, unknown> | undefined, key: string): string {
  const v = props?.[key];
  return typeof v === 'string' ? v : '';
}

function renderTableEntry(entry: DataEntry): HTMLElement {
  const table = el('table', 'saui-table');
  const thead = el('thead');
  const headRow = el('tr');
  for (const col of entry.columns ?? []) {
    headRow.appendChild(el('th', undefined, col.label));
  }
  thead.appendChild(headRow);
  table.appendChild(thead);
  const tbody = el('tbody');
  for (const row of entry.rows ?? []) {
    const tr = el('tr');
    for (const cell of row) {
      tr.appendChild(el('td', undefined, cell == null ? '' : String(cell)));
    }
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  return table;
}

// Charts render as their accessible data-table alternative in this first-party
// renderer: honest, keyboard-navigable, and never a fabricated visual. A
// packaged chart library can enhance this later without changing the protocol.
function renderChartFallback(node: ComponentNode, entry: DataEntry | undefined): HTMLElement {
  const wrap = el('figure', 'saui-chart');
  const caption = el('figcaption', undefined, propString(node.props, 'title') || 'Chart');
  wrap.appendChild(caption);
  if (entry && entry.kind === 'table') {
    wrap.appendChild(renderTableEntry(entry));
  } else if (entry && entry.kind === 'series') {
    const table = el('table', 'saui-table');
    const head = el('tr');
    head.appendChild(el('th', undefined, propString(node.props, 'x_label') || 'x'));
    head.appendChild(el('th', undefined, propString(node.props, 'y_label') || 'y'));
    table.appendChild(head);
    for (const p of entry.points ?? []) {
      const tr = el('tr');
      tr.appendChild(el('td', undefined, p.t_ms != null ? new Date(p.t_ms).toISOString() : String(p.x)));
      tr.appendChild(el('td', undefined, String(p.y)));
      table.appendChild(tr);
    }
    wrap.appendChild(table);
  }
  return wrap;
}

function renderRecord(entry: DataEntry | undefined): HTMLElement {
  const dl = el('dl', 'saui-record');
  for (const [key, value] of Object.entries(entry?.fields ?? {})) {
    dl.appendChild(el('dt', undefined, key));
    dl.appendChild(el('dd', undefined, value == null ? '' : String(value)));
  }
  return dl;
}

function boundEntry(surface: SurfaceDoc, node: ComponentNode): DataEntry | undefined {
  const name = node.bindings?.['data'];
  return name ? surface.data?.[name] : undefined;
}

function renderComponent(surface: SurfaceDoc, node: ComponentNode): HTMLElement {
  const entry = boundEntry(surface, node);
  switch (node.type) {
    case 'text':
    case 'rich_text':
      return el('p', 'saui-text', propString(node.props, 'text'));
    case 'heading':
      return el('h2', 'saui-heading', propString(node.props, 'text'));
    case 'badge':
      return el('span', 'saui-badge', propString(node.props, 'text'));
    case 'empty_state':
      return el('div', 'saui-empty', propString(node.props, 'text') || 'No results.');
    case 'error_state':
      return el('div', 'saui-error', propString(node.props, 'text') || 'Something went wrong.');
    case 'link': {
      const a = el('a', 'saui-link', propString(node.props, 'text')) as HTMLAnchorElement;
      const href = propString(node.props, 'href');
      if (href) a.setAttribute('href', href);
      a.setAttribute('rel', 'noreferrer noopener');
      return a;
    }
    case 'metric': {
      const wrap = el('div', 'saui-metric');
      wrap.appendChild(el('div', 'saui-metric-label', propString(node.props, 'label')));
      const value = entry && entry.kind === 'scalar' ? String(entry.value) : '';
      wrap.appendChild(el('div', 'saui-metric-value', value));
      return wrap;
    }
    case 'entity_card':
    case 'key_value_card':
    case 'document':
    case 'file':
      return renderRecord(entry);
    case 'table':
    case 'sortable_table':
    case 'comparison_matrix':
    case 'list':
    case 'timeline':
    case 'activity_log':
    case 'tree':
      return entry && entry.kind === 'table' ? renderTableEntry(entry) : el('div', 'saui-empty', 'No data.');
    case 'line_chart':
    case 'area_chart':
    case 'bar_chart':
    case 'stacked_bar_chart':
    case 'scatter_chart':
    case 'pie_chart':
    case 'candlestick_chart':
    case 'range_chart':
    case 'sparkline':
    case 'histogram':
    case 'heat_map':
    case 'network_graph':
    case 'map':
    case 'geo_layer':
      return renderChartFallback(node, entry);
    case 'button': {
      const button = el('button', 'saui-button', propString(node.props, 'label')) as HTMLButtonElement;
      button.type = 'button';
      const actionId = node.action_ids?.[0];
      button.addEventListener('click', () => {
        emitComponentEvent(surface.id, node.id, ComponentEventKind.kActivate, actionId, null);
      });
      return button;
    }
    case 'source_list': {
      const ul = el('ul', 'saui-sources');
      const sources = (node.props?.['sources'] as { href?: string; title?: string }[]) ?? [];
      for (const s of sources) {
        const li = el('li');
        const a = el('a', 'saui-link', s.title || s.href || '') as HTMLAnchorElement;
        if (s.href) a.setAttribute('href', s.href);
        a.setAttribute('rel', 'noreferrer noopener');
        li.appendChild(a);
        ul.appendChild(li);
      }
      return ul;
    }
    case 'stack':
    case 'row':
    case 'grid':
    case 'collapsible_section':
    case 'resizable_panel':
    case 'report_preview': {
      const container = el('div', `saui-${node.type}`);
      const title = propString(node.props, 'title');
      if (title) container.appendChild(el('h3', 'saui-section-title', title));
      for (const child of node.children ?? []) {
        container.appendChild(renderComponent(surface, child));
      }
      return container;
    }
    default: {
      // Unknown-but-validated type: render its bound data generically rather
      // than failing. The catalog is closed, so this is a forward-compat path.
      if (entry && entry.kind === 'table') return renderTableEntry(entry);
      if (entry && entry.kind === 'record') return renderRecord(entry);
      return el('div', 'saui-generic', propString(node.props, 'text'));
    }
  }
}

function renderSurface(surfaceJson: string): void {
  let surface: SurfaceDoc;
  try {
    surface = JSON.parse(surfaceJson) as SurfaceDoc;
  } catch {
    return; // the browser only sends validated JSON; a parse failure is inert
  }
  currentActions = new Map((surface.actions ?? []).map((a) => [a.id, a]));
  const root = el('div', 'saui-surface');
  if (surface.title) root.appendChild(el('h1', 'saui-title', surface.title));
  for (const node of surface.components) {
    root.appendChild(renderComponent(surface, node));
  }
  surfaceEl.replaceChildren(root);
  surfaceEl.hidden = false;
  idleEl.hidden = true;
}

function emitComponentEvent(
  surfaceId: string,
  componentId: string,
  kind: number,
  actionId: string | undefined,
  value: unknown,
): void {
  if (!pageHandler) return;
  pageHandler.notifyComponentEvent({
    surfaceId,
    componentId,
    kind,
    actionId: actionId ?? null,
    valueJson: JSON.stringify(value ?? null),
  });
}

function setVoiceActive(active: boolean, state: string): void {
  if (!voiceToggleEl) return;
  voiceToggleEl.setAttribute('aria-pressed', active ? 'true' : 'false');
  voiceToggleEl.dataset.state = state;
}

function setRouteText(text: string): void {
  routeEl.textContent = text || 'Voice';
}

function sendRealtimeEvent(event: Record<string, unknown>): void {
  const channel = realtimeConnection?.dataChannel;
  if (!channel || channel.readyState !== 'open') return;
  channel.send(JSON.stringify(event));
}

function isCurrentRealtimeStart(generation: number): boolean {
  return realtimeStarting && generation === realtimeStartGeneration;
}

function rememberRealtimeToolCall(event: Record<string, unknown>): void {
  const item = event.item as Record<string, unknown> | undefined;
  if (!item || item.type !== 'function_call') return;
  const key = String(item.id ?? event.item_id ?? '');
  const name = typeof item.name === 'string' ? item.name : '';
  const callId = typeof item.call_id === 'string' ? item.call_id : key;
  if (key && name) {
    realtimeToolCalls.set(key, { callId, name });
  }
}

function extractRealtimeToolCall(event: Record<string, unknown>): RealtimeFunctionCall | undefined {
  rememberRealtimeToolCall(event);
  const item = event.item as Record<string, unknown> | undefined;
  const itemId = String(event.item_id ?? item?.id ?? '');
  const remembered = itemId ? realtimeToolCalls.get(itemId) : undefined;
  const type = String(event.type ?? '');
  const name =
      typeof event.name === 'string' ? event.name :
      typeof item?.name === 'string' ? item.name :
      remembered?.name ?? '';
  const callId =
      typeof event.call_id === 'string' ? event.call_id :
      typeof item?.call_id === 'string' ? item.call_id :
      remembered?.callId ?? itemId;
  const rawArgs =
      typeof event.arguments === 'string' ? event.arguments :
      typeof event.arguments_json === 'string' ? event.arguments_json :
      typeof item?.arguments === 'string' ? item.arguments : '';
  const key = callId || itemId;
  if (!key || !name || !rawArgs || completedRealtimeToolCalls.has(key)) {
    return undefined;
  }
  if (
    type === 'response.function_call_arguments.done' ||
    type === 'response.output_item.done' ||
    type === 'conversation.item.created'
  ) {
    return { key, callId, name, argumentsJson: rawArgs };
  }
  return undefined;
}

async function sendRealtimeToolOutput(call: RealtimeFunctionCall, outputJson: string): Promise<void> {
  completedRealtimeToolCalls.add(call.key);
  sendRealtimeEvent({
    type: 'conversation.item.create',
    item: {
      type: 'function_call_output',
      call_id: call.callId,
      output: outputJson,
    },
  });
  sendRealtimeEvent({ type: 'response.create' });
}

async function handleRealtimeEvent(data: string): Promise<void> {
  let event: Record<string, unknown>;
  try {
    event = JSON.parse(data) as Record<string, unknown>;
  } catch {
    return;
  }
  const call = extractRealtimeToolCall(event);
  if (!call || !pageHandler) return;
  try {
    const result = await pageHandler.submitRealtimeToolCall({
      callId: call.callId,
      name: call.name,
      argumentsJson: call.argumentsJson,
    });
    await sendRealtimeToolOutput(call, result.outputJson);
  } catch {
    await sendRealtimeToolOutput(call, JSON.stringify({ status: 'error', detail: 'tool_bridge_failed' }));
  }
}

async function stopRealtimeVoice(): Promise<void> {
  const connection = realtimeConnection;
  realtimeStarting = false;
  realtimeStartGeneration += 1;
  realtimeConnection = undefined;
  realtimeToolCalls.clear();
  completedRealtimeToolCalls.clear();
  if (connection) {
    connection.dataChannel.close();
    connection.peer.close();
    for (const track of connection.stream.getTracks()) {
      track.stop();
    }
    connection.audio.remove();
  }
  setVoiceActive(false, 'idle');
  setRouteText('Voice');
}

async function startRealtimeVoice(): Promise<void> {
  if (!pageHandler || realtimeConnection || realtimeStarting) return;
  realtimeStarting = true;
  const generation = realtimeStartGeneration + 1;
  realtimeStartGeneration = generation;
  setVoiceActive(true, 'connecting');
  setRouteText('Connecting');
  try {
    const response = await pageHandler.createRealtimeVoiceSession();
    if (!isCurrentRealtimeStart(generation)) return;
    const session = JSON.parse(response.sessionJson || '{}') as RealtimeVoiceSessionDoc;
    if (session.status !== 'ready' || !session.client_secret ||
        !session.connect_url || !session.api_model) {
      throw new Error(session.detail || 'realtime_voice_unavailable');
    }

    const stream = await navigator.mediaDevices.getUserMedia({
      audio: {
        echoCancellation: true,
        noiseSuppression: true,
        autoGainControl: true,
      },
    });
    if (!isCurrentRealtimeStart(generation)) {
      for (const track of stream.getTracks()) {
        track.stop();
      }
      return;
    }
    const peer = new RTCPeerConnection();
    const audio = document.createElement('audio');
    audio.autoplay = true;
    audio.hidden = true;
    document.body.appendChild(audio);
    peer.ontrack = (event) => {
      audio.srcObject = event.streams[0] ?? null;
    };
    for (const track of stream.getAudioTracks()) {
      peer.addTrack(track, stream);
    }

    const dataChannel = peer.createDataChannel('oai-events');
    realtimeConnection = { peer, dataChannel, stream, audio };
    dataChannel.addEventListener('open', () => {
      sendRealtimeEvent({
        type: 'session.update',
        session: {
          instructions: session.instructions ?? '',
          tools: session.tools ?? [],
          tool_choice: 'auto',
        },
      });
      setVoiceActive(true, 'listening');
      setRouteText(session.product_target || session.api_model || 'Voice');
    });
    dataChannel.addEventListener('message', (event) => {
      void handleRealtimeEvent(String(event.data));
    });
    dataChannel.addEventListener('close', () => {
      void stopRealtimeVoice();
    });

    const offer = await peer.createOffer();
    await peer.setLocalDescription(offer);
    const sdpResponse = await fetch(
      `${session.connect_url}?model=${encodeURIComponent(session.api_model)}`,
      {
        method: 'POST',
        body: offer.sdp ?? '',
        headers: {
          Authorization: `Bearer ${session.client_secret}`,
          'Content-Type': 'application/sdp',
        },
      },
    );
    if (!sdpResponse.ok) {
      throw new Error(`realtime_sdp_${sdpResponse.status}`);
    }
    if (!isCurrentRealtimeStart(generation)) return;
    await peer.setRemoteDescription({
      type: 'answer',
      sdp: await sdpResponse.text(),
    });
    if (isCurrentRealtimeStart(generation)) {
      realtimeStarting = false;
    }
  } catch {
    if (generation === realtimeStartGeneration) {
      await stopRealtimeVoice();
      setRouteText('Voice unavailable');
    }
  }
}

// --- Mojo Page implementation (browser -> renderer) ---

callbackRouter.pushSurface.addListener((surfaceId: string, surfaceJson: string) => {
  renderSurface(surfaceJson);
});
callbackRouter.applySurfacePatch.addListener((_surfaceId: string, _patchJson: string) => {
  // The browser applies and validates patches; when a full re-push is simpler
  // it uses pushSurface. A future revision renders in-place patches by id.
});
interface TaskSnapshotDoc {
  id: string;
  goal: string;
  state: string;
  pending_approval_step?: string;
  pending_approval_prompt?: string;
  receipts?: { step_id: string; status: string; verification?: { verified: boolean } }[];
}

// Live tasks for this window, keyed by id; rendered as a compact ribbon with
// real controls (pause/resume/cancel/approve) wired to the typed handler.
const tasks: Map<string, TaskSnapshotDoc> = new Map();
const tasksEl = document.getElementById('tasks') as HTMLElement | null;

function renderTasks(): void {
  if (!tasksEl) return;
  tasksEl.replaceChildren();
  for (const task of tasks.values()) {
    if (task.state === 'completed' || task.state === 'cancelled') continue;
    const row = el('div', 'task-row');
    row.appendChild(el('span', `task-state task-${task.state}`, task.state.replace(/_/g, ' ')));
    row.appendChild(el('span', 'task-goal', task.goal));
    const controls = el('span', 'task-controls');
    const control = (label: string, onClick: () => void) => {
      const b = el('button', 'task-btn', label) as HTMLButtonElement;
      b.addEventListener('click', onClick);
      controls.appendChild(b);
    };
    if (task.state === 'executing') control('Pause', () => pageHandler?.pauseTask(task.id));
    if (task.state === 'paused') control('Resume', () => pageHandler?.resumeTask(task.id));
    if (task.state === 'awaiting_approval' && task.pending_approval_step) {
      const step = task.pending_approval_step;
      control('Approve', () => pageHandler?.approveStep(task.id, step, true));
      control('Reject', () => pageHandler?.approveStep(task.id, step, false));
    }
    if (task.state !== 'failed') control('Cancel', () => pageHandler?.cancelActiveTask(task.id));
    row.appendChild(controls);
    if (task.pending_approval_prompt) {
      row.appendChild(el('div', 'task-prompt', task.pending_approval_prompt));
    }
    tasksEl.appendChild(row);
  }
}

callbackRouter.pushTaskSnapshot.addListener((snapshotJson: string) => {
  try {
    const snapshot = JSON.parse(snapshotJson) as TaskSnapshotDoc;
    if (snapshot && typeof snapshot.id === 'string') {
      tasks.set(snapshot.id, snapshot);
      renderTasks();
    }
  } catch {
    /* a malformed snapshot renders nothing rather than something wrong */
  }
});

callbackRouter.setStatus.addListener((statusJson: string) => {
  try {
    const status = JSON.parse(statusJson) as {
      route?: string;
      voice_state?: string;
    };
    const route =
        status.route === 'cloud' ? 'Cloud' :
        status.route === 'local' ? 'On-device' : '';
    const voiceState = status.voice_state ?? '';
    const voiceActive = [
      'microphone_requesting',
      'listening',
      'partial_transcript',
      'finalizing_transcript',
    ].includes(voiceState);
    if (voiceToggleEl) {
      voiceToggleEl.setAttribute('aria-pressed', voiceActive ? 'true' : 'false');
      voiceToggleEl.dataset.state = voiceState;
    }
    routeEl.textContent = route;
  } catch {
    /* inert */
  }
});

function wireInputBar(): void {
  const input = document.getElementById('turn-input') as HTMLInputElement;
  const send = document.getElementById('turn-send') as HTMLButtonElement;
  const voice = document.getElementById('voice-toggle') as HTMLButtonElement;
  voiceToggleEl = voice;
  const submit = () => {
    const text = input.value.trim();
    if (text && pageHandler) {
      pageHandler.submitTurn({ text });
      input.value = '';
    }
  };
  send.addEventListener('click', submit);
  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') submit();
  });
  voice.addEventListener('click', () => {
    if (!pageHandler) return;
    if (voice.getAttribute('aria-pressed') === 'true') pageHandler.stopVoice();
    else pageHandler.startVoice();
  });
}

function main(): void {
  if (!handler) return;
  pageHandler = handler;
  handler.createPageHandler(callbackRouter.$.bindNewPipeAndPassRemote());
  wireInputBar();
  pageHandler.requestInitialState();
}

main();
