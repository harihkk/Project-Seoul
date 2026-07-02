// Project Seoul Canvas prototype - the adaptive interface compiler.
//
// Input: a validated SemanticResult plus a user InterfaceIntent.
// Output: a validated Surface composed of generic components.
// Every decision is a shape-and-role rule (cardinality, roles, temporal
// structure, requested representation). There are NO task-domain conditionals,
// and a schema never seen falls back to a generic record/table view instead of
// failing. This mirrors native/seoul/browser/saui/interface_compiler.cc.

import {
  type FieldSpec,
  type SemanticResult,
  type SemanticSchema,
  firstFieldByRole,
  measureFields,
  rowCount,
} from './semantic.js';

export type ComponentType =
  | 'metric'
  | 'entity_card'
  | 'key_value'
  | 'table'
  | 'comparison_matrix'
  | 'line_chart'
  | 'area_chart'
  | 'bar_chart'
  | 'scatter_chart'
  | 'range_chart'
  | 'tree'
  | 'network_graph'
  | 'map'
  | 'timeline'
  | 'document_sections'
  | 'source_list'
  | 'empty_state'
  | 'sections';

export interface Component {
  id: string;
  type: ComponentType;
  title?: string;
  // The compiler passes the whole result through; the renderer reads the slice
  // it needs by shape/role. Sub-components (sections) nest here.
  result?: SemanticResult;
  children?: Component[];
  // Charts: which fields to plot (resolved by the compiler from roles).
  x?: FieldSpec;
  series?: FieldSpec[];
  note?: string;
}

export interface Surface {
  id: string;
  title: string;
  root: Component;
  reasons: string[];
  intent: InterfaceIntent;
  result: SemanticResult;
}

export interface InterfaceIntent {
  title?: string;
  // Explicit "show as X"; honored only when compatible with the data.
  requestedRepresentation?: ComponentType;
  hiddenFieldIds?: string[];
  compareEntityIds?: string[];
}

let nextId = 1;
function id(prefix: string): string {
  return `${prefix}-${nextId++}`;
}

// Which representations a shape can honor if the user explicitly requests one.
const REQUEST_COMPATIBLE: Record<string, ComponentType[]> = {
  entity_collection: ['table', 'bar_chart', 'line_chart', 'scatter_chart'],
  table: ['table', 'bar_chart', 'line_chart', 'scatter_chart'],
  time_series: ['line_chart', 'area_chart', 'bar_chart', 'table'],
  record: ['entity_card', 'key_value', 'table'],
};

function chartWouldMislead(schema: SemanticSchema): boolean {
  // A chart needs at least one numeric measure; otherwise it is misleading.
  return measureFields(schema).length === 0;
}

