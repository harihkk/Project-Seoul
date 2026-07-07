// Seoul Canvas Design Lab - the Lab's adaptive interface compiler.
// Consumes canonical semantic results and emits canonical adaptive-surface
// documents (protocol/adaptive-surface.schema.json). Selection is by shape
// and roles only - no domain conditionals. This is a SEPARATE implementation
// from the native C++ compiler (saui/interface_compiler.cc): the wire types
// are shared, the selection policy is a Lab approximation, and shapes the Lab
// cannot render well fall back to an EXPLAINED table/document/record, never a
// silent one. Component ids are deterministic ("root", "part-<name>") so
// patches can target them stably.

import type {
  AdaptiveSurface,
  ComponentNode,
  ComponentType,
  DataEntry,
  DataProvenance,
  SemanticResult,
  SemanticSchema,
} from './protocol.js';
import { validateSurface, formatErrors } from './protocol.js';
import { validateSurfaceSemantics } from './surface-store.js';
import {
  asGraph,
  asRecord,
  asRows,
  fieldByRole,
  fields,
  measureFields,
  temporalField,
} from './semantics.js';

export interface InterfaceIntent {
  title?: string;
  requestedRepresentation?: ComponentType;
}

export interface CompiledInterface {
  surface: AdaptiveSurface;
  /** Why this representation was chosen; surfaced in the receipt margin. */
  reasons: string[];
  /** The representation the root component uses. */
  representation: ComponentType;
}

const LIST_SHAPES = new Set([
  'entity_collection', 'table', 'cube', 'series', 'time_series',
  'interval_series', 'event_stream', 'status_stream', 'hierarchy',
  'geo_features', 'route', 'document_sections', 'citations', 'media',
  'form_schema', 'action_set', 'code_structure',
]);

function generateSurfaceId(): string {
  if (typeof crypto !== 'undefined' && 'randomUUID' in crypto) return crypto.randomUUID();
  let out = '';
  for (const c of 'xxxxxxxx-xxxx-4xxx-8xxx-xxxxxxxxxxxx') {
    out += c === 'x' ? Math.floor(Math.random() * 16).toString(16) : c;
  }
  return out;
}

function entryProvenance(result: SemanticResult): DataProvenance {
  const p = result.provenance;
  const out: DataProvenance = {
    source_name: p.source_name,
    retrieved_at_ms: p.retrieved_at_ms,
  };
  if (p.source_url !== undefined) out.source_url = p.source_url;
  if (p.effective_at_ms !== undefined) out.effective_at_ms = p.effective_at_ms;
  if (p.timezone !== undefined) out.timezone = p.timezone;
  if (p.units !== undefined) out.units = p.units;
  if (p.freshness !== undefined) out.freshness = p.freshness;
  if (p.completeness !== undefined) out.completeness = p.completeness;
  return out;
}

// Table cells are primitives on the wire (the native parser rejects null
// cells); absent values become the empty string and render as "-".
function primitiveCell(value: unknown): string | number | boolean {
  if (value === null || value === undefined) return '';
  if (typeof value === 'string' || typeof value === 'number' || typeof value === 'boolean') return value;
  return String(value);
}

function rowsToTableEntry(schema: SemanticSchema, rows: Record<string, unknown>[], provenance: DataProvenance): DataEntry {
  return {
    kind: 'table',
    columns: fields(schema).map((f) => ({ key: f.id, label: f.label })),
    rows: rows.map((r) => fields(schema).map((f) => primitiveCell(r[f.id]))),
    provenance,
  };
}

/**
 * Converts a semantic result into named data entries, mirroring the native
 * semantic_to_saui naming: "value" (scalar), "record" (dict shapes), "rows"
 * (list shapes), "nodes"/"edges" (graph), and "series_<field>" per temporal
 * measure. Composite parts are prefixed "<part>_".
 */
