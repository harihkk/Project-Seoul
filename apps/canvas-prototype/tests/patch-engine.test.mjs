// Surface-store patch engine tests: canonical schema validation at the door,
// atomic all-or-nothing application, precise change summaries, and every one
// of the ten canonical op kinds - including set_bindings (update binding) and
// upsert_data_entry (update data entry). DOM behavior (focus/scroll/identity
// preservation) is covered by render.smoke.mjs in a real browser.

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
const bundlePath = path.join(outDir, 'core.patch-test.mjs');
writeFileSync(bundlePath, bundle.outputFiles[0].text, 'utf8');
const { SurfaceStore, capabilities, compileInterface } = await import(pathToFileURL(bundlePath).href);

function chartSurfaceStore() {
  const capability = capabilities().find((c) => c.descriptor.id === 'info.pipeline.latency');
  const compiled = compileInterface(capability.result, { title: 'Latency' });
  const store = new SurfaceStore();
  store.put(compiled.surface);
  return { store, surfaceId: compiled.surface.id, surface: compiled.surface };
}

test('set_props merges and reports exactly the touched component', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const outcome = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'set_props', target: 'root', props: { title: 'Updated' } }],
  });
  assert.equal(outcome.ok, true);
  assert.deepEqual(outcome.applied.changed_component_ids, ['root']);
  assert.equal(outcome.applied.new_revision, 2);
  assert.equal(outcome.surface.components[0].props.title, 'Updated');
  // Untouched props survive the merge.
  assert.equal(outcome.surface.components[0].props.x_key, 'sampled_at');
});

test('a failed op leaves the surface untouched (atomicity)', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const before = JSON.stringify(store.get(surfaceId));
  const outcome = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      { op: 'set_title', title: 'New title' },
      { op: 'set_props', target: 'does-not-exist', props: { a: 1 } },
    ],
  });
  assert.equal(outcome.ok, false);
  assert.match(outcome.error, /no component/);
  assert.equal(JSON.stringify(store.get(surfaceId)), before, 'nothing from the good op remains');
  assert.equal(store.revision(surfaceId), 1);
});

test('a patch outside the canonical schema is rejected at the door', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const unknownOp = store.applyPatch({ surface_id: surfaceId, ops: [{ op: 'transmogrify', target: 'root' }] });
  assert.equal(unknownOp.ok, false);
  assert.match(unknownOp.error, /canonical schema/);
  const emptyOps = store.applyPatch({ surface_id: surfaceId, ops: [] });
  assert.equal(emptyOps.ok, false);
  const badId = store.applyPatch({ surface_id: 'not-a-uuid', ops: [{ op: 'set_title', title: 'x' }] });
  assert.equal(badId.ok, false);
});

test('set_bindings rejects a dangling rebind and applies a valid one', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const dangling = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'set_bindings', target: 'root', bindings: { data: 'missing_entry' } }],
  });
  assert.equal(dangling.ok, false);
  assert.match(dangling.error, /missing data entry/);
  assert.equal(store.revision(surfaceId), 1);

  const rebind = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      {
        op: 'upsert_data_entry',
        entry: 'latency_backfill',
        value: { kind: 'series', points: [{ t_ms: 1000, y: 1 }], x_unit: 'ms since epoch', y_unit: 'ms' },
      },
      { op: 'set_bindings', target: 'root', bindings: { data: 'latency_backfill' } },
    ],
  });
  assert.equal(rebind.ok, true, JSON.stringify(rebind));
  assert.equal(rebind.surface.components[0].bindings.data, 'latency_backfill');
  assert.deepEqual(rebind.applied.changed_entry_names, ['latency_backfill']);
});

test('append_series_points appends only to a series entry', () => {
  const { store, surfaceId, surface } = chartSurfaceStore();
  const seriesEntry = Object.keys(surface.data).find((k) => k.startsWith('series_'));
  const before = store.get(surfaceId).data[seriesEntry].points.length;
  const ok = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'append_series_points', entry: seriesEntry, points: [{ t_ms: 99999, y: 1.5 }] }],
  });
  assert.equal(ok.ok, true);
  assert.equal(store.get(surfaceId).data[seriesEntry].points.length, before + 1);
  const notSeries = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'append_series_points', entry: 'rows', points: [{ t_ms: 1, y: 1 }] }],
  });
  assert.equal(notSeries.ok, false);
  assert.match(notSeries.error, /not a series/);
});

