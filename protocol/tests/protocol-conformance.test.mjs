// Seoul canonical protocol: TypeScript-side conformance tests.
// Consumes the SAME fixture corpus as the native C++ conformance tests
// (semantic_wire_unittest.cc, saui_protocol_fixtures_unittest.cc,
// task_snapshot_wire_unittest.cc, tool_descriptor_wire_unittest.cc), so the
// two languages cannot drift without a failure on one side.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { validate, formatErrors } from '../ts/validate.mjs';

const protocolDir = join(dirname(fileURLToPath(import.meta.url)), '..');
const fixtureDir = join(protocolDir, 'fixtures');

const SCHEMA_FILES = [
  'semantic-result.schema.json',
  'adaptive-surface.schema.json',
  'surface-patch.schema.json',
  'component-event.schema.json',
  'task-snapshot.schema.json',
  'capability-descriptor.schema.json',
];
const registry = {};
for (const file of SCHEMA_FILES) {
  registry[file] = JSON.parse(readFileSync(join(protocolDir, file), 'utf8'));
}
const schemaByShortName = (name) => registry[`${name}.schema.json`];

function fixture(relative) {
  return JSON.parse(readFileSync(join(fixtureDir, relative), 'utf8'));
}

function listJson(dir) {
  return readdirSync(join(fixtureDir, dir))
    .filter((f) => f.endsWith('.json'))
    .sort()
    .map((f) => `${dir}/${f}`);
}

test('every semantic fixture validates against semantic-result.schema.json', () => {
  const cases = listJson('semantic').filter((f) => !f.includes('append'));
  assert.equal(cases.length, 20, 'the named corpus cases stay in sync with kSemanticCases in semantic_wire_unittest.cc');
  for (const name of cases) {
    const result = validate(fixture(name), schemaByShortName('semantic-result'), registry);
    assert.ok(result.valid, `${name}:\n${formatErrors(result.errors)}`);
  }
});

test('the streaming append companion carries schema-conformant rows', () => {
  const base = fixture('semantic/streaming-update.json');
  const append = fixture('semantic/streaming-update.append.json');
  assert.equal(base.state, 'streaming');
  assert.ok(Array.isArray(append.rows) && append.rows.length === 3);
  const fieldIds = new Set(base.schema.fields.map((f) => f.id));
  for (const row of append.rows) {
    for (const key of Object.keys(row)) {
      assert.ok(fieldIds.has(key), `append row carries undeclared field ${key}`);
    }
  }
  // Merging is what the Design Lab runtime does for streaming updates; the
  // merged document must still validate.
  const merged = { ...base, data: [...base.data, ...append.rows] };
  const result = validate(merged, schemaByShortName('semantic-result'), registry);
  assert.ok(result.valid, formatErrors(result.errors));
});

test('surface, patch, event, task, and capability fixtures validate', () => {
  const suites = [
    ['surface', 'adaptive-surface'],
    ['patch', 'surface-patch'],
    ['event', 'component-event'],
    ['task', 'task-snapshot'],
    ['capability', 'capability-descriptor'],
  ];
  for (const [dir, schemaName] of suites) {
    for (const name of listJson(dir)) {
      const result = validate(fixture(name), schemaByShortName(schemaName), registry);
      assert.ok(result.valid, `${name}:\n${formatErrors(result.errors)}`);
    }
  }
});

test('the invalid corpus fails validation, each case', () => {
  const { cases } = fixture('invalid/expectations.json');
  assert.ok(cases.length >= 7);
  for (const entry of cases) {
    const result = validate(fixture(entry.file), schemaByShortName(entry.schema), registry);
    assert.equal(result.valid, false, `${entry.file} unexpectedly validated`);
  }
});

test('protocol-version compatibility: non-version-1 documents are rejected', () => {
  const { cases } = fixture('compat/expectations.json');
  assert.equal(cases.length, 4);
  for (const entry of cases) {
    const result = validate(fixture(entry.file), schemaByShortName(entry.schema), registry);
    assert.equal(result.valid, false, `${entry.file} must be rejected: ${entry.reason}`);
    const versionError = result.errors.some(
      (e) => e.path.includes('schema_version') || e.message.includes('schema_version'),
    );
    assert.ok(versionError, `${entry.file} must fail on the version field, got:\n${formatErrors(result.errors)}`);
  }
});

test('the catalog manifest references only existing fixtures and routes uniquely', () => {
  const catalog = fixture('catalog.json');
  assert.equal(catalog.entries.length, 20);
  const ids = new Set();
  for (const entry of catalog.entries) {
    const descriptor = fixture(entry.capability);
    const result = fixture(entry.result);
    assert.equal(descriptor.id, entry.id, `${entry.capability} id mismatch`);
    assert.ok(!ids.has(entry.id), `duplicate capability id ${entry.id}`);
    ids.add(entry.id);
    assert.ok(entry.example_goal.length > 0);
    assert.ok(result.schema.shape, `${entry.result} is not a semantic result`);
    // Fixture capabilities must declare their synthetic nature.
    assert.equal(descriptor.provider, 'fixture');
    assert.match(descriptor.observation_contract, /no real browser/i);
  }
});

test('validator subset: oneOf discrimination and unsupported keywords', () => {
  const patchSchema = schemaByShortName('surface-patch');
  const good = validate(
    { surface_id: '5a0d7e31-2c4b-4f6a-8d9e-1b3c5d7f9a0b', ops: [{ op: 'set_title', title: 'x' }] },
    patchSchema,
    registry,
  );
  assert.ok(good.valid, formatErrors(good.errors));

  const badTwoWays = validate(
    { surface_id: '5a0d7e31-2c4b-4f6a-8d9e-1b3c5d7f9a0b', ops: [{ op: 'set_title' }] },
    patchSchema,
    registry,
  );
  assert.equal(badTwoWays.valid, false);

  assert.throws(
    () => validate({}, { type: 'object', patternProperties: {} }),
    /unsupported keyword/,
    'keywords outside the documented subset must be a hard error',
  );
});

test('event-handler-shaped and markup prop keys are rejected by pattern', () => {
  const surfaceSchema = schemaByShortName('adaptive-surface');
  for (const key of ['onclick', 'onload', 'html', 'script', 'srcdoc', 'innerhtml', 'style']) {
    const doc = {
      schema_version: 1,
      kind: 'response',
      components: [{ id: 'a', type: 'text', props: { [key]: 'x' } }],
    };
    const result = validate(doc, surfaceSchema, registry);
    assert.equal(result.valid, false, `prop key "${key}" must be rejected`);
  }
});
