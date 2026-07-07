// Seoul Canvas Design Lab - a small, local, trusted SVG chart layer.
// Renders line, area, bar, and scatter with axes, faint gridlines, a legend,
// and a hover crosshair + haloed focus dots + tooltip, plus a simple circular
// network-graph layout. SVG only, no remote assets, sized responsively, built
// with safe DOM nodes (no innerHTML). Charts read a canonical table data
// entry through the component's "data" binding; the compiler declares the x
// column and series columns as props (x_key/x_kind/columns), so the chart
// needs no schema access. Every chart is paired with an accessible table via
// the representation switch, so a chart is never the only path to the data.

import type { AdaptiveSurface, ComponentNode, DataEntry } from './protocol.js';

const NS = 'http://www.w3.org/2000/svg';
// The muted editorial series palette (amber / teal / mauve / clay), themed via
// CSS custom properties so dark and light each get tuned values.
const SERIES_COLORS = ['var(--s1)', 'var(--s2)', 'var(--s3)', 'var(--s4)'];
let gradSeq = 0;

function el<K extends keyof SVGElementTagNameMap>(
  name: K,
  attrs: Record<string, string | number> = {},
): SVGElementTagNameMap[K] {
  const node = document.createElementNS(NS, name);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, String(v));
  return node;
}

function fmtNumber(n: number): string {
  if (!isFinite(n)) return '-';
  const abs = Math.abs(n);
  if (abs >= 1000) return n.toLocaleString(undefined, { maximumFractionDigits: 0 });
  return n.toLocaleString(undefined, { maximumFractionDigits: 2 });
}

// Round a range to a "nice" number (1/2/5 x 10^n) so axis ticks read
// 0/250/500/750/1000 instead of 184.78/385.93/...
function niceNum(range: number, round: boolean): number {
  if (range <= 0 || !isFinite(range)) return 1;
  const exp = Math.floor(Math.log10(range));
  const frac = range / Math.pow(10, exp);
  let nice: number;
  if (round) nice = frac < 1.5 ? 1 : frac < 3 ? 2 : frac < 7 ? 5 : 10;
  else nice = frac <= 1 ? 1 : frac <= 2 ? 2 : frac <= 5 ? 5 : 10;
  return nice * Math.pow(10, exp);
}

interface SeriesSpec {
  key: string;
  label: string;
  unit: string;
}

interface ChartInput {
  xKind: 'timestamp' | 'number' | 'category';
  xKey: string;
  series: SeriesSpec[];
  rows: Record<string, unknown>[];
}

function tableRowsAsObjects(entry: DataEntry): Record<string, unknown>[] {
  const columns = entry.columns ?? [];
  return (entry.rows ?? []).map((cells) => {
    const row: Record<string, unknown> = {};
    columns.forEach((c, i) => {
      row[c.key] = (cells as unknown[])[i];
    });
    return row;
  });
}

function chartInput(node: ComponentNode, surface: AdaptiveSurface): ChartInput | null {
  const entryName = node.bindings?.['data'];
  const entry = entryName ? surface.data?.[entryName] : undefined;
  if (!entry) return null;
  const props = node.props ?? {};
  if (entry.kind === 'series') {
    // A directly bound series entry: single series named by y_label.
    const rows = (entry.points ?? []).map((p) => ({ x: p.t_ms ?? p.x ?? 0, y: p.y }));
    return {
      xKind: (entry.points ?? []).some((p) => p.t_ms !== undefined) ? 'timestamp' : 'number',
      xKey: 'x',
      series: [{ key: 'y', label: String(props['y_label'] ?? 'value'), unit: String(props['units'] ?? '') }],
      rows,
    };
  }
  if (entry.kind !== 'table') return null;
  const series = Array.isArray(props['columns'])
    ? (props['columns'] as Record<string, string>[]).map((c) => ({
        key: String(c['key'] ?? ''),
        label: String(c['label'] ?? c['key'] ?? ''),
        unit: String(c['unit'] ?? ''),
      }))
    : [];
  return {
    xKind: (props['x_kind'] as ChartInput['xKind']) ?? 'category',
    xKey: String(props['x_key'] ?? ''),
    series,
    rows: tableRowsAsObjects(entry),
  };
}

function fmtX(xKind: ChartInput['xKind'], value: unknown): string {
  if (xKind === 'timestamp' && typeof value === 'number') {
    const d = new Date(value);
    return `${d.getUTCMonth() + 1}/${d.getUTCDate()} ${String(d.getUTCHours()).padStart(2, '0')}:00`;
  }
  return String(value ?? '');
}

