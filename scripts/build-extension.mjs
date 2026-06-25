#!/usr/bin/env node
// Project Seoul Development Harness - build script.
//
// Uses only the TypeScript compiler and the Node standard library (no bundler).
// It removes and recreates the output directory, compiles the sources, copies
// the static manifest/HTML/CSS, then fails if an expected file is missing or an
// unexpected executable artifact slipped into the build.

import { spawnSync } from 'node:child_process';
import {
  existsSync,
  mkdirSync,
  copyFileSync,
  rmSync,
  readdirSync,
  statSync,
} from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const ROOT = process.cwd();
const SRC = path.join(ROOT, 'apps', 'browser-harness');
const DIST_ROOT = path.join(ROOT, 'dist');
const DIST = path.join(DIST_ROOT, 'browser-harness');

const ALLOWED_OUTPUT_EXTENSIONS = new Set(['.js', '.html', '.css', '.json', '.png', '.svg']);

const EXPECTED_FILES = [
  'manifest.json',
  'background.js',
  'content.js',
  'protocol.js',
  'validation.js',
  'task-machine.js',
  'task-store.js',
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

  copyInto('manifest.json', 'manifest.json');
  copyInto('src/sidepanel/index.html', 'sidepanel/index.html');
  copyInto('src/sidepanel/index.css', 'sidepanel/index.css');
  copyStaticAssets();

  verifyExpectedFiles();
  verifyNoUnexpectedArtifacts();

  console.log(`build: OK - unpacked extension written to ${path.relative(ROOT, DIST)}`);
}

main();
