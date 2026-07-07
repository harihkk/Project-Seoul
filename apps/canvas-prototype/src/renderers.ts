// Seoul Canvas Design Lab - component renderers.
// Renders canonical ComponentNodes against their bound data entries with safe
// DOM only (textContent, never innerHTML). Every element carries
// data-seoul-component="<id>" so the patch reconciler can update exactly the
// affected components and nothing else. The dispatcher covers every component
// type the Lab compiler emits; an unknown type renders an explained error
// state rather than a silent table.

import type { AdaptiveSurface, ComponentNode, DataEntry } from './protocol.js';
import { renderChart, renderNetworkGraph } from './charts.js';

export function elt(tag: string, cls?: string, text?: string): HTMLElement {
  const n = document.createElement(tag);
  if (cls) n.className = cls;
  if (text !== undefined) n.textContent = text;
  return n;
}

interface DisplayColumn {
  key: string;
  label: string;
  unit?: string;
  role?: string;
}

function displayColumns(node: ComponentNode): DisplayColumn[] {
  const raw = node.props?.['columns'];
  return Array.isArray(raw) ? (raw as unknown as DisplayColumn[]) : [];
}

function prop(node: ComponentNode, key: string): string {
  const v = node.props?.[key];
  return typeof v === 'string' ? v : '';
}

function boundEntry(node: ComponentNode, surface: AdaptiveSurface, slot = 'data'): DataEntry | undefined {
  const name = node.bindings?.[slot];
  return name ? surface.data?.[name] : undefined;
}

function tableRows(entry: DataEntry | undefined): Record<string, unknown>[] {
  if (!entry || entry.kind !== 'table') return [];
  const columns = entry.columns ?? [];
  return (entry.rows ?? []).map((cells) => {
    const row: Record<string, unknown> = {};
    columns.forEach((c, i) => { row[c.key] = (cells as unknown[])[i]; });
    return row;
  });
}

function fmtValue(column: DisplayColumn | undefined, value: unknown): string {
  if (value === null || value === undefined || value === '') return '-';
  if (column?.role === 'timestamp' || column?.role === 'interval_start' || column?.role === 'interval_end') {
    if (typeof value === 'number') {
      const d = new Date(value);
      return `${d.getUTCFullYear()}-${String(d.getUTCMonth() + 1).padStart(2, '0')}-${String(d.getUTCDate()).padStart(2, '0')} ${String(d.getUTCHours()).padStart(2, '0')}:${String(d.getUTCMinutes()).padStart(2, '0')}`;
    }
  }
  if (typeof value === 'number') {
    return value.toLocaleString(undefined, { maximumFractionDigits: 2 }) + (column?.unit ? ` ${column.unit}` : '');
  }
  return String(value);
}

// --- Individual renderers ----------------------------------------------------

function renderMetric(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const entry = boundEntry(node, surface);
  const box = elt('div', 'metric');
  const value = entry?.kind === 'scalar' ? entry.value : undefined;
  const unit = prop(node, 'unit');
  box.appendChild(elt('div', 'metric-value', fmtValue(unit ? { key: '', label: '', unit } : undefined, value)));
  box.appendChild(elt('div', 'metric-label', prop(node, 'label') || 'Value'));
  return box;
}

function renderRecordCard(node: ComponentNode, surface: AdaptiveSurface, dense: boolean): HTMLElement {
  const entry = boundEntry(node, surface);
  const record = entry?.kind === 'record' ? (entry.fields ?? {}) : {};
  const columns = displayColumns(node);
  const grid = elt('div', dense ? 'kv' : 'card-grid');
  for (const column of columns) {
    const row = elt('div', 'kv-row');
    row.appendChild(elt('span', 'kv-key', column.label));
    const val = elt('span', 'kv-val', fmtValue(column, (record as Record<string, unknown>)[column.key]));
    if (column.role === 'status') {
      val.classList.add('status-pill');
      val.dataset.status = String((record as Record<string, unknown>)[column.key] ?? '').toLowerCase();
    }
    row.appendChild(val);
    grid.appendChild(row);
  }
  return grid;
}

