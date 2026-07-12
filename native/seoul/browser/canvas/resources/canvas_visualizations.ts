// Local, deterministic SVG visualizations for the complete SAUI chart/map
// catalog. Payload text is interpolated by Lit and never interpreted as markup.

import {html, nothing, svg} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComponentNode, DataEntry} from './canvas_types.js';
import {propString} from './canvas_types.js';

const WIDTH = 640;
const HEIGHT = 260;
const LEFT = 48;
const RIGHT = 18;
const TOP = 18;
const BOTTOM = 38;
const PLOT_WIDTH = WIDTH - LEFT - RIGHT;
const PLOT_HEIGHT = HEIGHT - TOP - BOTTOM;

interface Datum {
  label: string;
  values: number[];
}

function finiteNumber(value: unknown): number|undefined {
  const number = typeof value === 'number' ? value : Number(value);
  return Number.isFinite(number) ? number : undefined;
}

function dataRows(entry: DataEntry): Datum[] {
  if (entry.kind === 'series') {
    return (entry.points ?? []).flatMap((point, index) => {
      const y = finiteNumber(point.y);
      if (y === undefined) {
        return [];
      }
      const x = point.t_ms ?? point.x ?? index;
      const label = point.t_ms !== undefined ?
          new Date(point.t_ms).toLocaleTimeString([], {
            hour: '2-digit', minute: '2-digit',
          }) : String(x);
      return [{label, values: [y]}];
    });
  }
  if (entry.kind !== 'table') {
    return [];
  }
  return (entry.rows ?? []).flatMap((row, index) => {
    const values = row.map(finiteNumber).filter(
        (value): value is number => value !== undefined);
    if (!values.length) {
      return [];
    }
    const labelValue = row.find(value => finiteNumber(value) === undefined);
    return [{label: String(labelValue ?? index + 1), values}];
  });
}

function extent(values: number[], includeZero = false): [number, number] {
  let low = Math.min(...values);
  let high = Math.max(...values);
  if (includeZero) {
    low = Math.min(0, low);
    high = Math.max(0, high);
  }
  if (!Number.isFinite(low) || !Number.isFinite(high)) {
    return [0, 1];
  }
  if (low === high) {
    const pad = Math.abs(low) * .1 || 1;
    return [low - pad, high + pad];
  }
  return [low, high];
}

function xAt(index: number, count: number): number {
  return LEFT + (count <= 1 ? PLOT_WIDTH / 2 : index * PLOT_WIDTH / (count - 1));
}

function yAt(value: number, domain: [number, number]): number {
  return TOP + (domain[1] - value) / (domain[1] - domain[0]) * PLOT_HEIGHT;
}

function pointsAttribute(rows: Datum[], domain: [number, number]): string {
  return rows.map((row, index) =>
    `${xAt(index, rows.length)},${yAt(row.values[0]!, domain)}`).join(' ');
}

function axes(node: ComponentNode, domain: [number, number]) {
  const xLabel = propString(node.props, 'x_label');
  const yLabel = propString(node.props, 'y_label');
  return svg`
    <g class="viz-axes" aria-hidden="true">
      <line x1="${LEFT}" y1="${TOP + PLOT_HEIGHT}"
          x2="${LEFT + PLOT_WIDTH}" y2="${TOP + PLOT_HEIGHT}"></line>
      <line x1="${LEFT}" y1="${TOP}" x2="${LEFT}"
          y2="${TOP + PLOT_HEIGHT}"></line>
      <text x="${LEFT - 8}" y="${TOP + 5}" text-anchor="end">
        ${domain[1].toLocaleString()}
      </text>
      <text x="${LEFT - 8}" y="${TOP + PLOT_HEIGHT}" text-anchor="end">
        ${domain[0].toLocaleString()}
      </text>
      <text x="${LEFT + PLOT_WIDTH / 2}" y="${HEIGHT - 5}"
          text-anchor="middle">${xLabel}</text>
      <text class="viz-y-label" x="12" y="${TOP + PLOT_HEIGHT / 2}"
          text-anchor="middle">${yLabel}</text>
    </g>`;
}

