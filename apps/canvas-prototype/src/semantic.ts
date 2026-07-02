// Project Seoul Canvas prototype - the domain-neutral semantic model.
//
// This mirrors the native semantic data fabric (native/seoul/browser/semantic):
// a capability returns a SemanticResult described by a SHAPE and per-field
// ROLES plus provenance. Nothing here names a domain; the adaptive interface
// compiler decides how to present a result purely from its shape and roles, so
// a schema it has never seen still renders.

export type SemanticShape =
  | 'scalar'
  | 'record'
  | 'entity_collection'
  | 'table'
  | 'time_series'
  | 'interval_series'
  | 'event_stream'
  | 'hierarchy'
  | 'graph'
  | 'geo_features'
  | 'document_sections'
  | 'citations'
  | 'composite';

export type FieldPrimitive =
  | 'string'
  | 'integer'
  | 'number'
  | 'boolean'
  | 'timestamp'; // milliseconds since the Unix epoch

export type SemanticRole =
  | 'none'
  | 'identifier'
  | 'name'
  | 'description'
  | 'measure'
  | 'dimension'
  | 'category'
  | 'status'
  | 'timestamp'
  | 'interval_start'
  | 'interval_end'
  | 'money'
  | 'percentage'
  | 'count'
  | 'latitude'
  | 'longitude'
  | 'url'
  | 'parent_reference'
  | 'source_node'
  | 'target_node'
  | 'body'
  | 'citation_ref';

export interface FieldSpec {
  id: string;
  label: string;
  primitive: FieldPrimitive;
  role: SemanticRole;
  unit?: string;
  currency?: string;
  nullable?: boolean;
}

export interface Provenance {
  source: string; // capability/provider identity; never empty
  sourceUrl?: string;
  retrievedAt: number; // ms epoch
  freshness: 'real_time' | 'near_real_time' | 'cached' | 'static';
  completeness?: number; // [0,1]
}

export interface SemanticSchema {
  shape: SemanticShape;
  fields: FieldSpec[];
  // composite: named sub-results.
  parts?: { name: string; result: SemanticResult }[];
}

export interface SemanticResult {
  schema: SemanticSchema;
  // Row-shaped data for collection/series/etc.; a single object for record;
  // a primitive for scalar. Kept as unknown so the compiler reads it by shape.
  data: unknown;
  provenance: Provenance;
  state?: 'complete' | 'partial' | 'empty' | 'error';
  note?: string;
}

// --- Small helpers used by the compiler and renderer ------------------------

export function fieldsByRole(
  schema: SemanticSchema,
  role: SemanticRole,
): FieldSpec[] {
  return schema.fields.filter((f) => f.role === role);
}

export function firstFieldByRole(
  schema: SemanticSchema,
  role: SemanticRole,
): FieldSpec | undefined {
  return schema.fields.find((f) => f.role === role);
}

export function measureFields(schema: SemanticSchema): FieldSpec[] {
  return schema.fields.filter(
    (f) =>
      (f.role === 'measure' ||
        f.role === 'money' ||
        f.role === 'percentage' ||
        f.role === 'count') &&
      (f.primitive === 'number' || f.primitive === 'integer'),
  );
}

export function asRows(result: SemanticResult): Record<string, unknown>[] {
  return Array.isArray(result.data)
    ? (result.data as Record<string, unknown>[])
    : [];
}

export function rowCount(result: SemanticResult): number {
  return Array.isArray(result.data) ? (result.data as unknown[]).length : 0;
}
