#!/usr/bin/env node
// Project Seoul Development Harness - build script.
//
// The background service worker is an ES module (manifest type: module), so tsc
// emits it and its ./background/* modules as separate files that import each
// other at runtime - no bundling needed. The content script is injected as a
// classic script and cannot use runtime ES module imports, so its split source
// modules (src/content/*) are bundled by esbuild into a single content.js. This
// bundling is what lets the content script consume the shared validation source
// of truth directly instead of mirroring it.
//
// Steps: recreate the output dir, run tsc (type-check + emit the module graph),
// bundle the content entry with esbuild, drop the redundant per-module content
// output, copy static assets, then fail if an expected file is missing or an
// unexpected executable artifact slipped in.

import { spawnSync } from 'node:child_process';
import {
  existsSync,
  mkdirSync,
  copyFileSync,
  rmSync,
  readdirSync,
  readFileSync,
  statSync,
} from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { buildSync } from 'esbuild';

const ROOT = process.cwd();
const SRC = path.join(ROOT, 'apps', 'browser-harness');
const DIST_ROOT = path.join(ROOT, 'dist');
const DIST = path.join(DIST_ROOT, 'browser-harness');
const CONTENT_ENTRY = path.join(SRC, 'src', 'content', 'entry.ts');
const CONTENT_OUT = path.join(DIST, 'content.js');

const ALLOWED_OUTPUT_EXTENSIONS = new Set(['.js', '.html', '.css', '.json', '.png', '.svg']);

const EXPECTED_FILES = [
  'manifest.json',
  'background.js',
  'content.js',
  'protocol.js',
  'validation.js',
  'control-session-machine.js',
  'control-session-store.js',
  'session.js',
  'sidepanel/index.html',
  'sidepanel/index.css',
  'sidepanel/index.js',
];

function fail(message) {
  console.error(`build: ERROR - ${message}`);
  process.exit(1);
}

function cleanOnly() {
  rmSync(DIST_ROOT, { recursive: true, force: true });
  console.log('build: removed dist/');
}

function compileTypeScript() {
  const tscEntry = path.join(ROOT, 'node_modules', 'typescript', 'bin', 'tsc');
  if (!existsSync(tscEntry)) {
    fail('TypeScript compiler not found. Run `npm install` first.');
  }
  const result = spawnSync(process.execPath, [tscEntry, '-p', 'tsconfig.json'], {
    cwd: ROOT,
    stdio: 'inherit',
  });
  if (result.error) fail(`failed to launch tsc: ${result.error.message}`);
  if (result.status !== 0) fail(`tsc exited with status ${result.status}`);
}

// Bundle the content-script module graph (src/content/* plus the shared
// validation/protocol sources it imports) into a single classic script. IIFE
// format with no imports/exports, no minification (deterministic, reviewable
// output), targeting the manifest's minimum Chrome version. esbuild output is
// deterministic for a fixed input and pinned version, so two builds produce an
// identical content.js.
function bundleContentScript() {
  if (!existsSync(CONTENT_ENTRY)) fail(`missing content entry: ${CONTENT_ENTRY}`);
  const result = buildSync({
    entryPoints: [CONTENT_ENTRY],
    outfile: CONTENT_OUT,
    bundle: true,
    format: 'iife',
    platform: 'browser',
    target: ['chrome116'],
    minify: false,
    sourcemap: false,
    legalComments: 'none',
    logLevel: 'silent',
  });
  if (result.errors && result.errors.length > 0) {
    fail(`esbuild failed to bundle the content script:\n${JSON.stringify(result.errors)}`);
  }
  // The redundant per-module tsc output for the content graph is not used at
  // runtime (the bundle is authoritative); drop it so dist has one content
  // artifact and cannot drift from the tested source.
  rmSync(path.join(DIST, 'content'), { recursive: true, force: true });

  // Consistency guard: the bundle must actually contain the shared field-safety
  // implementation, not a divergent copy. This exact message is defined only in
  // src/validation.ts (classifyTypeTarget); its presence in the built artifact
  // proves the content script uses the shared source.
  const SHARED_SAFETY_MARKER = 'Refusing to type into a password field.';
  const bundled = readFileSync(CONTENT_OUT, 'utf8');
  if (!bundled.includes(SHARED_SAFETY_MARKER)) {
    fail('content.js does not contain the shared validation implementation; the content build has diverged from validation.ts.');
  }
}

function copyInto(relSource, relDest) {
  const from = path.join(SRC, relSource);
  const to = path.join(DIST, relDest);
  if (!existsSync(from)) fail(`missing source file: ${relSource}`);
  mkdirSync(path.dirname(to), { recursive: true });
  copyFileSync(from, to);
}

function copyStaticAssets() {
  const staticDir = path.join(SRC, 'static');
  if (!existsSync(staticDir)) return;
  const walk = (dir, rel) => {
    for (const entry of readdirSync(dir)) {
      if (entry.startsWith('.')) continue; // skip .gitkeep and dotfiles
      const abs = path.join(dir, entry);
      const relPath = path.join(rel, entry);
      if (statSync(abs).isDirectory()) {
        walk(abs, relPath);
      } else {
        const dest = path.join(DIST, 'static', relPath);
        mkdirSync(path.dirname(dest), { recursive: true });
        copyFileSync(abs, dest);
      }
    }
  };
  walk(staticDir, '');
}

function verifyExpectedFiles() {
  const missing = EXPECTED_FILES.filter((rel) => !existsSync(path.join(DIST, rel)));
  if (missing.length > 0) {
    fail(`expected output files are missing:\n  - ${missing.join('\n  - ')}`);
  }
}

function verifyNoUnexpectedArtifacts() {
  const offenders = [];
  const walk = (dir) => {
    for (const entry of readdirSync(dir)) {
      const abs = path.join(dir, entry);
      if (statSync(abs).isDirectory()) {
        walk(abs);
        continue;
      }
      const ext = path.extname(entry).toLowerCase();
      if (!ALLOWED_OUTPUT_EXTENSIONS.has(ext)) {
        offenders.push(path.relative(DIST, abs));
      }
    }
  };
  walk(DIST);
  if (offenders.length > 0) {
    fail(`unexpected artifacts copied into the build:\n  - ${offenders.join('\n  - ')}`);
  }
}

function main() {
  if (process.argv.includes('--clean-only')) {
    cleanOnly();
    return;
  }

  rmSync(DIST, { recursive: true, force: true });
  mkdirSync(DIST, { recursive: true });

  compileTypeScript();
  bundleContentScript();

  copyInto('manifest.json', 'manifest.json');
  copyInto('src/sidepanel/index.html', 'sidepanel/index.html');
  copyInto('src/sidepanel/index.css', 'sidepanel/index.css');
  copyStaticAssets();

  verifyExpectedFiles();
  verifyNoUnexpectedArtifacts();

  console.log(`build: OK - unpacked extension written to ${path.relative(ROOT, DIST)}`);
}

main();