export function convertToEntries(result: SemanticResult, prefix = ''): Record<string, DataEntry> {
  const provenance = entryProvenance(result);
  const shape = result.schema.shape;
  const entries: Record<string, DataEntry> = {};
  const name = (n: string) => `${prefix}${n}`;

  if (shape === 'scalar') {
    entries[name('value')] = { kind: 'scalar', value: result.data, provenance };
    return entries;
  }
  if (shape === 'record' || shape === 'document' || shape === 'artifact' || shape === 'diff') {
    const record = asRecord(result);
    const fieldsDict: Record<string, unknown> = {};
    for (const f of fields(result.schema)) {
      if (record[f.id] !== undefined && record[f.id] !== null) fieldsDict[f.id] = primitiveCell(record[f.id]);
    }
    entries[name('record')] = { kind: 'record', fields: fieldsDict, provenance };
    return entries;
  }
  if (shape === 'graph') {
    const graph = asGraph(result);
    entries[name('nodes')] = rowsToTableEntry(result.schema, graph.nodes, provenance);
    entries[name('edges')] = {
      kind: 'table',
      columns: (result.schema.edge_fields ?? []).map((f) => ({ key: f.id, label: f.label })),
      rows: graph.edges.map((r) => (result.schema.edge_fields ?? []).map((f) => primitiveCell(r[f.id]))),
      provenance,
    };
    return entries;
  }
  if (shape === 'composite') {
    const record = asRecord(result);
    (result.schema.parts ?? []).forEach((part, i) => {
      const partName = (result.schema.part_names ?? [])[i] ?? `part${i}`;
      const partResult: SemanticResult = {
        schema: part,
        data: record[partName],
        provenance: result.provenance,
      };
      Object.assign(entries, convertToEntries(partResult, `${partName}_`));
    });
    return entries;
  }
  if (LIST_SHAPES.has(shape)) {
    const rows = asRows(result);
    entries[name('rows')] = rowsToTableEntry(result.schema, rows, provenance);
    const temporal = temporalField(result.schema);
    if (temporal) {
      for (const m of measureFields(result.schema)) {
        const points = rows
          .filter((r) => typeof r[temporal.id] === 'number' && typeof r[m.id] === 'number')
          .map((r) => ({ t_ms: r[temporal.id] as number, y: r[m.id] as number }));
        if (points.length > 0) {
          entries[name(`series_${m.id}`)] = {
            kind: 'series',
            points,
            x_unit: 'ms since epoch',
            y_unit: m.unit ?? '',
            provenance,
          };
        }
      }
    }
    return entries;
  }
  // Defensive: canonical shapes are covered above.
  entries[name('record')] = { kind: 'record', fields: {}, provenance };
  return entries;
}

interface Pick {
  node: ComponentNode;
  reasons: string[];
}

// Chart honesty, mirroring the native compiler: a single point is a number,
// not a trend (ChartWouldMislead), and charts require fully attributed,
// timed data (ChartProvenanceComplete). Falls back to an explained table.
function chartWouldMislead(result: SemanticResult): boolean {
  if (measureFields(result.schema).length === 0) return true;
  return asRows(result).length < 2;
}

function chartProvenanceComplete(result: SemanticResult): boolean {
  const p = result.provenance;
  return Boolean(p.source_name) && p.retrieved_at_ms !== undefined && p.effective_at_ms !== undefined;
}

function chartEligible(result: SemanticResult): boolean {
  return !chartWouldMislead(result) && chartProvenanceComplete(result);
}

function chartNode(id: string, type: ComponentType, result: SemanticResult, prefix: string): ComponentNode {
  const temporal = temporalField(result.schema);
  const measures = measureFields(result.schema);
  // A single-measure temporal chart binds its series entry directly, so an
  // append_series_points patch updates exactly what the chart reads - but
  // only when conversion actually emits that entry (it skips measures with
  // no numeric points); a dangling binding would freeze the surface.
  const rows = asRows(result);
  const seriesEntryExists =
    temporal !== undefined &&
    measures.length === 1 &&
    rows.some((r) => typeof r[temporal.id] === 'number' && typeof r[measures[0].id] === 'number');
  const singleSeries = seriesEntryExists && (type === 'line_chart' || type === 'area_chart');
  return {
    id,
    type,
    props: {
      title: '',
      x_label: temporal?.label ?? 'x',
      y_label: measures[0]?.label ?? 'value',
      units: measures[0]?.unit ?? '',
      x_key: temporal?.id ?? '',
      x_kind: temporal ? 'timestamp' : 'number',
      columns: measures.map((m) => ({ key: m.id, label: m.label, unit: m.unit ?? '' })),
      // Bars are measured from zero (a truncated bar axis misleads); the
      // catalog requires the prop, matching the native compiler.
      ...(type === 'bar_chart' || type === 'stacked_bar_chart' ? { baseline_zero: true } : {}),
    },
    bindings: { data: singleSeries ? `${prefix}series_${measures[0].id}` : `${prefix}rows` },
    accessible_name: `${type.replace(/_/g, ' ')} of ${measures.map((m) => m.label).join(', ')}`,
  };
}

