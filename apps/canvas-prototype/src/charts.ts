// Project Seoul Canvas prototype - a small, local, trusted SVG chart layer.
// Renders line, area, bar, and scatter with axes, faint gridlines, a legend,
// and a hover crosshair + haloed focus dots + tooltip. SVG only, no remote
// assets, sized responsively, built with safe DOM nodes (no innerHTML). Every
// chart is paired with an accessible table via the surface's representation
// switch, so a chart is never the only path to the data.

import type { Component } from './compiler.js';
import { type FieldSpec, asRows } from './semantic.js';

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
// 0/250/500/750/1000 instead of 184.78/385.93/... - a small but real polish
// signal that separates a considered chart from a default one.
function niceNum(range: number, round: boolean): number {
  if (range <= 0 || !isFinite(range)) return 1;
  const exp = Math.floor(Math.log10(range));
  const frac = range / Math.pow(10, exp);
  let nice: number;
  if (round) nice = frac < 1.5 ? 1 : frac < 3 ? 2 : frac < 7 ? 5 : 10;
  else nice = frac <= 1 ? 1 : frac <= 2 ? 2 : frac <= 5 ? 5 : 10;
  return nice * Math.pow(10, exp);
}

function fmtX(field: FieldSpec | undefined, value: unknown): string {
  if (field?.primitive === 'timestamp' && typeof value === 'number') {
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

// Renders `component` (a chart) into a fresh SVG element with a tooltip layer.
export function renderChart(component: Component): HTMLElement {
  const result = component.result!;
  const rows = asRows(result);
  const xField = component.x;
  const series = component.series || [];

  const wrap = document.createElement('div');
  wrap.className = 'chart';

  const width = 880;
  const height = 320;
  const pad = { top: 14, right: 4, bottom: 38, left: 52 };
  const plotW = width - pad.left - pad.right;
  const plotH = height - pad.top - pad.bottom;

  const svg = el('svg', {
    viewBox: `0 0 ${width} ${height}`,
    class: 'chart-svg',
    role: 'img',
    'aria-label': `${component.type.replace('_', ' ')} of ${series.map((s) => s.label).join(', ')}`,
  });
  svg.setAttribute('preserveAspectRatio', 'xMidYMid meet');

  // X domain: categorical (index) for bar / string x; numeric for timestamps.
  const numericX = xField?.primitive === 'timestamp' || xField?.primitive === 'number';
  const xs = rows.map((r) => (xField ? r[xField.id] : undefined));
  const xNums = numericX ? xs.map((v) => Number(v)) : rows.map((_, i) => i);
  const xMin = Math.min(...xNums);
  const xMax = Math.max(...xNums);

  // Y domain across all series.
  let yMin = Infinity;
  let yMax = -Infinity;
  for (const s of series) {
    for (const r of rows) {
      const v = Number(r[s.id]);
      if (isFinite(v)) {
        yMin = Math.min(yMin, v);
        yMax = Math.max(yMax, v);
      }
    }
  }
  if (!isFinite(yMin)) { yMin = 0; yMax = 1; }
  if (component.type === 'bar_chart' || component.type === 'area_chart') yMin = Math.min(0, yMin);
  if (yMin === yMax) yMax = yMin + 1;
  // Snap the domain to nice round bounds and a nice step.
  const yTickTarget = 5;
  const yStep = niceNum(niceNum(yMax - yMin, false) / (yTickTarget - 1), true);
  yMin = Math.floor(yMin / yStep) * yStep;
  yMax = Math.ceil(yMax / yStep) * yStep;

  // Categorical bars use a band scale (bar i centered in band i) so edge bars
  // do not overhang the plot; continuous data maps min..max across the width.
  const banded = component.type === 'bar_chart' && !numericX;
  const sx = (v: number) =>
    banded
      ? pad.left + ((v + 0.5) / Math.max(1, rows.length)) * plotW
      : pad.left + (xMax === xMin ? plotW / 2 : ((v - xMin) / (xMax - xMin)) * plotW);
  const sy = (v: number) => pad.top + plotH - ((v - yMin) / (yMax - yMin)) * plotH;

  // Horizontal gridlines + y ticks at nice round values (faint; no vertical grid).
  const yCount = Math.round((yMax - yMin) / yStep);
  for (let i = 0; i <= yCount; i++) {
    const v = yMin + i * yStep;
    const y = sy(v);
    svg.appendChild(el('line', { x1: pad.left, y1: y, x2: width - pad.right, y2: y, class: 'grid' }));
    const t = el('text', { x: pad.left - 10, y: y + 3.5, class: 'axis-label', 'text-anchor': 'end' });
    t.textContent = fmtNumber(v);
    svg.appendChild(t);
  }

  // X axis ticks (a handful).
  const xTickCount = Math.min(rows.length, 6);
  for (let i = 0; i < xTickCount; i++) {
    const idx = Math.round((i * (rows.length - 1)) / Math.max(1, xTickCount - 1));
    const xv = numericX ? Number(xs[idx]) : idx;
    const x = sx(xv);
    const anchor = i === 0 ? 'start' : i === xTickCount - 1 ? 'end' : 'middle';
    const t = el('text', { x, y: height - pad.bottom + 20, class: 'axis-label', 'text-anchor': anchor });
    t.textContent = fmtX(xField, xs[idx]);
    svg.appendChild(t);
  }

  // Baseline axis only (left + bottom), hairline.
  svg.appendChild(el('line', { x1: pad.left, y1: pad.top, x2: pad.left, y2: pad.top + plotH, class: 'axis' }));
  svg.appendChild(el('line', { x1: pad.left, y1: pad.top + plotH, x2: width - pad.right, y2: pad.top + plotH, class: 'axis' }));

  const allPoints: Point[][] = [];
  const focusDots: SVGCircleElement[] = [];

  series.forEach((s, si) => {
    const color = SERIES_COLORS[si % SERIES_COLORS.length];
    const pts: Point[] = rows.map((r, i) => {
      const rawY = Number(r[s.id]);
      const xv = numericX ? Number(xs[i]) : i;
      return { x: sx(xv), y: sy(rawY), rawX: xs[i], rawY, label: s.label };
    });
    allPoints.push(pts);

    if (component.type === 'bar_chart') {
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
    } else if (component.type === 'scatter_chart') {
      pts.forEach((p) => {
        const c = el('circle', { cx: p.x, cy: p.y, r: 3.5, class: 'point' });
        c.style.fill = color;
        svg.appendChild(c);
      });
    } else {
      // line / area
      const d = pts.map((p, i) => `${i === 0 ? 'M' : 'L'}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' ');
      if (component.type === 'area_chart') {
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

    // A haloed focus dot per series, revealed on hover.
    const focus = el('circle', { r: 3.5, class: 'focus-dot' });
    focus.style.fill = color;
    focus.style.opacity = '0';
    focusDots.push(focus);
  });

  // Hover crosshair + focus dots + tooltip.
  const crosshair = el('line', { x1: 0, y1: pad.top, x2: 0, y2: pad.top + plotH, class: 'crosshair' });
  crosshair.style.opacity = '0';
  svg.appendChild(crosshair);
  focusDots.forEach((d) => svg.appendChild(d));

  const tip = document.createElement('div');
  tip.className = 'chart-tip';
  tip.style.opacity = '0';

  const showTip = component.type !== 'bar_chart' && component.type !== 'scatter_chart';
  const overlay = el('rect', { x: pad.left, y: pad.top, width: plotW, height: plotH, fill: 'transparent' });
  overlay.style.cursor = 'crosshair';
  overlay.addEventListener('mousemove', (ev) => {
    const rect = (svg as unknown as SVGSVGElement).getBoundingClientRect();
    const px = ((ev.clientX - rect.left) / rect.width) * width;
    // Nearest row by x pixel.
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
    // Rebuild the tooltip with safe DOM nodes (no innerHTML).
    tip.replaceChildren();
    const head = document.createElement('strong');
    head.textContent = fmtX(xField, p0.rawX);
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

  // Legend (safe DOM nodes).
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
