#!/usr/bin/env node
// Validate native/patches/manifest.json: schema, required per-entry fields, unique
// ascending order, base revision equals the lock, referenced patch files exist,
// and their sha256 matches. Dependency-free. Exits nonzero on any problem.
// An empty patch series is valid.

import { readFileSync, existsSync } from 'node:fs';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const nativeDir = path.resolve(here, '..');
const repoRoot = path.resolve(nativeDir, '..');
const manifestPath = path.join(nativeDir, 'patches', 'manifest.json');
const lockPath = path.join(nativeDir, 'chromium.lock.json');
const patchDirAbs = path.join(nativeDir, 'patches', 'chromium');

const problems = [];
const fail = (m) => problems.push(m);

function readJson(p) {
  try {
    return JSON.parse(readFileSync(p, 'utf8'));
  } catch (e) {
    fail(`cannot parse ${path.relative(repoRoot, p)}: ${e.message}`);
    return null;
  }
}

const manifest = readJson(manifestPath);
const lock = readJson(lockPath);
const SHA40 = /^[0-9a-f]{40}$/;
const SHA256 = /^[0-9a-f]{64}$/;
const REQUIRED = [
  'id', 'description', 'rationale', 'baseRevision', 'order',
  'file', 'affectedPaths', 'sha256', 'applyVerify', 'reverseVerify', 'reviewable',
];

if (manifest && lock) {
  if (manifest.schemaVersion !== 1) fail(`schemaVersion must be 1, got ${manifest.schemaVersion}`);
  if (!SHA40.test(manifest.baseRevision || '')) fail(`baseRevision must be a 40-hex SHA`);
  const lockRev = lock?.chromium?.revision;
  if (manifest.baseRevision !== lockRev) {
    fail(`manifest baseRevision (${manifest.baseRevision}) must equal lock chromium.revision (${lockRev})`);
  }
  if (!Array.isArray(manifest.patches)) {
    fail(`patches must be an array`);
  } else {
    const ids = new Set();
    const orders = [];
    manifest.patches.forEach((p, i) => {
      const where = `patches[${i}]${p && p.id ? ` (${p.id})` : ''}`;
      if (!p || typeof p !== 'object') {
        fail(`${where} is not an object`);
        return;
      }
      for (const k of REQUIRED) if (!(k in p)) fail(`${where} missing required field "${k}"`);
      if (p.id) {
        if (ids.has(p.id)) fail(`${where} duplicate id`);
        ids.add(p.id);
      }
      if (typeof p.order !== 'number' || !Number.isInteger(p.order)) fail(`${where} order must be an integer`);
      else orders.push(p.order);
      if (p.baseRevision !== manifest.baseRevision) fail(`${where} baseRevision must equal manifest baseRevision`);
      if (!Array.isArray(p.affectedPaths) || p.affectedPaths.length === 0) fail(`${where} affectedPaths must be a non-empty array`);
      if (!SHA256.test(p.sha256 || '')) fail(`${where} sha256 must be a 64-hex digest`);
      if (typeof p.file === 'string') {
        const abs = path.join(patchDirAbs, p.file);
        if (!abs.startsWith(patchDirAbs + path.sep)) fail(`${where} file escapes the patch dir`);
        else if (!existsSync(abs)) fail(`${where} file not found: native/patches/chromium/${p.file}`);
        else {
          const digest = createHash('sha256').update(readFileSync(abs)).digest('hex');
          if (digest !== p.sha256) fail(`${where} sha256 mismatch (file=${digest})`);
        }
      }
    });
    const sorted = [...orders].sort((a, b) => a - b);
    if (orders.some((o, i) => o !== sorted[i])) fail(`patch "order" values must be listed in ascending order`);
    if (new Set(orders).size !== orders.length) fail(`patch "order" values must be unique`);
  }
}

if (problems.length) {
  console.error('patch-manifest: FAIL');
  for (const p of problems) console.error('  - ' + p);
  process.exit(1);
}
const n = manifest?.patches?.length ?? 0;
console.log(`patch-manifest: OK (${n} patch${n === 1 ? '' : 'es'}, baseRevision ${manifest.baseRevision})`);
