#!/usr/bin/env node
// Generates protocol/component-catalog.json from the NATIVE sources
// (saui_catalog.cc and saui_limits.h). The native table is the single source
// of truth for component semantics - container-ness, accepted binding kinds,
// required props - and for the structural limits; nothing here is hand
// maintained. The emitted file is checked in, consumed by the Design Lab's
// patch engine, and drift-gated by scripts/check-protocol.mjs (--check).
import { readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..');
const catalogCc = readFileSync(join(repoRoot, 'native/seoul/browser/saui/saui_catalog.cc'), 'utf8');
const limitsH = readFileSync(join(repoRoot, 'native/seoul/browser/saui/saui_limits.h'), 'utf8');

// ---- Limits: every `inline constexpr size_t kMaxX = N;` -------------------
const limits = {};
for (const match of limitsH.matchAll(/inline constexpr size_t k(\w+) = (\d+);/g)) {
  const key = match[1].replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase();
  limits[key] = Number(match[2]);
}
if (!limits.max_surface_components || !limits.max_component_depth) {
  throw new Error('saui_limits.h parse failed: core limits missing');
}

// ---- Required-prop arrays: `constexpr const char* kXProps[] = {"a", "b"};` -
const propArrays = new Map();
for (const match of catalogCc.matchAll(/constexpr const char\* (k\w+)\[\] = \{([^}]*)\};/gs)) {
  propArrays.set(
    match[1],
    [...match[2].matchAll(/"([a-z_0-9]+)"/g)].map((m) => m[1]),
  );
}

// ---- Catalog rows ----------------------------------------------------------
// Row shape (ComponentTypeInfo, saui_catalog.h): type, name, category,
// container, input, chart, requires_accessible_name, binding mask,
// binding_required, required_props pointer, count.
const tableStart = catalogCc.indexOf('constexpr ComponentTypeInfo kCatalog[]');
if (tableStart === -1) throw new Error('kCatalog table not found');
const table = catalogCc.slice(tableStart);
const rowPattern = /\{ComponentType::k\w+,\s*"([a-z_0-9]+)",\s*ComponentCategory::k(\w+),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*([\w\s|]+?),\s*(true|false),\s*(k\w+|nullptr),\s*(\d+)\}/gs;
const kindNames = { kBindScalar: 'scalar', kBindRecord: 'record', kBindSeries: 'series', kBindTable: 'table' };
const components = {};
let rows = 0;
for (const match of table.matchAll(rowPattern)) {
  rows++;
  const [, name, category, container, input, chart, requiresName, maskExpr, bindingRequired, propsRef, propCount] = match;
  const bindingKinds = [...maskExpr.matchAll(/kBind(Scalar|Record|Series|Table)/g)]
    .map((m) => kindNames[`kBind${m[1]}`])
    .sort();
  const requiredProps = propsRef === 'nullptr' ? [] : (propArrays.get(propsRef) ?? null);
  if (requiredProps === null) throw new Error(`unknown props array ${propsRef}`);
  if (requiredProps.length !== Number(propCount)) {
    throw new Error(`prop count mismatch for ${name}: table says ${propCount}, array has ${requiredProps.length}`);
  }
  components[name] = {
    category: category.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase(),
    container: container === 'true',
    input: input === 'true',
    chart: chart === 'true',
    requires_accessible_name: requiresName === 'true',
    binding_kinds: bindingKinds,
    binding_required: bindingRequired === 'true',
    required_props: requiredProps,
  };
}
// The native static_assert holds the table at exactly the enum size; the
// adaptive-surface schema enum is the same list, so the count must match it.
const surfaceSchema = JSON.parse(readFileSync(join(repoRoot, 'protocol/adaptive-surface.schema.json'), 'utf8'));
const schemaTypes = surfaceSchema.$defs.component_type.enum;
if (rows !== schemaTypes.length) {
  throw new Error(`parsed ${rows} catalog rows but the schema enum has ${schemaTypes.length}`);
}
for (const type of schemaTypes) {
  if (!components[type]) throw new Error(`catalog row missing for schema type ${type}`);
}

const doc = {
  note: 'GENERATED from native/seoul/browser/saui/saui_catalog.cc and saui_limits.h by scripts/generate-component-catalog.mjs; do not edit. Drift-gated by scripts/check-protocol.mjs.',
  limits,
  components,
};
const content = JSON.stringify(doc, null, 2) + '\n';
const target = join(repoRoot, 'protocol/component-catalog.json');
if (process.argv.includes('--check')) {
  let current = '';
  try {
    current = readFileSync(target, 'utf8');
  } catch {
    // missing counts as drift
  }
  if (current !== content) {
    console.error('protocol/component-catalog.json is stale; run: node scripts/generate-component-catalog.mjs');
    process.exit(1);
  }
  console.log('component catalog in sync with native sources');
} else {
  writeFileSync(target, content);
  console.log(`generated ${target} (${rows} components, ${Object.keys(limits).length} limits)`);
}