function lineVisual(node: ComponentNode, rows: Datum[], area = false) {
  const domain = extent(rows.map(row => row.values[0]!));
  const points = pointsAttribute(rows, domain);
  const areaPoints = `${LEFT},${TOP + PLOT_HEIGHT} ${points} ` +
      `${LEFT + PLOT_WIDTH},${TOP + PLOT_HEIGHT}`;
  return svg`
    ${axes(node, domain)}
    ${area ? svg`<polygon class="viz-area" points="${areaPoints}"></polygon>` : nothing}
    <polyline class="viz-line" points="${points}"></polyline>
    ${rows.map((row, index) => svg`
      <circle class="viz-point" cx="${xAt(index, rows.length)}"
          cy="${yAt(row.values[0]!, domain)}" r="3">
        <title>${row.label}: ${row.values[0]!.toLocaleString()}</title>
      </circle>`)}
  `;
}

function scatterVisual(node: ComponentNode, rows: Datum[]) {
  const xValues = rows.map((row, index) => row.values[1] ?? index);
  const yValues = rows.map(row => row.values[0]!);
  const xDomain = extent(xValues);
  const yDomain = extent(yValues);
  return svg`
    ${axes(node, yDomain)}
    ${rows.map((row, index) => {
      const x = LEFT + (xValues[index]! - xDomain[0]) /
          (xDomain[1] - xDomain[0]) * PLOT_WIDTH;
      return svg`<circle class="viz-point" cx="${x}"
          cy="${yAt(yValues[index]!, yDomain)}" r="5">
        <title>${row.label}: ${xValues[index]}, ${yValues[index]}</title>
      </circle>`;
    })}`;
}

function barVisual(node: ComponentNode, rows: Datum[], stacked: boolean) {
  const totals = rows.map(row => stacked ?
      row.values.reduce((sum, value) => sum + value, 0) :
      Math.max(...row.values));
  const domain = extent(totals.concat(0), true);
  const groupWidth = PLOT_WIDTH / Math.max(rows.length, 1);
  return svg`
    ${axes(node, domain)}
    ${rows.map((row, rowIndex) => {
      const values = row.values;
      let cumulative = 0;
      return values.map((value, seriesIndex) => {
        const displayValue = stacked ? cumulative + value : value;
        const startValue = stacked ? cumulative : 0;
        cumulative += value;
        const x = LEFT + rowIndex * groupWidth +
            (stacked ? groupWidth * .18 : seriesIndex * groupWidth * .7 / values.length + groupWidth * .15);
        const width = stacked ? groupWidth * .64 : groupWidth * .7 / values.length;
        const y = yAt(Math.max(startValue, displayValue), domain);
        const height = Math.max(1, Math.abs(yAt(startValue, domain) - yAt(displayValue, domain)));
        return svg`<rect class="viz-series-${seriesIndex % 6}" x="${x}"
            y="${y}" width="${width}" height="${height}" rx="3">
          <title>${row.label}: ${value.toLocaleString()}</title>
        </rect>`;
      });
    })}`;
}

function pieVisual(rows: Datum[]) {
  const values = rows.map(row => Math.max(0, row.values[0]!));
  const total = values.reduce((sum, value) => sum + value, 0) || 1;
  let offset = 0;
  return svg`
    <circle class="viz-pie-track" cx="320" cy="130" r="88"></circle>
    ${values.map((value, index) => {
      const length = value / total * 100;
      const segment = svg`<circle class="viz-pie viz-series-${index % 6}"
          cx="320" cy="130" r="88" pathLength="100"
          stroke-dasharray="${length} ${100 - length}"
          stroke-dashoffset="${-offset}">
        <title>${rows[index]!.label}: ${value.toLocaleString()}</title>
      </circle>`;
      offset += length;
      return segment;
    })}
    <text class="viz-pie-total" x="320" y="136" text-anchor="middle">
      ${total.toLocaleString()}
    </text>`;
}

