// Seoul Canvas Design Lab - helpers over canonical semantic results.
// Pure inspection utilities shared by the compiler, renderers, and charts.
// These operate on the canonical wire types only; there is no Design-Lab
// semantic model.

import type { FieldSpec, SemanticResult, SemanticSchema } from './protocol.js';

const MEASURE_ROLES = new Set(['measure', 'money', 'percentage', 'count', 'duration', 'open', 'high', 'low', 'close']);

export function isMeasureRole(role: string | undefined): boolean {
  return role !== undefined && MEASURE_ROLES.has(role);
}

export function isNumericField(field: FieldSpec | undefined): boolean {
  return field?.primitive === 'number' || field?.primitive === 'integer';
}

export function fields(schema: SemanticSchema): FieldSpec[] {
  return schema.fields ?? [];
}

export function fieldByRole(schema: SemanticSchema, role: string): FieldSpec | undefined {
  return fields(schema).find((f) => f.role === role);
}

export function measureFields(schema: SemanticSchema): FieldSpec[] {
  return fields(schema).filter((f) => isMeasureRole(f.role) && isNumericField(f));
}

export function temporalField(schema: SemanticSchema): FieldSpec | undefined {
  return fieldByRole(schema, 'timestamp');
}

/** List-shaped data as rows; scalar/record/graph shapes return []. */
export function asRows(result: SemanticResult): Record<string, unknown>[] {
  return Array.isArray(result.data) ? (result.data as Record<string, unknown>[]) : [];
}

export function asRecord(result: SemanticResult): Record<string, unknown> {
  return result.data && typeof result.data === 'object' && !Array.isArray(result.data)
    ? (result.data as Record<string, unknown>)
    : {};
}

export interface GraphData {
  nodes: Record<string, unknown>[];
  edges: Record<string, unknown>[];
}

export function asGraph(result: SemanticResult): GraphData {
  const record = asRecord(result);
  return {
    nodes: Array.isArray(record['nodes']) ? (record['nodes'] as Record<string, unknown>[]) : [],
    edges: Array.isArray(record['edges']) ? (record['edges'] as Record<string, unknown>[]) : [],
  };
}

export function fmtCell(field: FieldSpec | undefined, value: unknown): string {
  if (value === null || value === undefined || value === '') return '-';
  if (field?.primitive === 'timestamp' && typeof value === 'number') {
    const d = new Date(value);
    return `${d.getUTCFullYear()}-${String(d.getUTCMonth() + 1).padStart(2, '0')}-${String(d.getUTCDate()).padStart(2, '0')} ${String(d.getUTCHours()).padStart(2, '0')}:${String(d.getUTCMinutes()).padStart(2, '0')}`;
  }
  if (isNumericField(field) && typeof value === 'number') {
    return value.toLocaleString(undefined, { maximumFractionDigits: 2 }) + (field?.unit ? ` ${field.unit}` : '');
  }
  return String(value);
}
