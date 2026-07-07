// Generalization tests for the Design Lab compiler and fixture runtime, over
// CANONICAL wire documents. Each held-out schema below is special-cased
// nowhere; the representation is chosen from shape + roles alone. The suite
// also proves: an explicit request is honored only when compatible, a chart
// falls back to an EXPLAINED table when it would mislead, the deterministic
// planner routes by descriptor text, every registered fixture capability
// compiles to a schema-valid surface, and receipts say "fixture contract
// validated" - never "verified" - because no real postcondition is observed.

import test from 'node:test';
import assert from 'node:assert/strict';
import esbuild from 'esbuild';
import { writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const here = path.dirname(new URL(import.meta.url).pathname);
const outDir = path.join(here, '..', 'dist');
mkdirSync(outDir, { recursive: true });

const bundle = await esbuild.build({
  entryPoints: [path.join(here, '..', 'src', 'core.ts')],
  bundle: true,
  format: 'esm',
  platform: 'neutral',
  write: false,
});
const bundlePath = path.join(outDir, 'core.test.mjs');
writeFileSync(bundlePath, bundle.outputFiles[0].text, 'utf8');
const core = await import(pathToFileURL(bundlePath).href);
const {
  availableRepresentations,
  buildRepresentationPatch,
  capabilities,
  compileInterface,
  mergeStreamingRows,
  planGoal,
  runTask,
  validateSurface,
  validateTaskSnapshot,
  SurfaceStore,
} = core;

const PROV = {
  source_name: 'held-out fixture',
  retrieved_at_ms: 1767225600000,
  effective_at_ms: 1767225600000,
  freshness: 'cached',
};

function field(id, label, primitive, role, extra = {}) {
  return { id, label, primitive, role, ...extra };
}

test('held-out time series with a measure compiles to a line chart', () => {
  const result = {
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('flux', 'Flux', 'number', 'measure', { unit: 'u' }),
      ],
    },
    data: [
      { at: 1000, flux: 1.5 },
      { at: 2000, flux: 2.5 },
    ],
    provenance: PROV,
  };
  const compiled = compileInterface(result);
  assert.equal(compiled.representation, 'line_chart');
  assert.ok(compiled.reasons.includes('temporal_measure_line_chart'));
  assert.equal(validateSurface(compiled.surface).valid, true);
  // The single-measure chart binds its series entry, so stream patches update
  // exactly what the chart reads.
  assert.equal(compiled.surface.components[0].bindings.data, 'series_flux');
});

test('held-out collection with category + measure compiles to a bar chart', () => {
  const result = {
    schema: {
      schema_version: 1,
      shape: 'entity_collection',
      fields: [
        field('basin', 'Basin', 'string', 'category'),
        field('yield_kg', 'Yield', 'number', 'measure', { unit: 'kg' }),
      ],
    },
    data: [
      { basin: 'north', yield_kg: 12 },
      { basin: 'south', yield_kg: 9 },
    ],
    provenance: PROV,
  };
  const compiled = compileInterface(result);
  assert.equal(compiled.representation, 'bar_chart');
});

test('collection without a numeric measure falls back to an explained table', () => {
  const result = {
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('note', 'Note', 'string', 'description'),
      ],
    },
    data: [{ at: 1000, note: 'calm' }],
    provenance: PROV,
  };
  const compiled = compileInterface(result);
  assert.equal(compiled.representation, 'sortable_table');
  assert.ok(compiled.reasons.includes('chart_would_mislead_fallback'));
});

test('an incompatible chart request is rejected with a named reason', () => {
  const result = {
    schema: {
      schema_version: 1,
      shape: 'record',
      fields: [field('id', 'Id', 'string', 'identifier')],
    },
    data: { id: 'x' },
    provenance: PROV,
  };
  const compiled = compileInterface(result, { requestedRepresentation: 'line_chart' });
  assert.ok(compiled.reasons.includes('user_request_rejected_incompatible'));
  assert.notEqual(compiled.representation, 'line_chart');
});