test('append_child, remove_component, replace_component, set_actions, set_state', () => {
  const { store, surfaceId } = chartSurfaceStore();
  // The chart root is not a container in the Lab tree; replace it with a
  // stack, then exercise structural ops against the stack.
  const toStack = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      {
        op: 'replace_component',
        target: 'root',
        component: {
          id: 'root',
          type: 'stack',
          children: [{ id: 'note-a', type: 'text', props: { text: 'a' } }],
        },
      },
    ],
  });
  assert.equal(toStack.ok, true);

  const structural = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      { op: 'append_child', target: 'root', component: { id: 'note-b', type: 'badge', props: { text: 'b' } } },
      { op: 'set_state', target: 'note-b', state: 'loading', message: 'Refreshing' },
      { op: 'remove_component', target: 'note-a' },
      { op: 'set_title', title: 'Structured' },
      {
        op: 'set_actions',
        actions: [{ id: 'go', label: 'Go', kind: 'local_state', target: 'k', requires_confirmation: false }],
      },
    ],
  });
  assert.equal(structural.ok, true, JSON.stringify(structural));
  const surface = store.get(surfaceId);
  assert.equal(surface.components[0].children.length, 1);
  assert.equal(surface.components[0].children[0].id, 'note-b');
  assert.equal(surface.components[0].children[0].state, 'loading');
  assert.equal(surface.title, 'Structured');
  assert.equal(surface.actions.length, 1);
  assert.equal(structural.applied.title_changed, true);
  assert.equal(structural.applied.actions_changed, true);
  assert.equal(store.revision(surfaceId), 3);

  // Removing the last remaining top-level component must fail.
  const removeRoot = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'remove_component', target: 'root' }],
  });
  assert.equal(removeRoot.ok, false);
});

test('duplicate component ids are rejected on append', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const toStack = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      {
        op: 'replace_component',
        target: 'root',
        component: { id: 'root', type: 'stack', children: [{ id: 'x', type: 'text', props: { text: 'x' } }] },
      },
    ],
  });
  assert.equal(toStack.ok, true);
  const dup = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'append_child', target: 'root', component: { id: 'x', type: 'text', props: { text: 'again' } } }],
  });
  assert.equal(dup.ok, false);
  assert.match(dup.error, /already exists/);
});

test('catalog rules are enforced from the generated native catalog', () => {
  const { store, surfaceId } = chartSurfaceStore();
  // Charts are not containers.
  const intoChart = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'append_child', target: 'root', component: { id: 'x', type: 'text', props: { text: 'x' } } }],
  });
  assert.equal(intoChart.ok, false);
  assert.match(intoChart.error, /not a container/);

  // A replacement that introduces a duplicate id elsewhere in the tree is
  // rejected by the whole-surface re-check.
  const toStack = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      {
        op: 'replace_component',
        target: 'root',
        component: {
          id: 'root',
          type: 'stack',
          children: [
            { id: 'a', type: 'text', props: { text: 'a' } },
            { id: 'b', type: 'text', props: { text: 'b' } },
          ],
        },
      },
    ],
  });
  assert.equal(toStack.ok, true);
  const dupViaReplace = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'replace_component', target: 'b', component: { id: 'a', type: 'text', props: { text: 'dup' } } }],
  });
  assert.equal(dupViaReplace.ok, false);
  assert.match(dupViaReplace.error, /duplicate component id/);

  // Binding-kind compatibility: a metric cannot bind a table entry.
  const wrongKind = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      { op: 'replace_component', target: 'a', component: { id: 'a', type: 'metric', props: { label: 'L' }, bindings: { data: 'rows' } } },
    ],
  });
  assert.equal(wrongKind.ok, false);
  assert.match(wrongKind.error, /cannot bind a table entry/);

  // Required props come from the catalog: a chart without units is rejected.
  const missingProp = store.applyPatch({
    surface_id: surfaceId,
    ops: [
      {
        op: 'replace_component',
        target: 'a',
        component: { id: 'a', type: 'line_chart', props: { title: 't', x_label: 'x', y_label: 'y' }, bindings: { data: 'rows' }, accessible_name: 'c' },
      },
    ],
  });
  assert.equal(missingProp.ok, false);
  assert.match(missingProp.error, /missing required prop "units"/);
});

test('replacing a component under a new id reports the old id for removal', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const outcome = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'replace_component', target: 'root', component: { id: 'root-v2', type: 'text', props: { text: 'replaced' } } }],
  });
  assert.equal(outcome.ok, true, JSON.stringify(outcome));
  assert.deepEqual(outcome.applied.changed_component_ids, ['root', 'root-v2']);
});

test('set_state without a message clears the previous message (native parity)', () => {
  const { store, surfaceId } = chartSurfaceStore();
  const withMessage = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'set_state', target: 'root', state: 'error', message: 'boom' }],
  });
  assert.equal(withMessage.ok, true);
  assert.equal(store.get(surfaceId).components[0].state_message, 'boom');
  const cleared = store.applyPatch({
    surface_id: surfaceId,
    ops: [{ op: 'set_state', target: 'root', state: 'ready' }],
  });
  assert.equal(cleared.ok, true);
  assert.equal(store.get(surfaceId).components[0].state_message, undefined);
});

test('an unprojectable semantic result degrades to an explained error artifact', () => {
  const longId = 'a'.repeat(50); // valid semantic field id, beyond the SAUI key budget
  const compiled = compileInterface({
    schema: {
      schema_version: 1,
      shape: 'table',
      fields: [
        { id: longId, label: 'Long', primitive: 'string', role: 'dimension' },
        { id: 'n', label: 'N', primitive: 'number', role: 'measure' },
      ],
    },
    data: [
      { [longId]: 'x', n: 1 },
      { [longId]: 'y', n: 2 },
    ],
    provenance: { source_name: 'held-out', retrieved_at_ms: 1767225600000, effective_at_ms: 1767225600000 },
  });
  assert.equal(compiled.representation, 'error_state');
  assert.deepEqual(compiled.reasons, ['saui_projection_unrepresentable']);
});