function candleVisual(node: ComponentNode, rows: Datum[]) {
  const allValues = rows.flatMap(row => row.values.slice(0, 4));
  const domain = extent(allValues);
  const width = PLOT_WIDTH / Math.max(rows.length, 1) * .46;
  return svg`
    ${axes(node, domain)}
    ${rows.map((row, index) => {
      const [open = 0, high = open, low = open, close = open] = row.values;
      const x = xAt(index, rows.length);
      const yOpen = yAt(open, domain);
      const yClose = yAt(close, domain);
      return svg`
        <line class="viz-candle-wick" x1="${x}" y1="${yAt(high, domain)}"
            x2="${x}" y2="${yAt(low, domain)}"></line>
        <rect class="${close >= open ? 'viz-candle-up' : 'viz-candle-down'}"
            x="${x - width / 2}" y="${Math.min(yOpen, yClose)}"
            width="${width}" height="${Math.max(2, Math.abs(yClose - yOpen))}">
          <title>${row.label}: O ${open}, H ${high}, L ${low}, C ${close}</title>
        </rect>`;
    })}`;
}

function heatVisual(rows: Datum[]) {
  const values = rows.flatMap(row => row.values);
  const domain = extent(values);
  const columns = Math.max(...rows.map(row => row.values.length), 1);
  const cellWidth = PLOT_WIDTH / columns;
  const cellHeight = PLOT_HEIGHT / Math.max(rows.length, 1);
  return svg`${rows.flatMap((row, rowIndex) => row.values.map((value, columnIndex) => {
    const intensity = (value - domain[0]) / (domain[1] - domain[0]);
    return svg`<rect class="viz-heat" x="${LEFT + columnIndex * cellWidth}"
        y="${TOP + rowIndex * cellHeight}" width="${Math.max(1, cellWidth - 2)}"
        height="${Math.max(1, cellHeight - 2)}" rx="3"
        style="--viz-intensity:${intensity}">
      <title>${row.label}: ${value.toLocaleString()}</title>
    </rect>`;
  }))}`;
}

function networkVisual(entry: DataEntry) {
  const rawRows = entry.kind === 'table' ? entry.rows ?? [] : [];
  const names = [...new Set(rawRows.flatMap(row => row.slice(0, 2).map(String)))].slice(0, 32);
  const position = new Map(names.map((name, index) => {
    const angle = index / Math.max(names.length, 1) * Math.PI * 2 - Math.PI / 2;
    return [name, {x: 320 + Math.cos(angle) * 92, y: 130 + Math.sin(angle) * 92}];
  }));
  return svg`
    ${rawRows.slice(0, 64).map(row => {
      const from = position.get(String(row[0]));
      const to = position.get(String(row[1]));
      return from && to ? svg`<line class="viz-edge" x1="${from.x}" y1="${from.y}"
          x2="${to.x}" y2="${to.y}"></line>` : nothing;
    })}
    ${names.map(name => {
      const point = position.get(name)!;
      return svg`<g><circle class="viz-node" cx="${point.x}" cy="${point.y}" r="10">
          <title>${name}</title></circle>
        <text class="viz-node-label" x="${point.x}" y="${point.y + 24}"
            text-anchor="middle">${name.slice(0, 16)}</text></g>`;
    })}`;
}

