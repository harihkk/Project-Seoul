// Seoul Canvas Design Lab - the voice summarizer.
// The spoken twin of the interface compiler: it reads the SAME canonical
// semantic result and composes the one-to-three-sentence insight Seoul says
// back, from shapes and roles alone. No domain conditionals - a schema it has
// never seen still gets an honest sentence. The visible insight line and the
// spoken audio are the same string, so eye and ear never disagree. This is
// the conformance reference for the future native port.

import type { FieldSpec, SemanticResult } from './protocol.js';
import { asGraph, asRecord, asRows, fieldByRole, fields, measureFields, temporalField } from './semantics.js';

function spokenNumber(value: number, unit?: string): string {
  const rounded = Math.abs(value) >= 100 ? Math.round(value) : Math.round(value * 100) / 100;
  return `${rounded.toLocaleString(undefined, { maximumFractionDigits: 2 })}${unit ? ` ${unit}` : ''}`;
}

function spokenTime(ms: number): string {
  const d = new Date(ms);
  return `${String(d.getUTCHours()).padStart(2, '0')}:${String(d.getUTCMinutes()).padStart(2, '0')} UTC`;
}

function nameOf(row: Record<string, unknown>, schema: { fields?: FieldSpec[] }): string {
  const field = (schema.fields ?? []).find((f) => f.role === 'name') ?? (schema.fields ?? []).find((f) => f.role === 'identifier');
  return field ? String(row[field.id] ?? '') : '';
}

function trendSentence(result: SemanticResult): string {
  const temporal = temporalField(result.schema);
  const measure = measureFields(result.schema)[0];
  const rows = asRows(result).filter((r) => typeof r[measure?.id ?? ''] === 'number');
  if (!temporal || !measure || rows.length < 2) return '';
  const values = rows.map((r) => r[measure.id] as number);
  const mean = values.reduce((a, b) => a + b, 0) / values.length;
  const first = values[0];
  const last = values[values.length - 1];
  const change = first === 0 ? 0 : ((last - first) / Math.abs(first)) * 100;
  const direction = Math.abs(change) < 2 ? 'holding steady' : change > 0 ? `up ${Math.round(Math.abs(change))} percent` : `down ${Math.round(Math.abs(change))} percent`;
  const peakIndex = values.indexOf(Math.max(...values));
  const peakAt = rows[peakIndex]?.[temporal.id];
  const peak = typeof peakAt === 'number' ? `, peaking at ${spokenNumber(Math.max(...values), measure.unit)} around ${spokenTime(peakAt)}` : '';
  return `${measure.label} averaged ${spokenNumber(mean, measure.unit)} across ${values.length} samples, ${direction}${peak}.`;
}

function extremesSentence(result: SemanticResult): string {
  const measure = measureFields(result.schema)[0];
  const rows = asRows(result).filter((r) => typeof r[measure?.id ?? ''] === 'number');
  if (!measure || rows.length < 2) return '';
  const sorted = [...rows].sort((a, b) => (b[measure.id] as number) - (a[measure.id] as number));
  const top = sorted[0];
  const bottom = sorted[sorted.length - 1];
  const topName = nameOf(top, result.schema);
  const bottomName = nameOf(bottom, result.schema);
  const highest = `highest ${measure.label} is ${spokenNumber(top[measure.id] as number, measure.unit)}${topName ? ` for ${topName}` : ''}`;
  const lowest = `lowest is ${spokenNumber(bottom[measure.id] as number, measure.unit)}${bottomName ? ` for ${bottomName}` : ''}`;
  return `${rows.length} entries - ${highest}; ${lowest}.`;
}

