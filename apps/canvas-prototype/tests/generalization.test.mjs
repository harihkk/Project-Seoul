// Generalization tests for the adaptive interface compiler. Each schema below
// is HELD OUT - none is special-cased anywhere in the compiler. The test proves
// the representation is chosen from shape + roles alone, that an explicit
// request is honored only when compatible, that a chart falls back to a table
// when it would mislead, and that the deterministic planner routes goals to
// capabilities by their own text. The prototype logic is esbuild-bundled here
// so the test is self-contained (no build step required).

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
const { compileInterface, availableRepresentations, planGoal, runTask } = core;

function prov() {
  return { source: 'test.capability', retrievedAt: 0, freshness: 'cached', completeness: 1 };
}

test('held-out time series with a measure compiles to a line chart', () => {
  const result = {
    schema: {
      shape: 'time_series',
      fields: [
        { id: 'observed_at', label: 'Observed', primitive: 'timestamp', role: 'timestamp' },
        { id: 'flux', label: 'Flux', primitive: 'number', role: 'measure', unit: 'W' },
      ],
    },
    data: [
      { observed_at: 1000, flux: 4 },
      { observed_at: 2000, flux: 6 },
      { observed_at: 3000, flux: 5 },
    ],
    provenance: prov(),
    state: 'complete',
  };
  const surface = compileInterface(result, {});
  assert.equal(surface.root.type, 'line_chart');
  assert.ok(surface.reasons.includes('temporal_measure_line_chart'));
});

test('held-out entity collection with a category + measure compiles to a bar chart', () => {
  const result = {
    schema: {
      shape: 'entity_collection',
      fields: [
        { id: 'zone', label: 'Zone', primitive: 'string', role: 'category' },
        { id: 'defect_rate', label: 'Defect rate', primitive: 'number', role: 'measure', unit: '%' },
      ],
    },
    data: [
      { zone: 'north', defect_rate: 2.1 },
      { zone: 'south', defect_rate: 3.4 },
    ],
    provenance: prov(),
    state: 'complete',
  };
  const surface = compileInterface(result, {});
  assert.equal(surface.root.type, 'bar_chart');
});

test('held-out collection with no numeric measure falls back to a table', () => {
  const result = {
    schema: {
      shape: 'entity_collection',
      fields: [
        { id: 'ticket', label: 'Ticket', primitive: 'string', role: 'identifier' },
        { id: 'assignee', label: 'Assignee', primitive: 'string', role: 'category' },
      ],
    },
    data: [{ ticket: 'T-1', assignee: 'a' }, { ticket: 'T-2', assignee: 'b' }],
    provenance: prov(),
    state: 'complete',
  };
  const surface = compileInterface(result, {});
  assert.equal(surface.root.type, 'table');
});

test('a chart request on non-numeric data is rejected, not obeyed', () => {
  const result = {
    schema: {
      shape: 'entity_collection',
      fields: [
        { id: 'ticket', label: 'Ticket', primitive: 'string', role: 'identifier' },
        { id: 'assignee', label: 'Assignee', primitive: 'string', role: 'category' },
      ],
    },
    data: [{ ticket: 'T-1', assignee: 'a' }],
    provenance: prov(),
    state: 'complete',
  };
  const surface = compileInterface(result, { requestedRepresentation: 'bar_chart' });
  assert.equal(surface.root.type, 'table');
  assert.ok(surface.reasons.includes('user_request_rejected_incompatible'));
});

test('a compatible representation request is honored and re-uses the surface id', () => {
  const result = {
    schema: {
      shape: 'time_series',
      fields: [
        { id: 'observed_at', label: 'Observed', primitive: 'timestamp', role: 'timestamp' },
        { id: 'flux', label: 'Flux', primitive: 'number', role: 'measure' },
      ],
    },
    data: [{ observed_at: 1000, flux: 4 }, { observed_at: 2000, flux: 6 }],
    provenance: prov(),
    state: 'complete',
  };
  const first = compileInterface(result, {});
  const patched = compileInterface(result, { requestedRepresentation: 'table' }, first.id);
  assert.equal(patched.id, first.id); // same surface, patched in place
  assert.equal(patched.root.type, 'table');
});

test('held-out scalar compiles to a metric; record to a card', () => {
  const scalar = compileInterface(
    { schema: { shape: 'scalar', fields: [{ id: 'v', label: 'Coverage', primitive: 'number', role: 'percentage' }] }, data: 87.5, provenance: prov(), state: 'complete' },
    {},
  );
  assert.equal(scalar.root.type, 'metric');

  const record = compileInterface(
    { schema: { shape: 'record', fields: [{ id: 'name', label: 'Name', primitive: 'string', role: 'name' }, { id: 'kind', label: 'Kind', primitive: 'string', role: 'category' }] }, data: { name: 'x', kind: 'y' }, provenance: prov(), state: 'complete' },
    {},
  );
  assert.equal(record.root.type, 'entity_card');
});

test('empty collection compiles to an empty state, not a broken chart', () => {
  const surface = compileInterface(
    { schema: { shape: 'entity_collection', fields: [{ id: 'x', label: 'X', primitive: 'string', role: 'identifier' }] }, data: [], provenance: prov(), state: 'empty' },
    {},
  );
  assert.equal(surface.root.type, 'empty_state');
});

test('availableRepresentations is derived from shape/roles, never a fixed list', () => {
  const reps = availableRepresentations({
    schema: {
      shape: 'time_series',
      fields: [
        { id: 't', label: 'T', primitive: 'timestamp', role: 'timestamp' },
        { id: 'a', label: 'A', primitive: 'number', role: 'measure' },
        { id: 'b', label: 'B', primitive: 'number', role: 'measure' },
      ],
    },
    data: [],
    provenance: prov(),
  });
  assert.ok(reps.includes('table'));
  assert.ok(reps.includes('line_chart'));
  assert.ok(reps.includes('scatter_chart')); // two measures
});

test('the deterministic planner routes a goal to a capability by its own words', () => {
  const p = planGoal('show the pipeline latency over time');
  assert.equal(p.ok, true);
  assert.equal(p.capability.id, 'info.pipeline.latency');

  const none = planGoal('zzqx vvwq pppl');
  assert.equal(none.ok, false);
  assert.ok(none.failure);
});

test('runTask executes the planned capability and returns a verified receipt', () => {
  const run = runTask('collect the substrate survey readings per station');
  assert.equal(run.ok, true);
  assert.equal(run.result.schema.shape, 'entity_collection');
  assert.equal(run.receipt.verified, true);
  assert.equal(run.receipt.route, 'deterministic');
});