// The core rule set: shape (+ roles) -> component. Returns [component, reasons].
function pickForShape(
  result: SemanticResult,
  intent: InterfaceIntent,
): { component: Component; reasons: string[] } {
  const schema = result.schema;
  const reasons: string[] = [];

  if (result.state === 'empty' || (isCollection(schema) && rowCount(result) === 0)) {
    reasons.push('empty_result');
    return {
      component: { id: id('c'), type: 'empty_state', result, title: intent.title },
      reasons,
    };
  }

  // Honor an explicit representation request when compatible.
  const requested = intent.requestedRepresentation;
  if (requested) {
    const compatible = REQUEST_COMPATIBLE[schema.shape] || [];
    const chartish = requested.endsWith('_chart');
    if (compatible.includes(requested) && !(chartish && chartWouldMislead(schema))) {
      reasons.push('user_requested_representation');
      return { component: buildComponent(requested, result, intent), reasons };
    }
    reasons.push('user_request_rejected_incompatible');
  }

  switch (schema.shape) {
    case 'scalar':
      reasons.push('shape_scalar_metric');
      return { component: buildComponent('metric', result, intent), reasons };

    case 'record': {
      // A record with many fields is a key-value list; few fields, a card.
      const type = schema.fields.length > 6 ? 'key_value' : 'entity_card';
      reasons.push(type === 'entity_card' ? 'record_entity_card' : 'record_key_value');
      return { component: buildComponent(type, result, intent), reasons };
    }

    case 'entity_collection':
    case 'table': {
      // Comparing a small set of entities -> matrix.
      if ((intent.compareEntityIds?.length ?? 0) >= 2) {
        reasons.push('comparable_entities_matrix');
        return {
          component: buildComponent('comparison_matrix', result, intent),
          reasons,
        };
      }
      // A collection with a category/dimension axis and a measure -> bar chart.
      const dim =
        firstFieldByRole(schema, 'category') || firstFieldByRole(schema, 'dimension');
      const measures = measureFields(schema);
      if (dim && measures.length >= 1 && rowCount(result) <= 40) {
        reasons.push('collection_bar_chart');
        const c = buildComponent('bar_chart', result, intent);
        c.x = dim;
        c.series = measures.slice(0, 1);
        return { component: c, reasons };
      }
      reasons.push('collection_table');
      return { component: buildComponent('table', result, intent), reasons };
    }

    case 'time_series': {
      if (chartWouldMislead(schema)) {
        reasons.push('chart_would_mislead_fallback_table');
        return { component: buildComponent('table', result, intent), reasons };
      }
      reasons.push('temporal_measure_line_chart');
      const c = buildComponent('line_chart', result, intent);
      c.x = firstFieldByRole(schema, 'timestamp');
      c.series = measureFields(schema);
      return { component: c, reasons };
    }

    case 'interval_series':
      reasons.push('intervals_timeline');
      return { component: buildComponent('timeline', result, intent), reasons };

    case 'event_stream':
      reasons.push('events_timeline');
      return { component: buildComponent('timeline', result, intent), reasons };

    case 'hierarchy':
      reasons.push('hierarchy_tree');
      return { component: buildComponent('tree', result, intent), reasons };

    case 'graph':
      reasons.push('graph_network');
      return { component: buildComponent('network_graph', result, intent), reasons };

    case 'geo_features':
      reasons.push('geo_map');
      return { component: buildComponent('map', result, intent), reasons };

    case 'document_sections':
      reasons.push('document_sections');
      return { component: buildComponent('document_sections', result, intent), reasons };

    case 'citations':
      reasons.push('citations_source_list');
      return { component: buildComponent('source_list', result, intent), reasons };

    case 'composite': {
      reasons.push('composite_sections');
      const children = (schema.parts || []).map((part) => {
        const sub = pickForShape(part.result, {});
        sub.component.title = part.name;
        return sub.component;
      });
      return {
        component: { id: id('c'), type: 'sections', children, title: intent.title },
        reasons,
      };
    }

    default:
      reasons.push('fallback_generic_table');
      return { component: buildComponent('table', result, intent), reasons };
  }
}

function isCollection(schema: SemanticSchema): boolean {
  return (
    schema.shape === 'entity_collection' ||
    schema.shape === 'table' ||
    schema.shape === 'time_series' ||
    schema.shape === 'interval_series' ||
    schema.shape === 'event_stream'
  );
}

function buildComponent(
  type: ComponentType,
  result: SemanticResult,
  intent: InterfaceIntent,
): Component {
  const c: Component = { id: id('c'), type, result, title: intent.title };
  if (type === 'line_chart' || type === 'area_chart') {
    c.x =
      firstFieldByRole(result.schema, 'timestamp') ||
      firstFieldByRole(result.schema, 'dimension');
    c.series = measureFields(result.schema);
  } else if (type === 'bar_chart') {
    c.x =
      firstFieldByRole(result.schema, 'category') ||
      firstFieldByRole(result.schema, 'dimension') ||
      firstFieldByRole(result.schema, 'identifier');
    c.series = measureFields(result.schema).slice(0, 1);
  } else if (type === 'scatter_chart') {
    const m = measureFields(result.schema);
    c.x = m[0];
    c.series = m.slice(1, 2);
  }
  return c;
}

// The public entry point. Keeps `existingSurfaceId` so a follow-up re-compiles
// the SAME surface in place (patch, not duplicate).
export function compileInterface(
  result: SemanticResult,
  intent: InterfaceIntent,
  existingSurfaceId?: string,
): Surface {
  const { component, reasons } = pickForShape(result, intent);
  return {
    id: existingSurfaceId || id('surface'),
    title: intent.title || 'Result',
    root: component,
    reasons,
    intent,
    result,
  };
}

// The representations a user may switch a given result to (drives the UI
// switcher). Purely shape/role-derived - no domain list.
export function availableRepresentations(result: SemanticResult): ComponentType[] {
  const out: ComponentType[] = [];
  const schema = result.schema;
  const hasMeasure = measureFields(schema).length > 0;
  const hasTime = !!firstFieldByRole(schema, 'timestamp');
  const hasDim =
    !!firstFieldByRole(schema, 'category') ||
    !!firstFieldByRole(schema, 'dimension');
  if (isCollection(schema)) {
    out.push('table');
    if (hasMeasure && (hasDim || hasTime)) out.push('bar_chart');
    if (hasMeasure && hasTime) out.push('line_chart', 'area_chart');
    if (measureFields(schema).length >= 2) out.push('scatter_chart');
  } else if (schema.shape === 'record') {
    out.push('entity_card', 'key_value');
  } else if (schema.shape === 'scalar') {
    out.push('metric');
  }
  return Array.from(new Set(out));
}
