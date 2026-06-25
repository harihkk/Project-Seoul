#!/usr/bin/env node
// Project Seoul Development Harness - static extension validator.
//
// Verifies the built unpacked extension and the tracked project source against
// the milestone's product-safety constraints. Exits non-zero on any violation.

import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const ROOT = process.cwd();
const DIST = path.join(ROOT, 'dist', 'browser-harness');

const APPROVED_PERMISSIONS = ['activeTab', 'scripting', 'sidePanel', 'storage'];
const MIN_CHROME = 114;

// Prohibited assistant/tool names, assembled from char codes so this validator
// file does not itself contain the literal strings it scans for.
const NAME_NEEDLES = [
  [99, 108, 97, 117, 100, 101],
  [97, 110, 116, 104, 114, 111, 112, 105, 99],
  [99, 111, 112, 105, 108, 111, 116],
  [99, 104, 97, 116, 103, 112, 116],
  [99, 111, 100, 101, 105, 117, 109],
  [111, 112, 101, 110, 97, 105],
].map((codes) => String.fromCharCode(...codes));

const SKIP_DIRS = new Set(['.git', 'node_modules', 'dist']);
const SECRET_PATTERNS = [
  /\.env(\.|$)/i,
  /\.pem$/i,
  /\.key$/i,
  /\.p12$/i,
  /\.pfx$/i,
  /\.keystore$/i,
  /\.crt$/i,
  /id_rsa/i,
  /credential/i,
  /secret/i,
];

const problems = [];
function problem(msg) {
  problems.push(msg);
}

function walkFiles(dir, skipNamed = new Set()) {
  const out = [];
  const visit = (current) => {
    for (const entry of readdirSync(current)) {
      const abs = path.join(current, entry);
      if (statSync(abs).isDirectory()) {
        if (skipNamed.has(entry)) continue;
        visit(abs);
      } else {
        out.push(abs);
      }
    }
  };
  visit(dir);
  return out;
}

function looksBinary(buf) {
  const len = Math.min(buf.length, 8000);
  for (let i = 0; i < len; i++) {
    if (buf[i] === 0) return true;
  }
  return false;
}

// --- Manifest checks ---

function checkManifest() {
  const manifestPath = path.join(DIST, 'manifest.json');
  if (!existsSync(manifestPath)) {
    problem('manifest.json is missing from the build output.');
    return null;
  }
  let manifest;
  try {
    manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));
  } catch (e) {
    problem(`manifest.json is not valid JSON: ${e.message}`);
    return null;
  }

  if (manifest.manifest_version !== 3) {
    problem(`manifest_version must be 3 (found ${JSON.stringify(manifest.manifest_version)}).`);
  }

  const minChrome = parseInt(String(manifest.minimum_chrome_version ?? ''), 10);
  if (!Number.isFinite(minChrome) || minChrome < MIN_CHROME) {
    problem(`minimum_chrome_version must be >= ${MIN_CHROME} (found ${JSON.stringify(manifest.minimum_chrome_version)}).`);
  }

  const perms = Array.isArray(manifest.permissions) ? [...manifest.permissions].sort() : [];
  const approved = [...APPROVED_PERMISSIONS].sort();
  if (JSON.stringify(perms) !== JSON.stringify(approved)) {
    problem(`permissions must equal exactly ${JSON.stringify(approved)} (found ${JSON.stringify(manifest.permissions)}).`);
  }

  if ('host_permissions' in manifest) {
    problem('host_permissions must not be declared.');
  }
  if ('optional_host_permissions' in manifest) {
    problem('optional_host_permissions must not be declared.');
  }

  const everyPermission = [
    ...(manifest.permissions ?? []),
    ...(manifest.optional_permissions ?? []),
  ];
  if (everyPermission.includes('debugger')) {
    problem('the "debugger" permission must not be requested.');
  }

  // Every manifest-referenced file must exist in the build output.
  const referenced = [];
  if (manifest.background?.service_worker) referenced.push(manifest.background.service_worker);
  if (manifest.side_panel?.default_path) referenced.push(manifest.side_panel.default_path);
  if (manifest.action?.default_popup) referenced.push(manifest.action.default_popup);
  const di = manifest.action?.default_icon;
  if (typeof di === 'string') referenced.push(di);
  else if (di && typeof di === 'object') referenced.push(...Object.values(di));
  if (manifest.icons && typeof manifest.icons === 'object') {
    referenced.push(...Object.values(manifest.icons));
  }
  for (const war of manifest.web_accessible_resources ?? []) {
    for (const res of war.resources ?? []) referenced.push(res);
  }
  for (const rel of referenced) {
    if (typeof rel === 'string' && !existsSync(path.join(DIST, rel))) {
      problem(`manifest references "${rel}", which is missing from the build output.`);
    }
  }

  return manifest;
}

// --- Built output checks ---

function checkBuildOutput() {
  if (!existsSync(DIST)) {
    problem('dist/browser-harness does not exist. Run `npm run build` first.');
    return;
  }
  const files = walkFiles(DIST);

  for (const abs of files) {
    const rel = path.relative(DIST, abs);
    const ext = path.extname(abs).toLowerCase();
    const base = path.basename(abs);

    if (['.ts', '.mts', '.cts'].includes(ext)) {
      problem(`TypeScript source must not be shipped: ${rel}`);
    }
    if (ext === '.map') {
      problem(`source map must not be shipped: ${rel}`);
    }
    if (SECRET_PATTERNS.some((p) => p.test(base))) {
      problem(`secret-looking file in build output: ${rel}`);
    }

    if (ext === '.js') {
      const text = readFileSync(abs, 'utf8');
      if (/\/\/[#@]\s*sourceMappingURL=/.test(text)) {
        problem(`source map reference present in ${rel}.`);
      }
      if (/\bfrom\s*['"][^'"]+\.ts['"]/.test(text) || /\bimport\(\s*['"][^'"]+\.ts['"]/.test(text)) {
        problem(`unrewritten .ts import specifier in ${rel}.`);
      }
      if (/\bfrom\s*['"]https?:\/\//.test(text) || /\bimport\(\s*['"]https?:\/\//.test(text)) {
        problem(`remotely hosted script import in ${rel}.`);
      }
    }

    if (ext === '.html') {
      const text = readFileSync(abs, 'utf8');
      const scriptTags = text.match(/<script\b[^>]*>/gi) ?? [];
      for (const tag of scriptTags) {
        if (!/\bsrc\s*=/.test(tag)) {
          problem(`inline <script> found in ${rel}.`);
        }
        if (/\bsrc\s*=\s*['"]https?:\/\//i.test(tag)) {
          problem(`remotely hosted <script src> in ${rel}.`);
        }
      }
    }
  }
}

// --- Tracked source checks ---

function checkProjectNames() {
  const files = walkFiles(ROOT, SKIP_DIRS);
  for (const abs of files) {
    const rel = path.relative(ROOT, abs);
    let buf;
    try {
      buf = readFileSync(abs);
    } catch {
      continue;
    }
    if (looksBinary(buf)) continue;
    const lower = buf.toString('utf8').toLowerCase();
    for (const needle of NAME_NEEDLES) {
      if (lower.includes(needle)) {
        problem(`prohibited development-assistant name found in tracked file: ${rel}`);
        break;
      }
    }
  }
}

function main() {
  checkManifest();
  checkBuildOutput();
  checkProjectNames();

  if (problems.length > 0) {
    console.error('validate: FAILED');
    for (const p of problems) console.error(`  - ${p}`);
    process.exit(1);
  }
  console.log('validate: OK - all extension constraints satisfied.');
}

main();