interface Point {
  x: number; // pixel
  y: number; // pixel
  rawX: unknown;
  rawY: number;
  label: string;
}

// Renders a chart component (line/area/bar/scatter) bound to canonical data.
export function renderChart(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const wrap = document.createElement('div');
  wrap.className = 'chart';
  const input = chartInput(node, surface);
  if (!input || input.rows.length === 0 || input.series.length === 0) {
    wrap.textContent = 'Chart data unavailable.';
    return wrap;
  }
  const { xKind, xKey, series, rows } = input;
  const type = node.type;

  const width = 880;
  const height = 320;
  const pad = { top: 14, right: 4, bottom: 38, left: 52 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;

  const svg = el('svg', {
    viewBox: `0 0 ${width} ${height}`,
    class: 'chart-svg',
    role: 'img',
    'aria-label': node.accessible_name ?? `${type.replace(/_/g, ' ')} of ${series.map((s) => s.label).join(', ')}`,
  });
  svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');

  const numericX = xKind === 'timestamp' || xKind === 'number';
  const xs = rows.map((r) => r[xKey]);
  const xNums = numericX ? xs.map((v) => Number(v)) : rows.map((_, i) => i);
  const xMin = Math.min(...xNums);
  const xMax = Math.max(...xNums);

  let yMin = Infinity;
  let yMax = -Infinity;
  for (const s of series) {
    for (const r of rows) {
      const v = Number(r[s.key]);
      if (isFinite(v)) {
        yMin = Math.min(yMin, v);
        yMax = Math.max(yMax, v);
      }
    }
  }
  if (!isFinite(yMin)) { yMin = 0; yMax = 1; }
  if (type === 'bar_chart' || type === 'area_chart') yMin = Math.min(0, yMin);
  if (yMin === yMax) yMax = yMin + 1;
  const yTickTarget = 5;
  const yStep = niceNum(niceNum(yMax - yMin, false) / (yTickTarget - 1), true);
  yMin = Math.floor(yMin / yStep) * yStep;
  yMax = Math.ceil(yMax / yStep) * yStep;

  const banded = type === 'bar_chart' && !numericX;
  const sx = (v: number) =>
    banded
      ? pad.left + ((v + 0.5) / Math.max(1, rows.length)) * plotW
      : pad.left + (xMax === xMin ? plotW / 2 : ((v - xMin) / (xMax - xMin)) * plotW);
  const sy = (v: number) => pad.top + plotH - ((v - yMin) / (yMax - yMin)) * plotH;

  const yCount = Math.round((yMax - yMin) / yStep);
  for (let i = 0; i <= yCount; i++) {
    const v = yMin + i * yStep;
    const y = sy(v);
    svg.appendChild(el('line', { x1: pad.left, y1: y, x2: width - pad.right, y2: y, class: 'grid' }));
    const t = el('text', { x: pad.left - 10, y: y + 3.5, class: 'axis-label', 'text-anchor': 'end' });
    t.textContent = fmtNumber(v);
    svg.appendChild(t);
  }

  const xTickCount = Math.min(rows.length, 6);
  for (let i = 0; i < xTickCount; i++) {
    const idx = Math.round((i * (rows.length - 1)) / Math.max(1, xTickCount - 1));
    const xv = numericX ? Number(xs[idx]) : idx;
    const x = sx(xv);
    const anchor = i === 0 ? 'start' : i === xTickCount - 1 ? 'end' : 'middle';
    const t = el('text', { x, y: height - pad.bottom + 20, class: 'axis-label', 'text-anchor': anchor });
    t.textContent = fmtX(xKind, xs[idx]);
    svg.appendChild(t);
  }

  svg.appendChild(el('line', { x1: pad.left, y1: pad.top, x2: pad.left, y2: pad.top + plotH, class: 'axis' }));
  svg.appendChild(el('line', { x1: pad.left, y1: pad.top + plotH, x2: width - pad.right, y2: pad.top + plotH, class: 'axis' }));

  const allPoints: Point[][] = [];
  const focusDots: SVGCircleElement[] = [];

  series.forEach((s, si) => {
    const color = SERIES_COLORS[si % SERIES_COLORS.length];
    const pts: Point[] = rows.map((r, i) => {
      const rawY = Number(r[s.key]);
      const xv = numericX ? Number(xs[i]) : i;
      return { x: sx(xv), y: sy(rawY), rawX: xs[i], rawY, label: s.label };
    });
    allPoints.push(pts);

    if (type === 'bar_chart') {
      const groupW = (plotW / Math.max(1, rows.length)) * 0.7;
      const bw = Math.max(3, groupW / Math.max(1, series.length));
      pts.forEach((p) => {
        const y0 = sy(Math.max(0, yMin));
        const top = Math.min(p.y, y0);
        const h = Math.abs(p.y - y0);
        const offset = (si - (series.length - 1) / 2) * bw;
        const rect = el('rect', { x: p.x - bw / 2 + offset, y: top, width: bw, height: Math.max(1, h), class: 'bar' });
        rect.style.fill = color;
        svg.appendChild(rect);
      });
    } else if (type === 'scatter_chart') {
      pts.forEach((p) => {
        const c = el('circle', { cx: p.x, cy: p.y, r: 3.5, class: 'point' });
        c.style.fill = color;
        svg.appendChild(c);
      });
    } else {
      const d = pts.map((p, i) => `${i === 0 ? 'M' : 'L'}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' ');
      if (type === 'area_chart') {
        const gid = `seoul-grad-${gradSeq++}`;
        const grad = el('linearGradient', { id: gid, x1: 0, y1: 0, x2: 0, y2: 1 });
        const stop0 = el('stop', { offset: '0%' });
        stop0.style.stopColor = color; stop0.style.stopOpacity = '0.26';
        const stop1 = el('stop', { offset: '100%' });
        stop1.style.stopColor = color; stop1.style.stopOpacity = '0';
        grad.appendChild(stop0); grad.appendChild(stop1);
        const defs = el('defs'); defs.appendChild(grad); svg.appendChild(defs);
        const y0 = sy(Math.max(0, yMin));
        const area = `${d} L${pts[pts.length - 1].x.toFixed(1)},${y0} L${pts[0].x.toFixed(1)},${y0} Z`;
        svg.appendChild(el('path', { d: area, fill: `url(#${gid})`, stroke: 'none' }));
      }
      const path = el('path', { d, fill: 'none', 'stroke-width': 1.5, class: 'line' });
      path.style.stroke = color;
      svg.appendChild(path);
    }

    const focus = el('circle', { r: 3.5, class: 'focus-dot' });
    focus.style.fill = color;
    focus.style.opacity = '0';
    focusDots.push(focus);
  });

  const crosshair = el('line', { x1: 0, y1: pad.top, x2: 0, y2: pad.top + plotH, class: 'crosshair' });
  crosshair.style.opacity = '0';
  svg.appendChild(crosshair);
  focusDots.forEach((d) => svg.appendChild(d));

  const tip = document.createElement('div');
  tip.className = 'chart-tip';
  tip.style.opacity = '0';

  const showTip = type !== 'bar_chart' && type !== 'scatter_chart';
  const overlay = el('rect', { x: pad.left, y: pad.top, width: plotW, height: plotH, fill: 'transparent' });
  overlay.style.cursor = 'crosshair';
  overlay.addEventListener('mousemove', (ev) => {
    const rect = (svg as unknown as SVGSVGElement).getBoundingClientRect();
    const px = ((ev.clientX - rect.left) / rect.width) * width;
    let best = 0;
    let bestDist = Infinity;
    (allPoints[0] || []).forEach((p, i) => {
      const dist = Math.abs(p.x - px);
      if (dist < bestDist) { bestDist = dist; best = i; }
    });
    const p0 = allPoints[0]?.[best];
    if (!p0) return;
    crosshair.setAttribute('x1', String(p0.x));
    crosshair.setAttribute('x2', String(p0.x));
    crosshair.style.opacity = '1';
    focusDots.forEach((dot, si) => {
      const p = allPoints[si]?.[best];
      if (!p || !showTip) { dot.style.opacity = '0'; return; }
      dot.setAttribute('cx', String(p.x));
      dot.setAttribute('cy', String(p.y));
      dot.style.opacity = '1';
    });
    if (!showTip) { tip.style.opacity = '0'; return; }
    tip.replaceChildren();
    const head = document.createElement('strong');
    head.textContent = fmtX(xKind, p0.rawX);
    tip.appendChild(head);
    series.forEach((s, si) => {
      const val = allPoints[si]?.[best]?.rawY;
      const row = document.createElement('div');
      const sw = document.createElement('span');
      sw.className = 'tip-swatch';
      sw.style.background = SERIES_COLORS[si % SERIES_COLORS.length];
      row.appendChild(sw);
      row.appendChild(document.createTextNode(`${s.label}: ${fmtNumber(Number(val))}${s.unit ? ' ' + s.unit : ''}`));
      tip.appendChild(row);
    });
    tip.style.opacity = '1';
    tip.style.left = `${Math.min((p0.x / width) * 100, 82)}%`;
    tip.style.top = `${Math.max(6, p0.y - 10)}px`;
  });
  overlay.addEventListener('mouseleave', () => {
    crosshair.style.opacity = '0';
    tip.style.opacity = '0';
    focusDots.forEach((d) => (d.style.opacity = '0'));
  });
  svg.appendChild(overlay);

  const legend = document.createElement('div');
  legend.className = 'chart-legend';
  series.forEach((s, si) => {
    const item = document.createElement('span');
    item.className = 'legend-item';
    const sw = document.createElement('span');
    sw.className = 'legend-swatch';
    sw.style.background = SERIES_COLORS[si % SERIES_COLORS.length];
    item.appendChild(sw);
    item.appendChild(document.createTextNode(`${s.label}${s.unit ? ` (${s.unit})` : ''}`));
    legend.appendChild(item);
  });

  const plot = document.createElement('div');
  plot.className = 'chart-plot';
  plot.appendChild(svg);
  plot.appendChild(tip);
  if (series.length > 1) wrap.appendChild(legend);
  wrap.appendChild(plot);
  return wrap;
}

// Renders a network_graph component: nodes evenly on a circle, straight
// edges, labels beside each node. Local layout, no physics, no remote code.
export function renderNetworkGraph(node: ComponentNode, surface: AdaptiveSurface): HTMLElement {
  const wrap = document.createElement('div');
  wrap.className = 'chart network';
  const nodesEntry = node.bindings?.['data'] ? surface.data?.[node.bindings['data']] : undefined;
  const edgesEntry = node.bindings?.['edges'] ? surface.data?.[node.bindings['edges']] : undefined;
  if (!nodesEntry || nodesEntry.kind !== 'table' || !edgesEntry || edgesEntry.kind !== 'table') {
    wrap.textContent = 'Graph data unavailable.';
    return wrap;
  }
  const nodes = tableRowsAsObjects(nodesEntry);
  const edges = tableRowsAsObjects(edgesEntry);
  const idKey = nodesEntry.columns?.[0]?.key ?? 'id';
  const fromKey = edgesEntry.columns?.[0]?.key ?? 'from';
  const toKey = edgesEntry.columns?.[1]?.key ?? 'to';

  const width = 880;
  const height = 360;
  const cx = width / 2;
  const cy = height / 2;
  const radius = Math.min(width, height) / 2 - 70;

  const svg = el('svg', {
    viewBox: `0 0 ${width} ${height}`,
    class: 'chart-svg',
    role: 'img',
    'aria-label': node.accessible_name ?? 'network graph',
  });

  const position = new Map<string, { x: number; y: number }>();
  nodes.forEach((n, i) => {
    const angle = (i / Math.max(1, nodes.length)) * Math.PI * 2 - Math.PI / 2;
    position.set(String(n[idKey]), { x: cx + radius * Math.cos(angle), y: cy + radius * Math.sin(angle) });
  });

  for (const edge of edges) {
    const a = position.get(String(edge[fromKey]));
    const b = position.get(String(edge[toKey]));
    if (!a || !b) continue;
    svg.appendChild(el('line', { x1: a.x, y1: a.y, x2: b.x, y2: b.y, class: 'graph-edge' }));
  }
  nodes.forEach((n, i) => {
    const p = position.get(String(n[idKey]))!;
    const dot = el('circle', { cx: p.x, cy: p.y, r: 6, class: 'graph-node' });
    dot.style.fill = SERIES_COLORS[i % SERIES_COLORS.length];
    svg.appendChild(dot);
    const label = el('text', {
      x: p.x + (p.x >= cx ? 10 : -10),
      y: p.y + 4,
      class: 'axis-label',
      'text-anchor': p.x >= cx ? 'start' : 'end',
    });
    label.textContent = String(n[idKey]);
    svg.appendChild(label);
  });

  wrap.appendChild(svg);
  return wrap;
}