function tableNode(id: string, prefix: string, schema?: SemanticSchema): ComponentNode {
  return {
    id,
    type: 'sortable_table',
    props: schema ? { title: '', columns: displayColumns(schema) } : { title: '' },
    bindings: { data: `${prefix}rows` },
    accessible_name: 'data table',
  };
}

// Presentation metadata carried on props: data entries are generic (no
// roles), so components declare which columns mean what.
function displayColumns(schema: SemanticSchema): { key: string; label: string; unit: string; role: string }[] {
  return fields(schema).map((f) => ({ key: f.id, label: f.label, unit: f.unit ?? '', role: f.role ?? 'none' }));
}

function keyByRole(schema: SemanticSchema, ...roles: string[]): string | undefined {
  for (const role of roles) {
    const f = fieldByRole(schema, role);
    if (f) return f.id;
  }
  return undefined;
}

function nodeForRepresentation(rep: ComponentType, result: SemanticResult, id: string, prefix: string): ComponentNode {
  switch (rep) {
    case 'metric':
      return { id, type: 'metric', props: { label: fields(result.schema)[0]?.label ?? 'Value', unit: fields(result.schema)[0]?.unit ?? '' }, bindings: { data: `${prefix}value` }, accessible_name: 'metric' };
    case 'entity_card':
    case 'key_value_card':
      return { id, type: rep, props: { columns: displayColumns(result.schema) }, bindings: { data: `${prefix}record` }, accessible_name: 'record card' };
    case 'line_chart':
    case 'area_chart':
    case 'bar_chart':
    case 'scatter_chart':
      return chartNode(id, rep, result, prefix);
    case 'comparison_matrix':
      return { id, type: 'comparison_matrix', props: { id_key: keyByRole(result.schema, 'identifier', 'name') ?? fields(result.schema)[0]?.id ?? '', columns: displayColumns(result.schema) }, bindings: { data: `${prefix}rows` }, accessible_name: 'comparison matrix' };
    case 'network_graph':
      return { id, type: 'network_graph', props: { title: '' }, bindings: { data: `${prefix}nodes`, edges: `${prefix}edges` }, accessible_name: 'network graph' };
    default:
      return tableNode(id, prefix, result.schema);
  }
}