test('a compatible representation request is honored on the SAME surface id', () => {
  const result = {
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('flux', 'Flux', 'number', 'measure'),
      ],
    },
    data: [
      { at: 1000, flux: 1 },
      { at: 2000, flux: 2 },
    ],
    provenance: PROV,
  };
  const first = compileInterface(result);
  const second = compileInterface(result, { requestedRepresentation: 'sortable_table' }, first.surface.id);
  assert.equal(second.surface.id, first.surface.id);
  assert.equal(second.representation, 'sortable_table');
  assert.equal(second.surface.components[0].id, 'root');
});

test('scalar compiles to metric; record compiles to entity card', () => {
  const scalar = compileInterface({
    schema: { schema_version: 1, shape: 'scalar', fields: [field('v', 'V', 'number', 'percentage')] },
    data: 42.5,
    provenance: PROV,
  });
  assert.equal(scalar.representation, 'metric');
  const record = compileInterface({
    schema: {
      schema_version: 1,
      shape: 'record',
      fields: [field('id', 'Id', 'string', 'identifier'), field('name', 'Name', 'string', 'name')],
    },
    data: { id: 'a', name: 'Alpha' },
    provenance: PROV,
  });
  assert.equal(record.representation, 'entity_card');
});

test('an empty collection compiles to an empty state, not a broken chart', () => {
  const compiled = compileInterface({
    schema: {
      schema_version: 1,
      shape: 'entity_collection',
      fields: [field('id', 'Id', 'string', 'identifier'), field('n', 'N', 'number', 'measure')],
    },
    data: [],
    provenance: PROV,
  });
  assert.equal(compiled.representation, 'empty_state');
});

test('availableRepresentations derives from shape and roles', () => {
  const reps = availableRepresentations({
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('a', 'A', 'number', 'measure'),
        field('b', 'B', 'number', 'measure'),
      ],
    },
    data: [
      { at: 1, a: 1, b: 2 },
      { at: 2, a: 2, b: 3 },
    ],
    provenance: PROV,
  });
  assert.ok(reps.includes('line_chart'));
  assert.ok(reps.includes('scatter_chart'));
  assert.ok(reps.includes('sortable_table'));
});

test('the deterministic planner routes by descriptor text alone', () => {
  const plan = planGoal('show the pipeline latency over time');
  assert.equal(plan.ok, true);
  assert.equal(plan.capability.descriptor.id, 'info.pipeline.latency');
  const miss = planGoal('zzzz qqqq');
  assert.equal(miss.ok, false);
});

test('runTask validates the fixture contract and says so - no observation claim', () => {
  const run = runTask('collect the substrate survey readings per station');
  assert.equal(run.ok, true);
  assert.equal(run.result.schema.shape, 'entity_collection');
  const receipt = run.snapshot.receipts[0];
  assert.equal(receipt.verification.method, 'fixture_contract');
  assert.equal(receipt.verification.detail, 'fixture contract validated');
  assert.equal(receipt.route, 'deterministic');
  assert.match(receipt.observed_summary, /synthetic demo data/);
  assert.equal(validateTaskSnapshot(run.snapshot).valid, true);
});

test('a fixture that reports a structured error yields a failed task, honestly', () => {
  const run = runTask('read the unreachable archive inventory');
  assert.equal(run.capability.descriptor.id, 'info.archive.inventory');
  assert.equal(run.snapshot.state, 'failed');
  assert.equal(run.snapshot.receipts[0].verification.verified, false);
  const compiled = compileInterface(run.result);
  assert.equal(compiled.representation, 'error_state');
});

test('every registered fixture capability compiles to a schema-valid surface', () => {
  for (const capability of capabilities()) {
    const compiled = compileInterface(capability.result, { title: capability.descriptor.name });
    const valid = validateSurface(compiled.surface);
    assert.ok(valid.valid, `${capability.descriptor.id} compiled to an invalid surface`);
    assert.ok(compiled.reasons.length > 0, `${capability.descriptor.id} compiled without a stated reason`);
    // The corpus must stay projectable: a fixture that degrades to the
    // unrepresentable fallback (see the key-budget gap in protocol/README.md)
    // is a corpus regression, not a pass.
    assert.ok(
      !compiled.reasons.includes('saui_projection_unrepresentable'),
      `${capability.descriptor.id} no longer projects into SAUI budgets`,
    );
  }
});