function coordinateVisual(entry: DataEntry) {
  const rows = entry.kind === 'table' ? entry.rows ?? [] : [];
  const columns = entry.columns ?? [];
  const keys = columns.map(column => `${column.key} ${column.label}`.toLowerCase());
  const latIndex = keys.findIndex(key => /\b(lat|latitude)\b/.test(key));
  const lonIndex = keys.findIndex(key => /\b(lon|lng|longitude)\b/.test(key));
  const points = rows.flatMap((row, index) => {
    const numeric = row.map(finiteNumber).filter((value): value is number => value !== undefined);
    const lat = finiteNumber(row[latIndex]) ?? numeric[1];
    const lon = finiteNumber(row[lonIndex]) ?? numeric[0];
    return lat === undefined || lon === undefined ? [] : [{lat, lon, label: String(row[0] ?? index + 1)}];
  });
  return svg`
    <rect class="viz-map-field" x="${LEFT}" y="${TOP}"
        width="${PLOT_WIDTH}" height="${PLOT_HEIGHT}" rx="12"></rect>
    ${[-60, -30, 0, 30, 60].map(lat => svg`<line class="viz-graticule"
        x1="${LEFT}" y1="${TOP + (90 - lat) / 180 * PLOT_HEIGHT}"
        x2="${LEFT + PLOT_WIDTH}" y2="${TOP + (90 - lat) / 180 * PLOT_HEIGHT}"></line>`)}
    ${[-120, -60, 0, 60, 120].map(lon => svg`<line class="viz-graticule"
        x1="${LEFT + (lon + 180) / 360 * PLOT_WIDTH}" y1="${TOP}"
        x2="${LEFT + (lon + 180) / 360 * PLOT_WIDTH}" y2="${TOP + PLOT_HEIGHT}"></line>`)}
    ${points.map(point => svg`<circle class="viz-map-point"
        cx="${LEFT + (Math.max(-180, Math.min(180, point.lon)) + 180) / 360 * PLOT_WIDTH}"
        cy="${TOP + (90 - Math.max(-90, Math.min(90, point.lat))) / 180 * PLOT_HEIGHT}"
        r="6"><title>${point.label}: ${point.lat}, ${point.lon}</title></circle>`)}`;
}

export function renderDataTable(entry: DataEntry) {
  if (entry.kind === 'series') {
    return html`<table class="saui-table">
      <thead><tr><th scope="col">Position</th><th scope="col">Value</th></tr></thead>
      <tbody>${(entry.points ?? []).map((point, index) => html`<tr>
        <td>${point.t_ms !== undefined ? new Date(point.t_ms).toLocaleString() : point.x ?? index}</td>
        <td>${point.y}</td>
      </tr>`)}</tbody>
    </table>`;
  }
  return html`<table class="saui-table">
    <thead><tr>${(entry.columns ?? []).map(column => html`<th scope="col">${column.label}</th>`)}</tr></thead>
    <tbody>${(entry.rows ?? []).map(row => html`<tr>${row.map(cell => html`<td>${cell == null ? '' : String(cell)}</td>`)}</tr>`)}</tbody>
  </table>`;
}

export function renderVisualization(node: ComponentNode, entry: DataEntry) {
  const rows = dataRows(entry);
  const type = node.type;
  let graphic;
  if (type === 'line_chart' || type === 'sparkline' || type === 'range_chart') {
    graphic = lineVisual(node, rows);
  } else if (type === 'area_chart') {
    graphic = lineVisual(node, rows, true);
  } else if (type === 'scatter_chart') {
    graphic = scatterVisual(node, rows);
  } else if (type === 'bar_chart' || type === 'histogram') {
    graphic = barVisual(node, rows, false);
  } else if (type === 'stacked_bar_chart') {
    graphic = barVisual(node, rows, true);
  } else if (type === 'pie_chart') {
    graphic = pieVisual(rows);
  } else if (type === 'candlestick_chart') {
    graphic = candleVisual(node, rows);
  } else if (type === 'heat_map') {
    graphic = heatVisual(rows);
  } else if (type === 'network_graph') {
    graphic = networkVisual(entry);
  } else {
    graphic = coordinateVisual(entry);
  }

  const title = propString(node.props, 'title') || node.accessible_name || 'Visualization';
  return html`<figure class="saui-chart">
    <figcaption><span>${title}</span><span class="viz-units">${propString(node.props, 'units')}</span></figcaption>
    ${rows.length || type === 'network_graph' || type === 'map' || type === 'geo_layer' ?
      svg`<svg class="viz" viewBox="0 0 ${WIDTH} ${HEIGHT}" role="img" aria-label="${node.accessible_name || title}">${graphic}</svg>` :
      html`<div class="saui-empty">No plottable values.</div>`}
    <details class="viz-data"><summary>View source data</summary>${renderDataTable(entry)}</details>
  </figure>`;
}
