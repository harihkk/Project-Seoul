// Project Seoul Canvas - trusted Chromium Lit WebUI.
//
// The browser sends validated SAUI JSON over Mojo. This component performs a
// closed type-to-template mapping: Lit escapes payload values, no payload is
// treated as HTML/code, and interactions return stable ids through Mojo.

import {CrLitElement, html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {
  ComponentEventKind,
  PageCallbackRouter,
  PageHandlerFactory,
  PageHandlerRemote,
} from './canvas.mojom-webui.js';
import {getCss} from './canvas.css.js';
import {getHtml} from './canvas.html.js';
import type {
  ComponentNode,
  DataEntry,
  LibraryBoardDoc,
  LibrarySnapshotDoc,
  StudioProviderRouteDoc,
  StudioSnapshotDoc,
  SurfaceDoc,
  TaskSnapshotDoc,
} from './canvas_types.js';
import {propString, safeHexColor, safeHttpUrl} from './canvas_types.js';
import {renderDataTable, renderVisualization} from './canvas_visualizations.js';

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

const VISUAL_TYPES = new Set([
  'line_chart', 'area_chart', 'bar_chart', 'stacked_bar_chart',
  'scatter_chart', 'pie_chart', 'candlestick_chart', 'range_chart',
  'sparkline', 'histogram', 'heat_map', 'network_graph', 'map', 'geo_layer',
]);

const TABLE_TYPES = new Set([
  'table', 'sortable_table', 'comparison_matrix', 'list', 'timeline',
  'activity_log', 'tree', 'media', 'map_marker_list', 'task_graph',
  'diagnostic_list', 'blocker_list', 'plan_view', 'file_tree',
]);

const RECORD_TYPES = new Set([
  'entity_card', 'key_value_card', 'document', 'file', 'workflow_node',
  'workflow_edge', 'trigger_card', 'approval_request', 'execution_status',
  'result_card', 'action_receipt', 'cost_summary', 'provider_indicator',
  'current_step',
]);

export class SeoulCanvasAppElement extends CrLitElement {
  static get is() {
    return 'seoul-canvas-app';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      surface_: {type: Object},
      tasks_: {type: Array},
      inputValue_: {type: String},
      routeLabel_: {type: String},
      voiceState_: {type: String},
      voiceConfigured_: {type: Boolean},
      selectedView_: {type: String},
      library_: {type: Object},
      libraryError_: {type: String},
      libraryBusy_: {type: Boolean},
      boardName_: {type: String},
      pendingDeleteBoardId_: {type: String},
      libraryQuery_: {type: String},
      taskInputs_: {type: Object},
      studio_: {type: Object},
      studioError_: {type: String},
      studioBusy_: {type: Boolean},
    };
  }

  protected accessor surface_: SurfaceDoc|undefined;
  protected accessor tasks_: TaskSnapshotDoc[] = [];
  protected accessor inputValue_ = '';
  protected accessor routeLabel_ = 'Voice key missing';
  protected accessor voiceState_ = 'idle';
  protected accessor voiceConfigured_ = false;
  protected accessor selectedView_: 'canvas'|'library'|'boards'|'studio' = 'canvas';
  protected accessor library_: LibrarySnapshotDoc = {};
  protected accessor libraryError_ = '';
  protected accessor libraryBusy_ = false;
  protected accessor boardName_ = '';
  protected accessor pendingDeleteBoardId_ = '';
  protected accessor libraryQuery_ = '';
  protected accessor taskInputs_: Record<string, string> = {};
  protected accessor studio_: StudioSnapshotDoc = {};
  protected accessor studioError_ = '';
  protected accessor studioBusy_ = false;

  private pageHandler_: PageHandlerRemote|undefined;
  private callbackRouter_ = new PageCallbackRouter();
  private initialized_ = false;
  private currentActions_ = new Set<string>();
  private realtimeConnection_: RealtimeConnection|undefined;
  private realtimeStarting_ = false;
  private realtimeStartGeneration_ = 0;
  private realtimeToolCalls_ = new Map<string, {callId: string, name: string}>();
  private completedRealtimeToolCalls_ = new Set<string>();

  override render() {
    return getHtml.bind(this)();
  }

  override connectedCallback() {
    super.connectedCallback();
    if (this.initialized_) {
      return;
    }
    this.initialized_ = true;
    this.installPageCallbacks_();
    this.pageHandler_ = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.pageHandler_.$.bindNewPipeAndPassReceiver());
    this.pageHandler_.requestInitialState();
    // Only prefetch the library when it is the initial view; switching to the
    // library or boards view refreshes it lazily on demand.
    if (this.selectedView_ === 'library' || this.selectedView_ === 'boards') {
      void this.refreshLibrary_();
    }
  }

  override disconnectedCallback() {
    void this.stopRealtimeVoice_();
    super.disconnectedCallback();
  }

  protected boundEntry_(node: ComponentNode): DataEntry|undefined {
    const name = node.bindings?.['data'];
    return name ? this.surface_?.data?.[name] : undefined;
  }

  protected renderComponent_(node: ComponentNode): unknown {
    const entry = this.boundEntry_(node);
    const accessibleName = node.accessible_name || propString(node.props, 'title');
    if (VISUAL_TYPES.has(node.type)) {
      return entry ? renderVisualization(node, entry) :
          html`<div class="saui-empty">No visualization data.</div>`;
    }
    if (TABLE_TYPES.has(node.type)) {
      return entry && (entry.kind === 'table' || entry.kind === 'series') ?
          html`<section class="data-block" aria-label="${accessibleName || 'Data'}">
            ${propString(node.props, 'title') ? html`<h3>${propString(node.props, 'title')}</h3>` : nothing}
            <div class="table-scroll">${renderDataTable(entry)}</div>
          </section>` : html`<div class="saui-empty">No data.</div>`;
    }
    if (RECORD_TYPES.has(node.type)) {
      return this.renderRecord_(node, entry);
    }

    switch (node.type) {
      case 'text':
      case 'rich_text':
        return html`<p class="saui-text">${propString(node.props, 'text')}</p>`;
      case 'heading':
        return html`<h2 class="saui-heading">${propString(node.props, 'text')}</h2>`;
      case 'divider':
        return html`<hr>`;
      case 'badge':
      case 'execution_status':
        return html`<span class="saui-badge">${propString(node.props, 'text') || node.state || ''}</span>`;
      case 'progress': {
        const value = entry?.kind === 'scalar' ? Number(entry.value) : Number(node.props?.['value']);
        return html`<progress aria-label="${accessibleName || 'Progress'}" max="100"
            value="${Number.isFinite(value) ? value : 0}"></progress>`;
      }
      case 'spinner':
        return html`<span class="spinner" role="status" aria-label="${accessibleName || 'Loading'}"></span>`;
      case 'empty_state':
        return html`<div class="saui-empty">${propString(node.props, 'text') || 'No results.'}</div>`;
      case 'error_state':
        return html`<div class="saui-error" role="alert">${propString(node.props, 'text') || node.state_message || 'Something went wrong.'}</div>`;
      case 'link':
      case 'citation': {
        const href = safeHttpUrl(node.props?.['href']);
        return href ? html`<a class="saui-link" href="${href}" target="_blank"
            rel="noreferrer noopener">${propString(node.props, 'text') || href}</a>` :
            html`<span>${propString(node.props, 'text')}</span>`;
      }
      case 'image': {
        const href = safeHttpUrl(node.props?.['src'] ?? node.props?.['href']);
        return href ? html`<a class="media-card" href="${href}" target="_blank"
            rel="noreferrer noopener"><span>Open image</span><small>${accessibleName}</small></a>` :
            html`<div class="saui-empty">Image source unavailable.</div>`;
      }
      case 'source_list':
        return this.renderSources_(node);
      case 'metric': {
        const value = entry?.kind === 'scalar' ? entry.value : '';
        return html`<article class="saui-metric">
          <span class="saui-metric-label">${propString(node.props, 'label')}</span>
          <strong class="saui-metric-value">${value == null ? '' : String(value)}</strong>
          <span class="saui-metric-unit">${propString(node.props, 'unit')}</span>
        </article>`;
      }
      case 'button':
      case 'retry_control':
        return html`<button class="saui-button" type="button"
            @click="${() => this.emitComponentEvent_(node, ComponentEventKind.kActivate, null)}">
          ${propString(node.props, 'label') || propString(node.props, 'text') || 'Continue'}
        </button>`;
      case 'confirmation':
        return html`<article class="confirmation-card">
          <h3>${propString(node.props, 'title') || 'Confirm action'}</h3>
          <p>${propString(node.props, 'text')}</p>
          <div class="button-row">
            <button class="saui-button primary" type="button"
                @click="${() => this.emitComponentEvent_(node, ComponentEventKind.kSubmit, true)}">Confirm</button>
            <button class="saui-button" type="button"
                @click="${() => this.emitComponentEvent_(node, ComponentEventKind.kDismiss, false)}">Cancel</button>
          </div>
        </article>`;
      case 'checkbox':
        return html`<label class="field checkbox"><input type="checkbox"
            @change="${(event: Event) => this.emitComponentEvent_(node, ComponentEventKind.kValueChanged, (event.target as HTMLInputElement).checked)}">
          <span>${propString(node.props, 'label')}</span></label>`;
      case 'text_input':
      case 'search_field':
      case 'numeric_input':
      case 'date_input':
      case 'time_input':
      case 'slider':
        return this.renderInput_(node);
      case 'select':
      case 'radio_group':
      case 'segmented_control':
      case 'filter_chips':
        return this.renderOptions_(node);
      case 'code_block':
      case 'diff_view':
      case 'log_viewer':
      case 'stack_trace':
        return html`<pre class="code-block"><code>${propString(node.props, 'text')}</code></pre>`;
      case 'stack':
      case 'row':
      case 'grid':
      case 'tabs':
      case 'collapsible_section':
      case 'resizable_panel':
      case 'carousel':
      case 'detail_drawer':
      case 'report_preview':
      case 'schema_form':
        return html`<section class="saui-layout saui-${node.type}" aria-label="${accessibleName}">
          ${propString(node.props, 'title') ? html`<h3>${propString(node.props, 'title')}</h3>` : nothing}
          ${(node.children ?? []).map(child => this.renderComponent_(child))}
        </section>`;
      default:
        return html`<section class="generic-card" aria-label="${accessibleName}">
          ${entry?.kind === 'record' ? this.renderRecord_(node, entry) : propString(node.props, 'text')}
        </section>`;
    }
  }

  protected onInput_(event: Event) {
    this.inputValue_ = (event.target as HTMLInputElement).value;
  }

  protected onInputKeydown_(event: KeyboardEvent) {
    if (event.key === 'Enter' && !event.isComposing) {
      this.submitTurn_();
    }
  }

  protected submitTurn_() {
    const text = this.inputValue_.trim();
    if (!text || !this.pageHandler_) {
      return;
    }
    this.pageHandler_.submitTurn({text});
    this.inputValue_ = '';
  }

  protected toggleVoice_() {
    if (this.realtimeConnection_ || this.realtimeStarting_) {
      void this.stopRealtimeVoice_();
    } else {
      void this.startRealtimeVoice_();
    }
  }

  protected activeTasks_(): TaskSnapshotDoc[] {
    return this.tasks_.filter(task =>
      task.state !== 'completed' && task.state !== 'cancelled');
  }

  protected taskControl_(task: TaskSnapshotDoc, command: string) {
    if (!this.pageHandler_) return;
    if (command === 'pause') this.pageHandler_.pauseTask(task.id);
    if (command === 'resume') this.pageHandler_.resumeTask(task.id);
    if (command === 'cancel') this.pageHandler_.cancelActiveTask(task.id);
    if (command === 'approve' && task.pending_approval_step) {
      this.pageHandler_.approveStep(task.id, task.pending_approval_step, true);
    }
    if (command === 'reject' && task.pending_approval_step) {
      this.pageHandler_.approveStep(task.id, task.pending_approval_step, false);
    }
  }

  protected onTaskInput_(task: TaskSnapshotDoc, event: Event) {
    this.taskInputs_ = {
      ...this.taskInputs_,
      [task.id]: (event.target as HTMLInputElement).value,
    };
  }

  protected provideTaskInput_(task: TaskSnapshotDoc) {
    const value = (this.taskInputs_[task.id] ?? '').trim();
    if (!this.pageHandler_ || !task.pending_approval_step || !value) return;
    this.pageHandler_.provideTaskInput(
        task.id, task.pending_approval_step, value);
    this.taskInputs_ = {...this.taskInputs_, [task.id]: ''};
  }

  protected selectView_(view: 'canvas'|'library'|'boards'|'studio') {
    if (view === this.selectedView_) return;
    this.selectedView_ = view;
    void this.updateComplete.then(() => {
      const root = this.shadowRoot?.querySelector<HTMLElement>('#canvas-root');
      root?.scrollTo({
        top: 0,
        behavior: matchMedia('(prefers-reduced-motion: reduce)').matches ?
            'auto' : 'smooth',
      });
    });
    if (view === 'library' || view === 'boards') void this.refreshLibrary_();
    if (view === 'studio') void this.refreshStudio_();
  }

  protected renderStudio_(): unknown {
    const scenes = this.studio_.scenes ?? [];
    const themes = this.studio_.themes ?? [];
    const layers = this.studio_.site_layers ?? [];
    const local = this.studio_.providers?.local;
    const cloud = this.studio_.providers?.cloud;
    return html`<section class="studio-view" aria-label="Studio">
      <div class="view-heading"><div><span class="eyebrow">PROFILE RUNTIME</span>
        <h2>Studio</h2></div><span class="count-chip">Read-only index</span></div>
      <p class="studio-intro">See the reasoning routes and appearance systems this profile can use. Editing stays unavailable until each mutation has validation, recovery, and a compiled browser path.</p>
      ${this.studioError_ ? html`<div class="saui-error" role="alert">${this.studioError_}</div>` : nothing}
      ${this.studioBusy_ && !this.studio_.schema_version ? html`
        <div class="studio-loading" role="status"><span class="spinner" aria-hidden="true"></span>Loading profile systems…</div>` : nothing}
      <section class="studio-section" aria-labelledby="studio-routes-title">
        <div class="studio-section-heading"><div><span class="studio-index">01</span><h3 id="studio-routes-title">Reasoning routes</h3></div><p>Availability, without secrets or endpoints.</p></div>
        <div class="route-grid">
          ${this.renderProviderRoute_('On-device', local, local?.healthy ?? false)}
          ${this.renderProviderRoute_('Cloud', cloud, cloud?.available ?? false)}
        </div>
      </section>
      <section class="studio-section" aria-labelledby="studio-scenes-title">
        <div class="studio-section-heading"><div><span class="studio-index">02</span><h3 id="studio-scenes-title">Scenes</h3></div><span class="count-chip">${scenes.length}</span></div>
        ${scenes.length ? html`<div class="studio-list">${scenes.map(scene => html`
          <article class="studio-item"><div class="studio-item-mark" aria-hidden="true">${scene.name.slice(0, 1).toLocaleUpperCase()}</div>
            <div class="studio-item-body"><h4>${scene.name}</h4><p>Workspace ${scene.workspace_id}</p>
              <div class="studio-tags"><span>${scene.site_layer_count} site layer${scene.site_layer_count === 1 ? '' : 's'}</span>
                <span>${scene.theme_id ? 'Theme linked' : 'Global theme'}</span>
                ${scene.prefer_compact ? html`<span>Compact</span>` : nothing}</div></div>
          </article>`)}</div>` : html`<div class="empty-shelf"><h4>No Scenes configured</h4><p>Scenes will appear here from the profile registry; this view does not invent presets.</p></div>`}
      </section>
      <section class="studio-section" aria-labelledby="studio-themes-title">
        <div class="studio-section-heading"><div><span class="studio-index">03</span><h3 id="studio-themes-title">Themes</h3></div><span class="count-chip">${themes.length}</span></div>
        ${themes.length ? html`<div class="theme-grid">${themes.map(theme => html`
          <article class="theme-card"><div class="theme-preview" style="background:${safeHexColor(theme.background)}">
            <span class="theme-surface" style="background:${safeHexColor(theme.surface)};border-radius:${Math.max(0, Math.min(64, theme.corner_radius_px))}px">
              <i style="background:${safeHexColor(theme.accent)}"></i><i></i><i></i>
            </span></div>
            <div class="theme-meta"><div><h4>${theme.name}</h4><p>${theme.scheme} · ${theme.reduced_motion ? 'Reduced motion' : 'Motion on'}</p></div>
              <span class="theme-accent" style="background:${safeHexColor(theme.accent)}" aria-label="Accent ${safeHexColor(theme.accent)}"></span></div>
          </article>`)}</div>` : html`<div class="empty-shelf"><h4>No Themes configured</h4><p>Only accessibility-validated themes from the profile registry will appear here.</p></div>`}
      </section>
      <section class="studio-section" aria-labelledby="studio-layers-title">
        <div class="studio-section-heading"><div><span class="studio-index">04</span><h3 id="studio-layers-title">Site Layers</h3></div><span class="count-chip">${layers.length}</span></div>
        ${layers.length ? html`<div class="layer-grid">${layers.map(layer => html`
          <article class="layer-card"><header><span class="layer-state ${layer.enabled ? 'enabled' : ''}">${layer.enabled ? 'Enabled' : 'Paused'}</span><span>${layer.adjustment_count} adjustment${layer.adjustment_count === 1 ? '' : 's'}</span></header>
            <h4>${layer.name}</h4><p>${layer.origin_pattern}</p>
            <small>${layer.scene_scope ? `Scene · ${layer.scene_scope}` : 'All matching Scenes'}</small>
          </article>`)}</div>` : html`<div class="empty-shelf"><h4>No Site Layers configured</h4><p>Validated visual adjustments will appear here from the Site Layer registry.</p></div>`}
      </section>
    </section>`;
  }

  private renderProviderRoute_(
      label: string, route: StudioProviderRouteDoc|undefined,
      ready: boolean): unknown {
    const state = ready ? 'Ready' : route?.configured ? 'Needs attention' : 'Not configured';
    return html`<article class="provider-route ${ready ? 'ready' : ''}">
      <div class="provider-orbit" aria-hidden="true"><span></span></div>
      <div><span class="provider-label">${label}</span><h4>${state}</h4>
        <p>${route?.model_configured ? 'Model selected' : 'No model selected'}${route?.discovered_model_count ? ` · ${route.discovered_model_count} discovered` : ''}</p></div>
    </article>`;
  }

  protected renderLibrary_(): unknown {
    const query = this.libraryQuery_.trim().toLocaleLowerCase();
    const artifacts = (this.library_.artifacts ?? []).filter(artifact =>
      !query || [artifact.title, artifact.origin, artifact.kind, artifact.mime_type]
        .some(value => value.toLocaleLowerCase().includes(query)));
    const collections = (this.library_.live_collections ?? []).filter(collection =>
      !query || [collection.name, collection.refresh_capability,
        ...(collection.items ?? []).flatMap(item => [item.title, item.subtitle ?? '', item.status ?? ''])]
        .some(value => value.toLocaleLowerCase().includes(query)));
    return html`<section class="library-view" aria-label="Library">
      <div class="view-heading"><div><span class="eyebrow">DURABLE, USER-OWNED</span>
        <h2>Library</h2></div><span class="count-chip">${artifacts.length} artifacts</span></div>
      <label class="library-search"><span class="sr-only">Search Library</span>
        <input type="search" placeholder="Search saved things and live collections"
            .value="${this.libraryQuery_}" @input="${this.onLibraryQueryInput_}">
      </label>
      ${this.libraryError_ ? html`<div class="saui-error" role="alert">${this.libraryError_}</div>` : nothing}
      <section class="library-section"><h3>Saved artifacts</h3>
        ${artifacts.length ? html`<div class="artifact-grid">${artifacts.map(artifact => html`
          <article class="artifact-card"><span class="artifact-kind">${artifact.kind.replace(/_/g, ' ')}</span>
            <h4>${artifact.title || 'Untitled artifact'}</h4>
            <p>${artifact.origin || artifact.mime_type || 'Local reference'}</p>
            ${artifact.pinned ? html`<span class="saui-badge">Pinned</span>` : nothing}
          </article>`)}</div>` : html`<div class="empty-shelf"><h4>No saved artifacts yet</h4>
            <p>Captures, images, media, and download references appear here after you explicitly save them.</p></div>`}
      </section>
      <section class="library-section"><h3>Live collections</h3>
        ${collections.length ? html`<div class="collection-list">${collections.map(collection => html`
          <article class="collection-card"><header><div><h4>${collection.name}</h4>
            <p>${collection.refresh_capability}</p></div><span class="task-state task-${collection.refresh_state}">${collection.refresh_state}</span></header>
            ${collection.last_error ? html`<div class="saui-error">${collection.last_error}</div>` : nothing}
            <ul>${(collection.items ?? []).slice(0, 6).map(item => {
              const href = safeHttpUrl(item.url);
              return html`<li><div><strong>${item.title}</strong><small>${item.subtitle || item.status || ''}</small></div>
                ${href ? html`<a href="${href}" target="_blank" rel="noreferrer noopener">Open</a>` : nothing}</li>`;
            })}</ul>
          </article>`)}</div>` : html`<div class="empty-shelf"><h4>No live collections</h4>
            <p>Any registered read-only capability can back a collection; provider and domain names are not built into Library.</p></div>`}
      </section>
    </section>`;
  }

  protected renderBoards_(): unknown {
    const boards = this.library_.boards ?? [];
    return html`<section class="library-view" aria-label="Boards">
      <div class="view-heading"><div><span class="eyebrow">SPATIAL THINKING</span><h2>Boards</h2></div>
        <span class="count-chip">${boards.filter(board => !board.archived).length} active</span></div>
      <form class="board-create" @submit="${(event: Event) => {
        event.preventDefault(); void this.createBoard_();
      }}"><input aria-label="New board name" placeholder="Name a new board"
          .value="${this.boardName_}" @input="${this.onBoardNameInput_}">
        <button class="saui-button primary" type="submit"
            ?disabled="${this.libraryBusy_ || !this.boardName_.trim()}">Create board</button></form>
      ${this.libraryError_ ? html`<div class="saui-error" role="alert">${this.libraryError_}</div>` : nothing}
      ${boards.length ? html`<div class="board-grid">${boards.map(board => this.renderBoardCard_(board))}</div>` :
        html`<div class="empty-shelf"><h4>Your first board starts empty</h4>
          <p>Create one to arrange text, links, captures, images, and live result surfaces without duplicating their underlying data.</p></div>`}
    </section>`;
  }

  protected onBoardNameInput_(event: Event) {
    this.boardName_ = (event.target as HTMLInputElement).value;
  }

  protected onLibraryQueryInput_(event: Event) {
    this.libraryQuery_ = (event.target as HTMLInputElement).value;
  }

  private renderRecord_(node: ComponentNode, entry: DataEntry|undefined) {
    const title = propString(node.props, 'title') || propString(node.props, 'label');
    return html`<article class="record-card" aria-label="${node.accessible_name || title}">
      ${title ? html`<h3>${title}</h3>` : nothing}
      <dl>${Object.entries(entry?.fields ?? {}).map(([key, value]) => html`
        <div><dt>${key}</dt><dd>${value == null ? '' : String(value)}</dd></div>`)}</dl>
    </article>`;
  }

  private renderBoardCard_(board: LibraryBoardDoc) {
    return html`<article class="board-card ${board.archived ? 'archived' : ''}">
      <div class="board-preview" aria-hidden="true">
        ${(board.elements ?? []).slice(0, 5).map((element, index) => html`
          <span class="board-fragment fragment-${index % 3}">${element.title || element.kind}</span>`)}
      </div>
      <div class="board-meta"><div><h3>${board.name}</h3>
        <p>${board.elements?.length ?? 0} elements${board.archived ? ' · Archived' : ''}</p></div>
        <div class="board-actions"><button type="button" aria-label="${board.archived ? 'Restore' : 'Archive'} ${board.name}"
            @click="${() => void this.setBoardArchived_(board, !board.archived)}">${board.archived ? 'Restore' : 'Archive'}</button>
          ${this.pendingDeleteBoardId_ === board.id ? html`
            <button class="danger-button confirmed" type="button" aria-label="Confirm delete ${board.name}"
                @click="${() => void this.deleteBoard_(board)}">Confirm delete</button>
            <button type="button" @click="${() => this.pendingDeleteBoardId_ = ''}">Cancel</button>` : html`
            <button class="danger-button" type="button" aria-label="Delete ${board.name}"
                @click="${() => this.pendingDeleteBoardId_ = board.id}">Delete</button>`}
        </div></div>
    </article>`;
  }

  private applyLibrarySnapshot_(snapshotJson: string) {
    try {
      const snapshot = JSON.parse(snapshotJson) as LibrarySnapshotDoc;
      if (snapshot.status === 'error') {
        this.libraryError_ = snapshot.detail || 'Library is unavailable.';
        return;
      }
      this.library_ = snapshot;
      this.libraryError_ = '';
    } catch {
      this.libraryError_ = 'Library returned an unreadable snapshot.';
    }
  }

  private async refreshLibrary_() {
    if (!this.pageHandler_) return;
    this.libraryBusy_ = true;
    try {
      const response = await this.pageHandler_.getLibrarySnapshot();
      this.applyLibrarySnapshot_(response.snapshotJson);
    } catch {
      this.libraryError_ = 'Library could not be reached.';
    } finally {
      this.libraryBusy_ = false;
    }
  }

  private async refreshStudio_() {
    if (!this.pageHandler_ || this.studioBusy_) return;
    this.studioBusy_ = true;
    try {
      const response = await this.pageHandler_.getStudioSnapshot();
      const snapshot = JSON.parse(response.snapshotJson) as StudioSnapshotDoc;
      if (snapshot.status === 'error') {
        this.studioError_ = snapshot.detail || 'Studio is unavailable.';
        return;
      }
      this.studio_ = snapshot;
      this.studioError_ = '';
    } catch {
      this.studioError_ = 'Studio could not read the profile runtime.';
    } finally {
      this.studioBusy_ = false;
    }
  }

  private async createBoard_() {
    const name = this.boardName_.trim();
    if (!this.pageHandler_ || !name || this.libraryBusy_) return;
    this.libraryBusy_ = true;
    try {
      const response = await this.pageHandler_.createBoard(name);
      this.applyLibrarySnapshot_(response.snapshotJson);
      if (!this.libraryError_) this.boardName_ = '';
    } catch {
      this.libraryError_ = 'The board could not be created.';
    } finally {
      this.libraryBusy_ = false;
    }
  }

  private async setBoardArchived_(board: LibraryBoardDoc, archived: boolean) {
    if (!this.pageHandler_ || this.libraryBusy_) return;
    this.libraryBusy_ = true;
    try {
      const response = await this.pageHandler_.setBoardArchived(board.id, archived);
      this.applyLibrarySnapshot_(response.snapshotJson);
    } catch {
      this.libraryError_ = 'The board could not be updated.';
    } finally {
      this.libraryBusy_ = false;
    }
  }

  private async deleteBoard_(board: LibraryBoardDoc) {
    if (!this.pageHandler_ || this.libraryBusy_) return;
    this.libraryBusy_ = true;
    try {
      const response = await this.pageHandler_.deleteBoard(board.id);
      this.applyLibrarySnapshot_(response.snapshotJson);
      if (!this.libraryError_) this.pendingDeleteBoardId_ = '';
    } catch {
      this.libraryError_ = 'The board could not be deleted.';
    } finally {
      this.libraryBusy_ = false;
    }
  }

  private renderSources_(node: ComponentNode) {
    const raw = Array.isArray(node.props?.['sources']) ? node.props!['sources'] : [];
    const sources = raw.filter((item): item is Record<string, unknown> =>
      typeof item === 'object' && item !== null);
    return html`<ol class="saui-sources">${sources.map(source => {
      const href = safeHttpUrl(source['href']);
      return href ? html`<li><a href="${href}" target="_blank" rel="noreferrer noopener">
        ${typeof source['title'] === 'string' ? source['title'] : href}</a></li>` : nothing;
    })}</ol>`;
  }

  private renderInput_(node: ComponentNode) {
    const type = node.type === 'numeric_input' ? 'number' :
        node.type === 'date_input' ? 'date' :
        node.type === 'time_input' ? 'time' :
        node.type === 'slider' ? 'range' :
        node.type === 'search_field' ? 'search' : 'text';
    return html`<label class="field"><span>${propString(node.props, 'label')}</span>
      <input type="${type}" aria-label="${node.accessible_name || propString(node.props, 'label')}"
          @change="${(event: Event) => this.emitComponentEvent_(node, ComponentEventKind.kValueChanged, (event.target as HTMLInputElement).value)}">
    </label>`;
  }

  private renderOptions_(node: ComponentNode) {
    const raw = node.props?.['options'] ?? node.props?.['segments'] ?? node.props?.['chips'];
    const options = Array.isArray(raw) ? raw.map(option =>
      typeof option === 'string' ? {label: option, value: option} : {
        label: String((option as Record<string, unknown>)['label'] ?? ''),
        value: String((option as Record<string, unknown>)['value'] ?? (option as Record<string, unknown>)['label'] ?? ''),
      }) : [];
    return html`<fieldset class="option-group"><legend>${propString(node.props, 'label')}</legend>
      ${options.map(option => html`<button type="button" class="option-chip"
          @click="${() => this.emitComponentEvent_(node, ComponentEventKind.kSelect, option.value)}">${option.label}</button>`)}
    </fieldset>`;
  }

  private emitComponentEvent_(
      node: ComponentNode, kind: number, value: unknown) {
    if (!this.pageHandler_ || !this.surface_) return;
    const actionId = node.actions?.find(id => this.currentActions_.has(id));
    this.pageHandler_.notifyComponentEvent({
      surfaceId: this.surface_.id,
      componentId: node.id,
      kind,
      actionId: actionId ?? null,
      valueJson: JSON.stringify(value ?? null),
    });
  }

  private installPageCallbacks_() {
    this.callbackRouter_.pushSurface.addListener(
        (_surfaceId: string, surfaceJson: string) => {
          try {
            const surface = JSON.parse(surfaceJson) as SurfaceDoc;
            if (!surface || !Array.isArray(surface.components)) return;
            this.surface_ = surface;
            this.currentActions_ = new Set((surface.actions ?? []).map(action => action.id));
          } catch {
            // Malformed browser data is inert. The browser normally validates it.
          }
        });
    this.callbackRouter_.applySurfacePatch.addListener(
        (_surfaceId: string, _patchJson: string) => {
          // The browser validates and applies patches, then pushes the canonical
          // full document. Never apply unvalidated operations in the renderer.
        });
    this.callbackRouter_.pushTaskSnapshot.addListener((snapshotJson: string) => {
      try {
        const snapshot = JSON.parse(snapshotJson) as TaskSnapshotDoc;
        if (!snapshot || typeof snapshot.id !== 'string') return;
        const tasks = new Map(this.tasks_.map(task => [task.id, task]));
        tasks.set(snapshot.id, snapshot);
        this.tasks_ = [...tasks.values()];
      } catch {
        // Malformed snapshots render nothing.
      }
    });
    this.callbackRouter_.setStatus.addListener((statusJson: string) => {
      try {
        const status = JSON.parse(statusJson) as Record<string, unknown>;
        this.voiceConfigured_ = status['voice_realtime_configured'] === true;
        const voiceState = typeof status['voice_state'] === 'string' ? status['voice_state'] : 'idle';
        if (!this.realtimeConnection_) this.voiceState_ = voiceState;
        const target = status['voice_product_target'] || status['voice_api_model'];
        if (status['voice_realtime_creating']) this.routeLabel_ = 'Connecting';
        else if (this.voiceConfigured_) this.routeLabel_ = typeof target === 'string' ? target : 'Voice';
        else if (status['voice_realtime_error']) this.routeLabel_ = 'Voice unavailable';
        else this.routeLabel_ = 'Voice key missing';
      } catch {
        // Inert.
      }
    });
  }

  private sendRealtimeEvent_(event: Record<string, unknown>) {
    const channel = this.realtimeConnection_?.dataChannel;
    if (channel?.readyState === 'open') channel.send(JSON.stringify(event));
  }

  private rememberRealtimeToolCall_(event: Record<string, unknown>) {
    const item = event['item'] as Record<string, unknown>|undefined;
    if (!item || item['type'] !== 'function_call') return;
    const key = String(item['id'] ?? event['item_id'] ?? '');
    const name = typeof item['name'] === 'string' ? item['name'] : '';
    const callId = typeof item['call_id'] === 'string' ? item['call_id'] : key;
    if (key && name) this.realtimeToolCalls_.set(key, {callId, name});
  }

  private extractRealtimeToolCall_(event: Record<string, unknown>): RealtimeFunctionCall|undefined {
    this.rememberRealtimeToolCall_(event);
    const item = event['item'] as Record<string, unknown>|undefined;
    const itemId = String(event['item_id'] ?? item?.['id'] ?? '');
    const remembered = itemId ? this.realtimeToolCalls_.get(itemId) : undefined;
    const type = String(event['type'] ?? '');
    const name = typeof event['name'] === 'string' ? event['name'] :
        typeof item?.['name'] === 'string' ? item['name'] : remembered?.name ?? '';
    const callId = typeof event['call_id'] === 'string' ? event['call_id'] :
        typeof item?.['call_id'] === 'string' ? item['call_id'] : remembered?.callId ?? itemId;
    const rawArgs = typeof event['arguments'] === 'string' ? event['arguments'] :
        typeof event['arguments_json'] === 'string' ? event['arguments_json'] :
        typeof item?.['arguments'] === 'string' ? item['arguments'] : '';
    const key = callId || itemId;
    if (!key || !name || !rawArgs || this.completedRealtimeToolCalls_.has(key)) return undefined;
    return ['response.function_call_arguments.done', 'response.output_item.done',
            'conversation.item.created'].includes(type) ?
        {key, callId, name, argumentsJson: rawArgs} : undefined;
  }

  private async handleRealtimeEvent_(data: string) {
    let event: Record<string, unknown>;
    try { event = JSON.parse(data) as Record<string, unknown>; } catch { return; }
    const call = this.extractRealtimeToolCall_(event);
    if (!call || !this.pageHandler_) return;
    let outputJson: string;
    try {
      const result = await this.pageHandler_.submitRealtimeToolCall({
        callId: call.callId, name: call.name, argumentsJson: call.argumentsJson,
      });
      outputJson = result.outputJson;
    } catch {
      outputJson = JSON.stringify({status: 'error', detail: 'tool_bridge_failed'});
    }
    this.completedRealtimeToolCalls_.add(call.key);
    this.sendRealtimeEvent_({
      type: 'conversation.item.create',
      item: {type: 'function_call_output', call_id: call.callId, output: outputJson},
    });
    this.sendRealtimeEvent_({type: 'response.create'});
  }

  private async stopRealtimeVoice_() {
    const connection = this.realtimeConnection_;
    this.realtimeStarting_ = false;
    this.realtimeStartGeneration_++;
    this.realtimeConnection_ = undefined;
    this.realtimeToolCalls_.clear();
    this.completedRealtimeToolCalls_.clear();
    if (connection) {
      connection.dataChannel.close();
      connection.peer.close();
      connection.stream.getTracks().forEach(track => track.stop());
      connection.audio.srcObject = null;
    }
    this.voiceState_ = 'idle';
    this.routeLabel_ = this.voiceConfigured_ ? 'Voice' : 'Voice key missing';
  }

  private async startRealtimeVoice_() {
    if (!this.pageHandler_ || this.realtimeConnection_ || this.realtimeStarting_) return;
    this.realtimeStarting_ = true;
    const generation = ++this.realtimeStartGeneration_;
    this.voiceState_ = 'connecting';
    this.routeLabel_ = 'Connecting';
    try {
      const response = await this.pageHandler_.createRealtimeVoiceSession();
      if (!this.realtimeStarting_ || generation !== this.realtimeStartGeneration_) return;
      const session = JSON.parse(response.sessionJson || '{}') as RealtimeVoiceSessionDoc;
      if (session.status !== 'ready' || !session.client_secret ||
          !session.connect_url || !session.api_model) throw new Error(session.detail);
      const stream = await navigator.mediaDevices.getUserMedia({audio: {
        echoCancellation: true, noiseSuppression: true, autoGainControl: true,
      }});
      if (generation !== this.realtimeStartGeneration_) {
        stream.getTracks().forEach(track => track.stop());
        return;
      }
      const peer = new RTCPeerConnection();
      const audio = new Audio();
      audio.autoplay = true;
      peer.ontrack = event => {
        audio.srcObject = event.streams[0] ?? null;
        void audio.play();
      };
      stream.getAudioTracks().forEach(track => peer.addTrack(track, stream));
      const dataChannel = peer.createDataChannel('oai-events');
      this.realtimeConnection_ = {peer, dataChannel, stream, audio};
      dataChannel.addEventListener('open', () => {
        this.sendRealtimeEvent_({type: 'session.update', session: {
          instructions: session.instructions ?? '', tools: session.tools ?? [], tool_choice: 'auto',
        }});
        this.voiceState_ = 'listening';
        this.routeLabel_ = session.product_target || session.api_model || 'Voice';
      });
      dataChannel.addEventListener('message', event => void this.handleRealtimeEvent_(String(event.data)));
      dataChannel.addEventListener('close', () => void this.stopRealtimeVoice_());
      const offer = await peer.createOffer();
      await peer.setLocalDescription(offer);
      const sdpResponse = await fetch(
          `${session.connect_url}?model=${encodeURIComponent(session.api_model)}`,
          {method: 'POST', body: offer.sdp ?? '', headers: {
            Authorization: `Bearer ${session.client_secret}`,
            'Content-Type': 'application/sdp',
          }});
      if (!sdpResponse.ok) throw new Error(`realtime_sdp_${sdpResponse.status}`);
      await peer.setRemoteDescription({type: 'answer', sdp: await sdpResponse.text()});
      if (generation === this.realtimeStartGeneration_) this.realtimeStarting_ = false;
    } catch {
      if (generation === this.realtimeStartGeneration_) {
        await this.stopRealtimeVoice_();
        this.routeLabel_ = 'Voice unavailable';
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'seoul-canvas-app': SeoulCanvasAppElement;
  }
}

customElements.define(SeoulCanvasAppElement.is, SeoulCanvasAppElement);
