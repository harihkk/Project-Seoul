// Project Seoul Canvas prototype - surface renderer + the Canvas application.
// Renders a compiled Surface into a trusted artifact (no untrusted markup,
// safe DOM only) and wires the product loop: a goal enters the command bar,
// the planner selects a capability, the task runs and is verified, its
// semantic result is compiled by the adaptive interface compiler, and the
// artifact is rendered. A follow-up representation switch re-compiles the
// SAME surface in place - it never spawns an unrelated duplicate.
//
// Layout: a worklog document, not a dashboard. Each run appends a numbered
// entry whose receipt (capability id, freshness, verification) is typeset as
// marginalia in a left margin column - the Tufte sidenote convention - and
// whose content sits directly on the page. There is no sidebar, no cards,
// and no filled chips; structure comes from hairline rules and type.

import { renderChart } from './charts.js';
import {
  type Component,
  type ComponentType,
  type Surface,
  availableRepresentations,
  compileInterface,
} from './compiler.js';
import { capabilities, runTask } from './runtime.js';
import { type FieldSpec, type SemanticResult, asRows } from './semantic.js';

function elt(tag: string, cls?: string, text?: string): HTMLElement {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text; // textContent = no HTML injection
  return n;
}

function fmtCell(field: FieldSpec | undefined, value: unknown): string {
  if (value === null || value === undefined || value === '') return '-';
  if (field?.primitive === 'timestamp' && typeof value === 'number') {
    const d = new Date(value);
    return `${d.getUTCFullYear()}-${String(d.getUTCMonth() + 1).padStart(2, '0')}-${String(d.getUTCDate()).padStart(2, '0')} ${String(d.getUTCHours()).padStart(2, '0')}:${String(d.getUTCMinutes()).padStart(2, '0')}`;
  }
  if ((field?.primitive === 'number' || field?.primitive === 'integer') && typeof value === 'number') {
    return value.toLocaleString(undefined, { maximumFractionDigits: 2 }) + (field.unit ? ` ${field.unit}` : '');
  }
  return String(value);
}

function isNumeric(field: FieldSpec | undefined): boolean {
  return field?.primitive === 'number' || field?.primitive === 'integer';
}

// --- Component renderers ----------------------------------------------------

function renderMetric(result: SemanticResult): HTMLElement {
  const field = result.schema.fields[0];
  const box = elt('div', 'metric');
  box.appendChild(elt('div', 'metric-value', fmtCell(field, result.data)));
  box.appendChild(elt('div', 'metric-label', field?.label ?? 'Value'));
  return box;
}

function renderRecord(result: SemanticResult, dense: boolean): HTMLElement {
  const obj = (result.data || {}) as Record<string, unknown>;
  const grid = elt('div', dense ? 'kv' : 'card-grid');
  for (const f of result.schema.fields) {
    const row = elt('div', 'kv-row');
    row.appendChild(elt('span', 'kv-key', f.label));
    const val = elt('span', 'kv-val', fmtCell(f, obj[f.id]));
    if (f.role === 'status') {
      val.classList.add('status-pill');
      val.dataset.status = String(obj[f.id] ?? '').toLowerCase();
    }
    row.appendChild(val);
    grid.appendChild(row);
  }
  return grid;
}