function renderTable(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const entry = boundEntry(node, surface);
  const columns = displayColumns(node);
  const entryColumns = entry?.kind === 'table' ? (entry.columns ?? []) : [];
  const byKey = new Map(columns.map((c) => [c.key, c]));
  const rows = tableRows(entry);
  const wrap = elt('div', 'table-wrap');
  const table = elt('table', 'data-table');
  const thead = elt('thead');
  const htr = elt('tr');
  for (const c of entryColumns) {
    const display = byKey.get(c.key);
    const numeric = display?.role !== undefined && rows.every((r) => typeof r[c.key] === 'number' || r[c.key] === '' || r[c.key] === undefined) && rows.some((r) => typeof r[c.key] === 'number');
    htr.appendChild(elt('th', numeric ? 'num' : '', c.label + (display?.unit ? ` (${display.unit})` : '')));
  }
  thead.appendChild(htr);
  table.appendChild(thead);
  const tbody = elt('tbody');
  for (const r of rows) {
    const tr = elt('tr');
    for (const c of entryColumns) {
      const display = byKey.get(c.key);
      const numeric = typeof r[c.key] === 'number';
      const td = elt('td', numeric ? 'num' : '', fmtValue(display, r[c.key]));
      if (display?.role === 'status') {
        td.classList.add('status-cell');
        td.dataset.status = String(r[c.key] ?? '').toLowerCase();
      }
      tr.appendChild(td);
    }
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  wrap.appendChild(table);
  return wrap;
}

function renderComparison(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const entry = boundEntry(node, surface);
  const rows = tableRows(entry);
  const idKey = prop(node, 'id_key');
  const columns = displayColumns(node).filter((c) => c.key !== idKey);
  const table = elt('table', 'data-table');
  const htr = elt('tr');
  htr.appendChild(elt('th', '', ''));
  rows.forEach((r) => htr.appendChild(elt('th', 'num', String(r[idKey] ?? ''))));
  const thead = elt('thead');
  thead.appendChild(htr);
  table.appendChild(thead);
  const tbody = elt('tbody');
  for (const column of columns) {
    const tr = elt('tr');
    tr.appendChild(elt('td', 'row-head', column.label));
    rows.forEach((r) => tr.appendChild(elt('td', typeof r[column.key] === 'number' ? 'num' : '', fmtValue(column, r[column.key]))));
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  const wrap = elt('div', 'table-wrap');
  wrap.appendChild(table);
  return wrap;
}

function renderTree(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const rows = tableRows(boundEntry(node, surface));
  const idKey = prop(node, 'id_key');
  const nameKey = prop(node, 'name_key');
  const parentKey = prop(node, 'parent_key');
  const byParent = new Map<string, Record<string, unknown>[]>();
  for (const r of rows) {
    const p = String(r[parentKey] ?? '');
    if (!byParent.has(p)) byParent.set(p, []);
    byParent.get(p)!.push(r);
  }
  const build = (parentId: string): HTMLElement => {
    const ul = elt('ul', 'tree');
    for (const item of byParent.get(parentId) || []) {
      const li = elt('li');
      li.appendChild(elt('span', 'tree-node', String(item[nameKey] ?? item[idKey] ?? '')));
      const kids = byParent.get(String(item[idKey]));
      if (kids && kids.length) li.appendChild(build(String(item[idKey])));
      ul.appendChild(li);
    }
    return ul;
  };
  return build('');
}

function renderSourceList(node: ComponentNode): HTMLElement {
  // Sources arrive as the native structured prop: [{href, title}].
  const raw = node.props?.['sources'];
  const sources = Array.isArray(raw) ? (raw as unknown as { href: string; title?: string }[]) : [];
  const list = elt('div', 'source-list');
  sources.forEach((source, i) => {
    const item = elt('div', 'source-item');
    item.appendChild(elt('span', 'source-index', String(i + 1).padStart(2, '0')));
    const body = elt('div', 'source-body');
    body.appendChild(elt('div', 'source-title', String(source.title ?? 'Untitled')));
    const a = elt('a', 'source-url', source.href) as HTMLAnchorElement;
    if (/^https?:\/\//.test(source.href)) { a.href = source.href; a.target = '_blank'; a.rel = 'noopener noreferrer'; }
    body.appendChild(a);
    item.appendChild(body);
    list.appendChild(item);
  });
  return list;
}

function renderTimeline(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const rows = tableRows(boundEntry(node, surface));
  const whenKey = prop(node, 'when_key');
  const endKey = prop(node, 'end_key');
  const whatKey = prop(node, 'what_key');
  const columns = new Map(displayColumns(node).map((c) => [c.key, c]));
  const line = elt('div', 'timeline');
  rows.forEach((r) => {
    const item = elt('div', 'timeline-item');
    let when = fmtValue(columns.get(whenKey), r[whenKey]);
    if (endKey && r[endKey] !== undefined && r[endKey] !== '') {
      when += ` → ${fmtValue(columns.get(endKey), r[endKey])}`;
    }
    item.appendChild(elt('div', 'timeline-when', when));
    item.appendChild(elt('div', 'timeline-what', String(r[whatKey] ?? '-')));
    line.appendChild(item);
  });
  return line;
}

function renderDocument(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const box = elt('div', 'document');
  const titleKey = prop(node, 'title_key');
  const bodyKey = prop(node, 'body_key');
  if (node.props?.['sectioned']) {
    const rows = tableRows(boundEntry(node, surface));
    rows.forEach((r) => {
      const sec = elt('section', 'section');
      sec.appendChild(elt('h4', 'section-title', String(r[titleKey] ?? '')));
      sec.appendChild(elt('p', 'document-body', String(r[bodyKey] ?? '')));
      box.appendChild(sec);
    });
    return box;
  }
  const entry = boundEntry(node, surface);
  const record = entry?.kind === 'record' ? (entry.fields ?? {}) : {};
  const title = String((record as Record<string, unknown>)[titleKey] ?? '');
  if (title) box.appendChild(elt('h4', 'section-title', title));
  box.appendChild(elt('p', 'document-body', String((record as Record<string, unknown>)[bodyKey] ?? '')));
  return box;
}

function renderDiff(node: ComponentNode): HTMLElement {
  // The diff text is the native `diff` prop.
  const box = elt('div', 'diff-view');
  const title = prop(node, 'title');
  if (title) box.appendChild(elt('h4', 'section-title', title));
  const pre = elt('pre', 'diff-body');
  for (const line of prop(node, 'diff').split('\n')) {
    const cls = line.startsWith('+') ? 'diff-add' : line.startsWith('-') ? 'diff-del' : line.startsWith('@@') ? 'diff-hunk' : '';
    pre.appendChild(elt('span', `diff-line ${cls}`.trim(), line + '\n'));
  }
  box.appendChild(pre);
  return box;
}

function renderSchemaFormPreview(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const rows = tableRows(boundEntry(node, surface));
  const box = elt('div', 'form-preview');
  box.appendChild(elt('div', 'form-note', 'Form preview - inputs are inert in the Design Lab.'));
  rows.forEach((r) => {
    const row = elt('div', 'form-row');
    const required = r['required'] === true || r['required'] === 'true';
    row.appendChild(elt('span', 'form-label', String(r['label'] ?? r['field_id'] ?? '') + (required ? ' *' : '')));
    row.appendChild(elt('span', 'form-kind', String(r['input_kind'] ?? 'text')));
    const hint = String(r['hint'] ?? '');
    if (hint && hint !== '-') row.appendChild(elt('span', 'form-hint', hint));
    box.appendChild(row);
  });
  return box;
}

function renderActionList(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const rows = tableRows(boundEntry(node, surface));
  const labelKey = prop(node, 'label_key');
  const summaryKey = prop(node, 'summary_key');
  const box = elt('div', 'action-list');
  box.appendChild(elt('div', 'form-note', 'Synthetic action set - buttons are inert in the Design Lab.'));
  rows.forEach((r) => {
    const row = elt('div', 'action-row');
    const button = elt('button', 'action-btn', String(r[labelKey] ?? '')) as HTMLButtonElement;
    button.disabled = true;
    row.appendChild(button);
    if (summaryKey) row.appendChild(elt('span', 'action-summary', String(r[summaryKey] ?? '')));
    box.appendChild(row);
  });
  return box;
}

function renderStateBox(node: ComponentNode, kind: 'empty' | 'error'): HTMLElement {
  const box = elt('div', kind === 'empty' ? 'empty-state' : 'error-state');
  box.appendChild(elt('div', 'empty-title', kind === 'empty' ? 'No data' : 'Failed'));
  box.appendChild(elt('div', 'empty-sub', prop(node, 'text') || (kind === 'empty' ? 'The capability returned no rows.' : 'The capability reported an error.')));
  return box;
}

/**
 * Renders one component subtree. The returned element (and every child
 * component's element) carries data-seoul-component so patches can re-render
 * exactly the affected components.
 */
export function renderComponent(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  let out: HTMLElement;
  switch (node.type) {
    case 'metric': out = renderMetric(node, surface); break;
    case 'entity_card': out = renderRecordCard(node, surface, false); break;
    case 'key_value_card': out = renderRecordCard(node, surface, true); break;
    case 'table':
    case 'sortable_table': out = renderTable(node, surface); break;
    case 'comparison_matrix': out = renderComparison(node, surface); break;
    case 'tree': out = renderTree(node, surface); break;
    case 'source_list': out = renderSourceList(node); break;
    case 'timeline': out = renderTimeline(node, surface); break;
    case 'document': out = renderDocument(node, surface); break;
    case 'diff_view': out = renderDiff(node); break;
    case 'schema_form': out = renderSchemaFormPreview(node, surface); break;
    case 'list': out = node.props?.['as_actions'] ? renderActionList(node, surface) : renderTable(node, surface); break;
    case 'line_chart':
    case 'area_chart':
    case 'bar_chart':
    case 'scatter_chart': out = renderChart(node, surface); break;
    case 'network_graph': out = renderNetworkGraph(node, surface); break;
    case 'badge': out = elt('span', 'badge', prop(node, 'text')); break;
    case 'text': out = elt('p', 'text-block', prop(node, 'text')); break;
    case 'heading': out = elt('h3', 'heading-block', prop(node, 'text')); break;
    case 'empty_state': out = renderStateBox(node, 'empty'); break;
    case 'error_state': out = renderStateBox(node, 'error'); break;
    case 'stack': {
      out = elt('div', 'sections');
      (node.children ?? []).forEach((child) => {
        const sec = elt('section', 'section');
        sec.appendChild(renderComponent(child, surface));
        out.appendChild(sec);
      });
      break;
    }
    default:
      // An accepted-but-unimplemented type would be dishonest to render as an
      // unexplained table; name the gap instead.
      out = elt('div', 'error-state', `The Design Lab has no renderer for component type "${node.type}".`);
      break;
  }
  out.dataset.seoulComponent = node.id;
  if (node.state === 'loading') out.classList.add('is-loading');
  if (node.state === 'error') out.classList.add('is-error');
  return out;
}