test('streaming merge accepts conformant rows and rejects a completed result', () => {
  const streaming = capabilities().find((c) => c.descriptor.id === 'info.queue.depth');
  const merged = mergeStreamingRows(streaming.result, streaming.streamAppendRows);
  assert.ok(merged, 'conformant rows must merge');
  assert.equal(merged.data.length, streaming.result.data.length + streaming.streamAppendRows.length);
  const completed = capabilities().find((c) => c.descriptor.id === 'info.survey.readings');
  assert.equal(mergeStreamingRows(completed.result, [{ station: 'x' }]), undefined);
});

test('a representation switch is a canonical replace_component patch', () => {
  const capability = capabilities().find((c) => c.descriptor.id === 'info.pipeline.latency');
  const compiled = compileInterface(capability.result, { title: 'Latency' });
  const store = new SurfaceStore();
  store.put(compiled.surface);
  const { patch } = buildRepresentationPatch(capability.result, compiled.surface.id, 'sortable_table', 'Latency');
  const outcome = store.applyPatch(patch);
  assert.equal(outcome.ok, true, JSON.stringify(outcome));
  assert.equal(outcome.surface.components[0].type, 'sortable_table');
  assert.equal(outcome.applied.new_revision, 2);
});

test('chart honesty matches native policy: single points and unattributed data never chart', () => {
  const singlePoint = compileInterface({
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('flux', 'Flux', 'number', 'measure'),
      ],
    },
    data: [{ at: 1000, flux: 1.5 }],
    provenance: PROV,
  });
  assert.equal(singlePoint.representation, 'sortable_table', 'one point is a number, not a trend');
  assert.ok(singlePoint.reasons.includes('chart_would_mislead_fallback'));

  const unattributed = compileInterface({
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('flux', 'Flux', 'number', 'measure'),
      ],
    },
    data: [
      { at: 1000, flux: 1 },
      { at: 2000, flux: 2 },
    ],
    // No effective_at_ms: charts require fully timed attribution.
    provenance: { source_name: 'held-out fixture', retrieved_at_ms: 1767225600000 },
  });
  assert.equal(unattributed.representation, 'sortable_table');
  assert.ok(!availableRepresentations({
    schema: unattributed.surface ? { schema_version: 1, shape: 'time_series', fields: [field('at', 'At', 'timestamp', 'timestamp'), field('flux', 'Flux', 'number', 'measure')] } : undefined,
    data: [{ at: 1000, flux: 1 }, { at: 2000, flux: 2 }],
    provenance: { source_name: 'held-out fixture', retrieved_at_ms: 1767225600000 },
  }).includes('line_chart'), 'representation switches must not offer a chart the compiler would refuse');
});

test('the voice summarizer speaks an honest insight for every fixture, from roles alone', () => {
  const { summarizeResult } = core;
  for (const capability of capabilities()) {
    const insight = summarizeResult(capability.result);
    assert.ok(insight.length > 10, `${capability.descriptor.id} produced no insight`);
    assert.ok(insight.length < 400, `${capability.descriptor.id} insight too long to speak`);
  }
  // Trend, peak, and unit come from roles on a held-out series.
  const trend = summarizeResult({
    schema: {
      schema_version: 1,
      shape: 'time_series',
      fields: [
        field('at', 'At', 'timestamp', 'timestamp'),
        field('flux', 'Flux', 'number', 'measure', { unit: 'mw' }),
      ],
    },
    data: [
      { at: 1767225600000, flux: 10 },
      { at: 1767229200000, flux: 14 },
      { at: 1767232800000, flux: 18 },
    ],
    provenance: PROV,
  });
  assert.match(trend, /Flux averaged 14 mw across 3 samples/);
  assert.match(trend, /up 80 percent/);
  assert.match(trend, /peaking at 18 mw/);
  // Honesty riders always speak.
  const failed = capabilities().find((c) => c.descriptor.id === 'info.archive.inventory');
  assert.match(summarizeResult(failed.result), /did not work/i);
  const conflicted = capabilities().find((c) => c.descriptor.id === 'info.survey.disagreement');
  assert.match(summarizeResult(conflicted.result), /sources disagree on Reading/);
  const partial = capabilities().find((c) => c.descriptor.id === 'info.archive.exports');
  assert.match(summarizeResult(partial.result), /partial result/);
  assert.match(summarizeResult(partial.result), /Checksum was not supplied/);
});
