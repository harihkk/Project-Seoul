// Seoul Canvas Design Lab - provenance UI.
// Freshness labels, source attribution, partial/streaming state chips, source
// conflicts, and structured errors, rendered from the canonical semantic
// result. Conflicts are surfaced, never silently resolved; gaps
// (unavailable_field_ids, continuation) are named, never papered over.

import type { SemanticResult } from './protocol.js';
import { elt } from './renderers.js';

export function freshnessLabel(freshness: string | undefined): string {
  return (
    { real_time: 'live', delayed: 'delayed', cached: 'cached', stale: 'STALE' }[freshness ?? ''] ?? (freshness || 'cached')
  );
}

export function marginNote(cls: string, text: string): HTMLElement {
  return elt('div', `m-line ${cls}`, text);
}

/** Margin notes describing where the data came from and how current it is. */
export function provenanceMarginNotes(result: SemanticResult): HTMLElement[] {
  const p = result.provenance;
  const notes: HTMLElement[] = [];
  notes.push(marginNote('m-cap', p.provider ? `${p.provider}:${p.source_name}` : p.source_name));
  const fresh = marginNote('m-fresh', freshnessLabel(p.freshness));
  fresh.dataset.fresh = p.freshness ?? 'cached';
  notes.push(fresh);
  if (p.completeness !== undefined && p.completeness < 1) {
    notes.push(marginNote('m-partial', `${Math.round(p.completeness * 100)}% complete`));
  }
  return notes;
}

/** In-body notices for partial state, gaps, conflicts, and errors. */
export function resultNotices(result: SemanticResult): HTMLElement[] {
  const notices: HTMLElement[] = [];
  if (result.state === 'partial') {
    const note = elt('div', 'notice notice-partial');
    note.appendChild(elt('strong', '', 'Partial result.'));
    note.appendChild(
      elt('span', '', result.continuation_token ? ` A continuation is available (${result.continuation_token}).` : ' More data exists at the source.'),
    );
    notices.push(note);
  }
  if (result.state === 'streaming') {
    notices.push(elt('div', 'notice notice-stream', 'Streaming: this artifact updates in place as synthetic batches arrive.'));
  }
  for (const id of result.unavailable_field_ids ?? []) {
    notices.push(elt('div', 'notice notice-gap', `Field "${id}" was not supplied by the source and is reported as a gap, not fabricated.`));
  }
  for (const conflict of result.conflicts ?? []) {
    const note = elt('div', 'notice notice-conflict');
    note.appendChild(elt('strong', '', `Sources disagree on "${conflict.field_id}": `));
    note.appendChild(elt('span', '', conflict.note || `${conflict.source_a} and ${conflict.source_b} report different values.`));
    notices.push(note);
  }
  for (const error of result.errors ?? []) {
    notices.push(elt('div', 'notice notice-error', `${error.code}: ${error.message}`));
  }
  return notices;
}