function renderTable(result: SemanticResult): HTMLElement {
  const rows = asRows(result);
  const wrap = elt('div', 'table-wrap');
  const table = elt('table', 'data-table') as HTMLTableElement;
  const thead = elt('thead');
  const htr = elt('tr');
  for (const f of result.schema.fields) {
    const th = elt('th', isNumeric(f) ? 'num' : '', f.label + (f.unit ? ` (${f.unit})` : ''));
    htr.appendChild(th);
  }
  thead.appendChild(htr);
  table.appendChild(thead);
  const tbody = elt('tbody');
  for (const r of rows) {
    const tr = elt('tr');
    for (const f of result.schema.fields) {
      const td = elt('td', isNumeric(f) ? 'num' : '', fmtCell(f, r[f.id]));
      if (f.role === 'status') { td.classList.add('status-cell'); td.dataset.status = String(r[f.id] ?? '').toLowerCase(); }
      tr.appendChild(td);
    }
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  wrap.appendChild(table);
  return wrap;
}

function renderTree(result: SemanticResult): HTMLElement {
  const rows = asRows(result);
  const byParent = new Map<string, Record<string, unknown>[]>();
  for (const r of rows) {
    const p = String(r['parent'] ?? '');
    if (!byParent.has(p)) byParent.set(p, []);
    byParent.get(p)!.push(r);
  }
  const build = (parentId: string): HTMLElement => {
    const ul = elt('ul', 'tree');
    for (const node of byParent.get(parentId) || []) {
      const li = elt('li');
      li.appendChild(elt('span', 'tree-node', String(node['name'] ?? node['id'])));
      const kids = byParent.get(String(node['id']));
      if (kids && kids.length) li.appendChild(build(String(node['id'])));
      ul.appendChild(li);
    }
    return ul;
  };
  // Root = rows whose parent is empty.
  return build('');
}

function renderSourceList(result: SemanticResult): HTMLElement {
  const rows = asRows(result);
  const list = elt('div', 'source-list');
  rows.forEach((r, i) => {
    const item = elt('div', 'source-item');
    item.appendChild(elt('span', 'source-index', String(i + 1).padStart(2, '0')));
    const body = elt('div', 'source-body');
    body.appendChild(elt('div', 'source-title', String(r['title'] ?? 'Untitled')));
    const url = String(r['url'] ?? '');
    const a = elt('a', 'source-url', url) as HTMLAnchorElement;
    if (/^https?:\/\//.test(url)) { a.href = url; a.target = '_blank'; a.rel = 'noopener noreferrer'; }
    body.appendChild(a);
    item.appendChild(body);
    list.appendChild(item);
  });
  return list;
}

function renderTimeline(result: SemanticResult): HTMLElement {
  const rows = asRows(result);
  const line = elt('div', 'timeline');
  const tsField = result.schema.fields.find((f) => f.role === 'timestamp' || f.role === 'interval_start');
  const nameField = result.schema.fields.find((f) => f.role === 'name' || f.role === 'identifier') || result.schema.fields[0];
  rows.forEach((r) => {
    const item = elt('div', 'timeline-item');
    item.appendChild(elt('div', 'timeline-when', fmtCell(tsField, tsField ? r[tsField.id] : '')));
    item.appendChild(elt('div', 'timeline-what', fmtCell(nameField, nameField ? r[nameField.id] : '')));
    line.appendChild(item);
  });
  return line;
}

function renderComparison(result: SemanticResult): HTMLElement {
  // Entities become columns, fields become rows.
  const rows = asRows(result);
  const idField = result.schema.fields.find((f) => f.role === 'identifier') || result.schema.fields[0];
  const valueFields = result.schema.fields.filter((f) => f !== idField);
  const table = elt('table', 'data-table') as HTMLTableElement;
  const htr = elt('tr');
  htr.appendChild(elt('th', '', ''));
  rows.forEach((r) => htr.appendChild(elt('th', 'num', String(r[idField.id]))));
  const thead = elt('thead');
  thead.appendChild(htr);
  table.appendChild(thead);
  const tbody = elt('tbody');
  for (const f of valueFields) {
    const tr = elt('tr');
    tr.appendChild(elt('td', 'row-head', f.label));
    rows.forEach((r) => tr.appendChild(elt('td', isNumeric(f) ? 'num' : '', fmtCell(f, r[f.id]))));
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  const wrap = elt('div', 'table-wrap');
  wrap.appendChild(table);
  return wrap;
}

function renderComponent(component: Component): HTMLElement {
  switch (component.type) {
    case 'metric': return renderMetric(component.result!);
    case 'entity_card': return renderRecord(component.result!, false);
    case 'key_value': return renderRecord(component.result!, true);
    case 'table': return renderTable(component.result!);
    case 'comparison_matrix': return renderComparison(component.result!);
    case 'tree': return renderTree(component.result!);
    case 'source_list': return renderSourceList(component.result!);
    case 'timeline': return renderTimeline(component.result!);
    case 'line_chart':
    case 'area_chart':
    case 'bar_chart':
    case 'scatter_chart':
      return renderChart(component);
    case 'document_sections':
    case 'sections': {
      const box = elt('div', 'sections');
      (component.children || []).forEach((child) => {
        const sec = elt('section', 'section');
        if (child.title) sec.appendChild(elt('h4', 'section-title', child.title));
        sec.appendChild(renderComponent(child));
        box.appendChild(sec);
      });
      return box;
    }
    case 'empty_state': {
      const box = elt('div', 'empty-state');
      box.appendChild(elt('div', 'empty-title', 'No data'));
      box.appendChild(elt('div', 'empty-sub', component.result?.note || 'The capability returned no rows for this request.'));
      return box;
    }
    default:
      return renderTable(component.result!);
  }
}

const REP_LABEL: Record<string, string> = {
  table: 'Table', bar_chart: 'Bar', line_chart: 'Line', area_chart: 'Area',
  scatter_chart: 'Scatter', entity_card: 'Card', key_value: 'Details', metric: 'Metric',
};

function freshnessLabel(f: string): string {
  return { real_time: 'live', near_real_time: 'near-live', cached: 'cached', static: 'static' }[f] || f;
}

// --- The Canvas application -------------------------------------------------

interface AppState {
  surfaces: { surface: Surface; el: HTMLElement }[];
  taskCount: number;
}

export function mountCanvas(root: HTMLElement): void {
  const state: AppState = { surfaces: [], taskCount: 0 };

  root.innerHTML = '';
  const app = elt('div', 'app');

  // Masthead.
  const mast = elt('header', 'masthead');
  const brand = elt('div', 'brand');
  brand.appendChild(elt('span', 'brand-title', 'Seoul'));
  brand.appendChild(elt('span', 'brand-sub', 'canvas'));
  mast.appendChild(brand);
  const mastRight = elt('div', 'mast-right');
  mastRight.appendChild(elt('span', 'mast-note', `${capabilities().length} capabilities registered`));
  const themeBtn = elt('button', 'ghost-btn', 'Light') as HTMLButtonElement;
  themeBtn.title = 'Toggle theme';
  themeBtn.addEventListener('click', () => {
    const dark = document.documentElement.dataset.theme !== 'light';
    document.documentElement.dataset.theme = dark ? 'light' : 'dark';
    themeBtn.textContent = dark ? 'Dark' : 'Light';
  });
  mastRight.appendChild(themeBtn);
  mast.appendChild(mastRight);
  app.appendChild(mast);

  // The worklog document.
  const stack = elt('main', 'stack');
  const lede = elt('div', 'doc-lede');
  lede.appendChild(elt('p', 'lede-line', 'A goal goes in. The interface compiles itself from the shape of what comes back.'));
  stack.appendChild(lede);

  // Editorial index of registered capabilities - the discovery surface.
  // Clicking a row runs it. This replaces suggestion chips and lives in the
  // document itself, like a contents page.
  const index = elt('section', 'cap-index');
  index.appendChild(elt('div', 'index-heading', 'Index of capabilities'));
  const examples: [string, string, string][] = [
    ['Pipeline latency over time', 'show the pipeline latency timeline', 'time series'],
    ['Substrate survey readings', 'collect the substrate survey readings per station', 'entity collection'],
    ['Compute node profile', 'describe the compute node profile with region and status', 'record'],
    ['Workspace hierarchy', 'list the workspace project hierarchy tree', 'hierarchy'],
    ['Reference citations', 'return the reference citations for the thread', 'citations'],
    ['Reliability metric', 'report the current reliability metric', 'scalar'],
  ];
  examples.forEach(([label, goal, shape], i) => {
    const row = elt('button', 'index-row') as HTMLButtonElement;
    row.appendChild(elt('span', 'index-num', String(i + 1).padStart(2, '0')));
    row.appendChild(elt('span', 'index-name', label));
    row.appendChild(elt('span', 'index-shape', shape));
    row.addEventListener('click', () => { input.value = goal; run(); });
    index.appendChild(row);
  });
  stack.appendChild(index);
  app.appendChild(stack);

  // Bottom command bar - the single input to the product loop.
  const bar = elt('footer', 'command-bar');
  bar.appendChild(elt('span', 'prompt-mark', '\u203a'));
  const input = elt('input', 'composer-input') as HTMLInputElement;
  input.type = 'text';
  input.placeholder = 'State a goal - "show the pipeline latency timeline"';
  input.setAttribute('aria-label', 'Goal');
  bar.appendChild(input);
  const runBtn = elt('button', 'primary-btn', 'Run') as HTMLButtonElement;
  bar.appendChild(runBtn);
  app.appendChild(bar);

  root.appendChild(app);

  function marginNote(cls: string, text: string): HTMLElement {
    return elt('div', `m-line ${cls}`, text);
  }

  // Entry scaffold: [marginalia | content], hairline-ruled, no box.
  function makeEntry(): { entry: HTMLElement; margin: HTMLElement; content: HTMLElement } {
    const entry = elt('article', 'artifact');
    const margin = elt('div', 'artifact-margin');
    const content = elt('div', 'artifact-content');
    entry.appendChild(margin);
    entry.appendChild(content);
    return { entry, margin, content };
  }

  function renderSurfaceCard(surface: Surface, entryNo: number): HTMLElement {
    const { entry, margin, content } = makeEntry();

    const prov = surface.result.provenance;
    margin.appendChild(marginNote('m-num', String(entryNo).padStart(3, '0')));
    margin.appendChild(marginNote('m-cap', prov.source));
    const fresh = marginNote('m-fresh', freshnessLabel(prov.freshness));
    fresh.dataset.fresh = prov.freshness;
    margin.appendChild(fresh);
    margin.appendChild(marginNote('m-ok', 'verified'));
    if (surface.reasons[0]) margin.appendChild(marginNote('m-reason', surface.reasons[0].replace(/_/g, ' ')));

    content.appendChild(elt('h2', 'artifact-title', surface.title));

    // Representation switch: plain text links, not a segmented control.
    const reps = availableRepresentations(surface.result);
    if (reps.length > 1) {
      const row = elt('div', 'rep-row');
      const current = surface.intent.requestedRepresentation || surface.root.type;
      reps.forEach((rep) => {
        const b = elt('button', 'rep-btn', REP_LABEL[rep] || rep) as HTMLButtonElement;
        if (rep === current) b.classList.add('active');
        b.addEventListener('click', () => switchRepresentation(surface, rep, entryNo));
        row.appendChild(b);
      });
      content.appendChild(row);
    }

    const bodyEl = elt('div', 'artifact-body');
    bodyEl.appendChild(renderComponent(surface.root));
    content.appendChild(bodyEl);
    return entry;
  }

  function switchRepresentation(surface: Surface, rep: ComponentType, entryNo: number): void {
    const entryRec = state.surfaces.find((s) => s.surface.id === surface.id);
    if (!entryRec) return;
    // Re-compile the SAME surface id in place (patch, not duplicate).
    const recompiled = compileInterface(
      surface.result,
      { ...surface.intent, requestedRepresentation: rep, title: surface.title },
      surface.id,
    );
    const newCard = renderSurfaceCard(recompiled, entryNo);
    entryRec.el.replaceWith(newCard);
    entryRec.surface = recompiled;
    entryRec.el = newCard;
  }

  function run(): void {
    const goal = input.value.trim();
    if (!goal) return;
    state.taskCount++;
    const task = runTask(goal);
    if (!task.ok || !task.result) {
      // Honest failure entry, not a crash.
      const { entry, margin, content } = makeEntry();
      entry.classList.add('artifact-fail');
      margin.appendChild(marginNote('m-num', String(state.taskCount).padStart(3, '0')));
      margin.appendChild(marginNote('m-bad', 'no match'));
      content.appendChild(elt('h2', 'artifact-title', 'No capability matched'));
      content.appendChild(elt('div', 'artifact-body fail-body', task.failure || 'Planning failed.'));
      stack.appendChild(entry);
      finishRun();
      return;
    }
    const surface = compileInterface(task.result, { title: capabilityTitle(goal, task.receipt?.capabilityId) });
    const card = renderSurfaceCard(surface, state.taskCount);
    state.surfaces.unshift({ surface, el: card });
    stack.appendChild(card);
    finishRun();
  }

  function finishRun(): void {
    input.value = '';
    // The worklog reads chronologically; follow it to the newest entry.
    stack.scrollTo({ top: stack.scrollHeight });
    input.focus();
  }

  function capabilityTitle(goal: string, capId?: string): string {
    const cap = capabilities().find((c) => c.id === capId);
    // Title from the capability's own name, capitalized; falls back to the goal.
    if (cap) return cap.name.replace(/\b\w/g, (m) => m.toUpperCase());
    return goal;
  }

  runBtn.addEventListener('click', run);
  input.addEventListener('keydown', (ev) => {
    if (ev.key === 'Enter') { ev.preventDefault(); run(); }
  });

  // A first entry so the document is never blank on open.
  input.value = 'show the pipeline latency timeline';
  run();
}
