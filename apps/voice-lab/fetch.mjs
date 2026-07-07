#!/usr/bin/env node
// Seoul Voice Lab fetcher. Downloads the pinned runtime and model candidates,
// records each artifact's sha256 into manifest.json on FIRST fetch, and hard
// fails on any later hash drift. Everything lands under apps/voice-lab/
// (gitignored artifacts); only the manifest (with recorded hashes) is source.
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, renameSync, writeFileSync } from 'node:fs';
import { join, dirname, basename } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const manifestPath = join(here, 'manifest.json');
const manifest = JSON.parse(readFileSync(manifestPath, 'utf8'));

function sha256(path) {
  const hash = createHash('sha256');
  hash.update(readFileSync(path));
  return hash.digest('hex');
}

function fetchArtifact(entry, destDir) {
  mkdirSync(destDir, { recursive: true });
  const file = join(destDir, basename(entry.url));
  if (!existsSync(file)) {
    // Atomic: download to .part, rename only on success. A killed transfer
    // must never leave a truncated file that later gets a wrong hash pinned.
    const part = `${file}.part`;
    console.log(`fetching ${entry.id ?? 'runtime'} ...`);
    execFileSync('curl', ['-L', '--fail', '--retry', '5', '--retry-all-errors', '-C', '-', '-o', part, entry.url], { stdio: 'inherit' });
    renameSync(part, file);
  }
  const digest = sha256(file);
  if (entry.sha256 === '') {
    entry.sha256 = digest;
    console.log(`pinned ${basename(file)} sha256=${digest}`);
  } else if (entry.sha256 !== digest) {
    throw new Error(`sha256 drift for ${basename(file)}: manifest ${entry.sha256}, got ${digest}`);
  } else {
    console.log(`verified ${basename(file)}`);
  }
  if (file.endsWith('.tar.bz2')) {
    const marker = file.replace(/\.tar\.bz2$/, '.extracted');
    if (!existsSync(marker)) {
      execFileSync('tar', ['xjf', file, '-C', destDir], { stdio: 'inherit' });
      writeFileSync(marker, digest);
    }
  }
  return file;
}

const only = process.argv[2]; // optional: fetch one id
fetchArtifact(manifest.runtime, join(here, 'runtime'));
for (const model of manifest.models) {
  if (only && model.id !== only) continue;
  fetchArtifact(model, join(here, 'models'));
}
writeFileSync(manifestPath, JSON.stringify(manifest, null, 2) + '\n');
console.log('fetch complete; hashes recorded in manifest.json');