function shapeSentence(result: SemanticResult): string {
  const schema = result.schema;
  const rows = asRows(result);
  switch (schema.shape) {
    case 'scalar': {
      const field = fields(schema)[0];
      return `${field?.label ?? 'The value'} is ${typeof result.data === 'number' ? spokenNumber(result.data, field?.unit) : String(result.data)}.`;
    }
    case 'record': {
      const record = asRecord(result);
      const name = nameOf(record, schema);
      const status = fieldByRole(schema, 'status');
      const statusPart = status && record[status.id] !== undefined ? `, ${status.label.toLowerCase()} ${String(record[status.id])}` : '';
      const measure = measureFields(schema)[0];
      const measurePart = measure && typeof record[measure.id] === 'number' ? `, ${measure.label.toLowerCase()} ${spokenNumber(record[measure.id] as number, measure.unit)}` : '';
      return `${name || 'One record'}${statusPart}${measurePart}.`;
    }
    case 'series':
    case 'time_series':
      return trendSentence(result) || `${rows.length} samples returned.`;
    case 'entity_collection':
    case 'table':
    case 'cube':
      return extremesSentence(result) || `${rows.length} entries returned.`;
    case 'interval_series':
    case 'event_stream':
    case 'status_stream': {
      const start = fieldByRole(schema, 'interval_start') ?? fieldByRole(schema, 'timestamp');
      const times = rows.map((r) => r[start?.id ?? '']).filter((v): v is number => typeof v === 'number');
      const span = times.length > 1 ? ` between ${spokenTime(Math.min(...times))} and ${spokenTime(Math.max(...times))}` : '';
      return `${rows.length} ${schema.shape === 'interval_series' ? 'windows' : 'events'}${span}.`;
    }
    case 'hierarchy':
    case 'code_structure': {
      const parent = fieldByRole(schema, 'parent_reference');
      const roots = rows.filter((r) => !r[parent?.id ?? '']).length;
      return `${rows.length} nodes under ${roots} top-level ${roots === 1 ? 'root' : 'roots'}.`;
    }
    case 'graph': {
      const graph = asGraph(result);
      return `${graph.nodes.length} nodes with ${graph.edges.length} connections.`;
    }
    case 'geo_features':
    case 'route':
      return `${rows.length} locations returned.`;
    case 'document':
    case 'artifact': {
      const record = asRecord(result);
      const body = String(record[fieldByRole(schema, 'body')?.id ?? ''] ?? '');
      const firstSentence = body.split(/(?<=[.!?])\s/)[0] ?? '';
      return `${nameOf(record, schema) || 'Document'}: ${firstSentence}`.slice(0, 220);
    }
    case 'document_sections':
      return `${rows.length} sections returned.`;
    case 'citations':
    case 'media':
      return `${rows.length} sources${rows.length ? `; first is ${nameOf(rows[0], schema) || 'untitled'}` : ''}.`;
    case 'form_schema': {
      const required = rows.filter((r) => r['required'] === true).length;
      return `This needs ${rows.length} inputs, ${required} required.`;
    }
    case 'action_set': {
      const labels = rows.map((r) => nameOf(r, schema)).filter(Boolean).slice(0, 3);
      return `${rows.length} actions available${labels.length ? `: ${labels.join(', ')}` : ''}.`;
    }
    case 'diff': {
      const body = String(asRecord(result)[fieldByRole(schema, 'body')?.id ?? ''] ?? '');
      const added = body.split('\n').filter((l) => l.startsWith('+') && !l.startsWith('+++')).length;
      const removed = body.split('\n').filter((l) => l.startsWith('-') && !l.startsWith('---')).length;
      return `${nameOf(asRecord(result), schema) || 'The change'}: ${added} lines added, ${removed} removed.`;
    }
    case 'composite': {
      const record = asRecord(result);
      const parts = (schema.parts ?? []).slice(0, 2).map((part, i) => {
        const partName = (schema.part_names ?? [])[i] ?? `part ${i + 1}`;
        const partResult: SemanticResult = { schema: part, data: record[partName], provenance: result.provenance };
        return shapeSentence(partResult).replace(/\.$/, '');
      });
      return `${parts.join('. ')}.`;
    }
    default:
      return `${rows.length || 'A'} result returned.`;
  }
}

/**
 * The spoken (and visibly rendered) insight for one result. Honesty riders
 * always speak: failures speak the error, partial results say so, conflicts
 * and reported gaps are never glossed over.
 */
export function summarizeResult(result: SemanticResult): string {
  if (result.state === 'failed') {
    const error = (result.errors ?? [])[0];
    return `That did not work: ${error ? error.message : 'the capability reported a failure.'}`;
  }
  const sentences: string[] = [shapeSentence(result)];
  if (result.state === 'partial') {
    sentences.push('This is a partial result; more is available.');
  }
  if (result.state === 'streaming') {
    sentences.push('Live - it updates as new samples arrive.');
  }
  const conflict = (result.conflicts ?? [])[0];
  if (conflict) {
    const field = fields(result.schema).find((f) => f.id === conflict.field_id);
    sentences.push(`Note: sources disagree on ${field?.label ?? conflict.field_id}.`);
  }
  const gap = (result.unavailable_field_ids ?? [])[0];
  if (gap) {
    const field = fields(result.schema).find((f) => f.id === gap);
    sentences.push(`${field?.label ?? gap} was not supplied by the source.`);
  }
  return sentences.join(' ').replace(/\s+/g, ' ').trim();
}