function pickForShape(result: SemanticResult, intent: InterfaceIntent, id: string, prefix = ''): Pick {
  const schema = result.schema;
  const shape = schema.shape;
  const reasons: string[] = [];

  if (result.state === 'failed') {
    const message = (result.errors ?? []).map((e) => `${e.code}: ${e.message}`).join('; ') || 'The capability reported a failure.';
    return {
      node: { id, type: 'error_state', props: { text: message }, accessible_name: 'error state' },
      reasons: ['error_result'],
    };
  }
  if (LIST_SHAPES.has(shape) && asRows(result).length === 0) {
    return {
      node: { id, type: 'empty_state', props: { text: 'The capability returned no rows for this request.' }, accessible_name: 'empty state' },
      reasons: ['empty_result'],
    };
  }

  const requested = intent.requestedRepresentation;
  if (requested) {
    if (availableRepresentations(result).includes(requested)) {
      reasons.push('user_requested_representation');
      return { node: nodeForRepresentation(requested, result, id, prefix), reasons };
    }
    reasons.push('user_request_rejected_incompatible');
  }

  switch (shape) {
    case 'scalar':
      reasons.push('shape_scalar_metric');
      return { node: nodeForRepresentation('metric', result, id, prefix), reasons };
    case 'record': {
      const compact = fields(schema).length <= 6;
      reasons.push(compact ? 'record_entity_card' : 'record_key_value');
      return { node: nodeForRepresentation(compact ? 'entity_card' : 'key_value_card', result, id, prefix), reasons };
    }
    case 'series':
    case 'time_series': {
      if (temporalField(schema) && chartEligible(result)) {
        reasons.push('temporal_measure_line_chart');
        return { node: chartNode(id, 'line_chart', result, prefix), reasons };
      }
      reasons.push('chart_would_mislead_fallback');
      return { node: tableNode(id, prefix, schema), reasons };
    }
    case 'entity_collection':
    case 'table':
    case 'cube': {
      const category = fieldByRole(schema, 'category') ?? fieldByRole(schema, 'identifier') ?? fieldByRole(schema, 'dimension');
      if (category && chartEligible(result) && asRows(result).length <= 40) {
        reasons.push('categorical_measure_bar_chart');
        const node = chartNode(id, 'bar_chart', result, prefix);
        node.props = { ...node.props, x_key: category.id, x_label: category.label, x_kind: 'category' };
        return { node, reasons };
      }
      reasons.push('collection_table');
      return { node: tableNode(id, prefix, schema), reasons };
    }
    case 'interval_series':
    case 'event_stream':
    case 'status_stream':
      reasons.push('intervals_timeline');
      return { node: { id, type: 'timeline', props: { when_key: keyByRole(schema, 'interval_start', 'timestamp') ?? '', end_key: keyByRole(schema, 'interval_end') ?? '', what_key: keyByRole(schema, 'name', 'identifier') ?? fields(schema)[0]?.id ?? '', columns: displayColumns(schema) }, bindings: { data: `${prefix}rows` }, accessible_name: 'timeline' }, reasons };
    case 'hierarchy':
    case 'code_structure':
      reasons.push(shape === 'hierarchy' ? 'hierarchy_tree' : 'code_structure_tree');
      return { node: { id, type: 'tree', props: { id_key: keyByRole(schema, 'identifier') ?? fields(schema)[0]?.id ?? '', name_key: keyByRole(schema, 'name', 'identifier') ?? fields(schema)[0]?.id ?? '', parent_key: keyByRole(schema, 'parent_reference') ?? '' }, bindings: { data: `${prefix}rows` }, accessible_name: 'tree' }, reasons };
    case 'graph':
      reasons.push('graph_network');
      return { node: nodeForRepresentation('network_graph', result, id, prefix), reasons };
    case 'geo_features':
    case 'route':
      // The Design Lab has no basemap renderer; an explained coordinate table
      // is the honest representation here (the native Canvas maps this shape
      // to a real map component).
      reasons.push('geo_fallback_table_design_lab');
      return { node: tableNode(id, prefix, schema), reasons };
    case 'document':
    case 'artifact':
      reasons.push('document_view');
      return { node: { id, type: 'document', props: { title_key: keyByRole(schema, 'name') ?? '', body_key: keyByRole(schema, 'body') ?? '', columns: displayColumns(schema) }, bindings: { data: `${prefix}record` }, accessible_name: 'document' }, reasons };
    case 'document_sections':
      reasons.push('document_sections');
      return { node: { id, type: 'document', props: { sectioned: true, title_key: keyByRole(schema, 'name') ?? '', body_key: keyByRole(schema, 'body') ?? '' }, bindings: { data: `${prefix}rows` }, accessible_name: 'document sections' }, reasons };
    case 'citations':
    case 'media': {
      // Native semantics: a source list carries its sources as a bounded
      // structured prop ({href, title} items), not a data binding.
      reasons.push('citations_source_list');
      const urlKey = keyByRole(schema, 'url', 'media_url') ?? '';
      const titleKey = keyByRole(schema, 'name') ?? '';
      const rows = asRows(result);
      const bound = rows.slice(0, 64);
      if (rows.length > bound.length) reasons.push('sources_truncated');
      const sources = bound
        .filter((r) => typeof r[urlKey] === 'string' && /^https?:\/\//.test(r[urlKey] as string))
        .map((r) => ({ href: String(r[urlKey]), title: String(r[titleKey] ?? r[urlKey]) }));
      return { node: { id, type: 'source_list', props: { sources }, accessible_name: 'source list' }, reasons };
    }
    case 'form_schema':
      reasons.push('form_preview_design_lab');
      return { node: { id, type: 'schema_form', props: { inert: true, columns: displayColumns(schema) }, bindings: { data: `${prefix}rows` }, accessible_name: 'form preview (inert)' }, reasons };
    case 'action_set':
      reasons.push('action_set_buttons');
      return { node: { id, type: 'list', props: { as_actions: true, label_key: keyByRole(schema, 'name', 'identifier') ?? fields(schema)[0]?.id ?? '', summary_key: keyByRole(schema, 'description') ?? '' }, bindings: { data: `${prefix}rows` }, accessible_name: 'available actions (synthetic, inert)' }, reasons };
    case 'diff': {
      // Native semantics: the unified diff travels as the component's `diff`
      // prop (code-length budget), not a record binding.
      reasons.push('diff_view');
      const record = asRecord(result);
      const bodyKey = keyByRole(schema, 'body') ?? '';
      const titleKey = keyByRole(schema, 'name') ?? '';
      return {
        node: {
          id,
          type: 'diff_view',
          props: {
            diff: String(record[bodyKey] ?? ''),
            title: String(record[titleKey] ?? ''),
          },
          accessible_name: 'diff view',
        },
        reasons,
      };
    }
    case 'composite': {
      reasons.push('composite_sections');
      const children: ComponentNode[] = (schema.parts ?? []).map((part, i) => {
        const partName = (schema.part_names ?? [])[i] ?? `part${i}`;
        const partResult: SemanticResult = { schema: part, data: asRecord(result)[partName], provenance: result.provenance };
        const child = pickForShape(partResult, {}, `part-${partName}`, `${partName}_`);
        reasons.push(...child.reasons.map((r) => `${partName}:${r}`));
        return child.node;
      });
      return { node: { id, type: 'stack', children }, reasons };
    }
    default:
      reasons.push('fallback_generic_table');
      return { node: tableNode(id, prefix, schema), reasons };
  }
}

/** Representations the Lab can honestly offer for this result. */
export function availableRepresentations(result: SemanticResult): ComponentType[] {
  const schema = result.schema;
  const shape = schema.shape;
  if (result.state === 'failed') return [];
  const reps: ComponentType[] = [];
  const rows = asRows(result);
  const measures = measureFields(schema);
  switch (shape) {
    case 'scalar':
      return ['metric'];
    case 'record':
      return ['entity_card', 'key_value_card'];
    case 'series':
    case 'time_series':
      if (temporalField(schema) && chartEligible(result)) {
        reps.push('line_chart', 'area_chart');
        if (measures.length >= 2) reps.push('scatter_chart');
      }
      reps.push('sortable_table');
      return reps;
    case 'entity_collection':
    case 'table':
    case 'cube': {
      const category = fieldByRole(schema, 'category') ?? fieldByRole(schema, 'identifier') ?? fieldByRole(schema, 'dimension');
      if (category && chartEligible(result) && rows.length <= 40) reps.push('bar_chart');
      reps.push('sortable_table');
      if (rows.length >= 2 && rows.length <= 6 && measures.length > 0) reps.push('comparison_matrix');
      return reps;
    }
    case 'graph':
      return ['network_graph', 'sortable_table'];
    default:
      return [];
  }
}

/**
 * Compiles a semantic result into a canonical adaptive-surface document.
 * Passing `existingSurfaceId` recompiles the SAME surface (used to build
 * replace_component patches); the root component id is always "root" so
 * patches target it stably. The emitted document must validate against
 * adaptive-surface.schema.json; a violation here is a Lab bug and throws.
 */
export function compileInterface(
  result: SemanticResult,
  intent: InterfaceIntent = {},
  existingSurfaceId?: string,
): CompiledInterface {
  const { node, reasons } = pickForShape(result, intent, 'root');
  const surface: AdaptiveSurface = {
    schema_version: 1,
    id: existingSurfaceId ?? generateSurfaceId(),
    kind: 'response',
    title: intent.title ?? 'Result',
    components: [node],
    data: convertToEntries(result),
    provenance: {
      generator: 'deterministic',
      created_at_ms: Date.now(),
      source_urls: result.provenance.source_url ? [result.provenance.source_url] : [],
    },
  };
  const schemaCheck = validateSurface(surface);
  const semanticError = schemaCheck.valid ? validateSurfaceSemantics(surface) : formatErrors(schemaCheck.errors);
  if (semanticError) {
    // A schema-valid semantic result can still be unprojectable into SAUI
    // budgets (for example a field id longer than the SAUI key budget - a
    // known cross-layer gap shared with the native compiler and tracked in
    // the readiness report). Degrade to an explained error artifact; never
    // throw, never render a silently wrong surface.
    const fallback: AdaptiveSurface = {
      schema_version: 1,
      id: surface.id,
      kind: 'response',
      title: intent.title ?? 'Result',
      components: [
        {
          id: 'root',
          type: 'error_state',
          props: { text: `This result cannot be projected into a canonical surface: ${semanticError}` },
          accessible_name: 'unprojectable result',
        },
      ],
      provenance: surface.provenance,
    };
    return { surface: fallback, reasons: ['saui_projection_unrepresentable'], representation: 'error_state' };
  }
  return { surface, reasons, representation: node.type };
}
