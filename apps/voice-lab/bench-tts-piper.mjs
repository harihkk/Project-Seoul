#!/usr/bin/env node
// Piper VITS benchmark: same protocol as bench-tts.mjs (cold, warm-short
// first-audio upper bound, long-summary RTF) so the two candidates compare
// apples to apples. Appends into out/tts-metrics.json.
import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const runtimeDir = join(here, 'runtime', 'sherpa-onnx-v1.13.3-onnxruntime-1.24.4-osx-arm64-shared');
const bin = join(runtimeDir, 'bin', 'sherpa-onnx-offline-tts');
const modelDir = join(here, 'models', 'vits-piper-en_US-amy-medium');
if (!existsSync(modelDir)) { console.error('fetch piper first'); process.exit(1); }

const SHORT = 'The reliability index is ninety nine point nine four percent.';
const LONG =
  'Queue latency averaged forty seven milliseconds over the last twenty four samples, trending upward. ' +
  'The peak was sixty three milliseconds at two in the afternoon, and the most recent reading is fifty one. ' +
  'Two stations reported readings outside their usual range.';

function wavDurationSeconds(path) {
  const b = readFileSync(path);
  const sampleRate = b.readUInt32LE(24);
  const bits = b.readUInt16LE(34);
  const channels = b.readUInt16LE(22);
  let off = 12;
  while (off < b.length - 8) {
    const id = b.toString('ascii', off, off + 4);
    const size = b.readUInt32LE(off + 4);
    if (id === 'data') return size / (sampleRate * channels * (bits / 8));
    off += 8 + size;
  }
  throw new Error('no data chunk');
}

function synth(text, out) {
  const t0 = process.hrtime.bigint();
  execFileSync(bin, [
    `--vits-model=${join(modelDir, 'en_US-amy-medium.onnx')}`,
    `--vits-tokens=${join(modelDir, 'tokens.txt')}`,
    `--vits-data-dir=${join(modelDir, 'espeak-ng-data')}`,
    `--output-filename=${out}`,
    text,
  ], { stdio: 'pipe', env: { ...process.env, DYLD_LIBRARY_PATH: join(runtimeDir, 'lib') } });
  return { wallMs: Number(process.hrtime.bigint() - t0) / 1e6, audioSeconds: wavDurationSeconds(out) };
}

const cold = synth(SHORT, join(here, 'out', 'tts-piper-amy-cold.wav'));
const warm = synth(SHORT, join(here, 'out', 'tts-piper-amy-short.wav'));
const long = synth(LONG, join(here, 'out', 'tts-piper-amy-long.wav'));
const entry = {
  voice: 'piper_en_US_amy_medium',
  sex: 'female',
  cold_start_ms: Math.round(cold.wallMs),
  first_audio_upper_bound_ms: Math.round(warm.wallMs),
  long_wall_ms: Math.round(long.wallMs),
  long_audio_s: Number(long.audioSeconds.toFixed(2)),
  rtf: Number((long.wallMs / 1000 / long.audioSeconds).toFixed(3)),
};
console.log(JSON.stringify(entry));
const metricsPath = join(here, 'out', 'tts-metrics.json');
const metrics = JSON.parse(readFileSync(metricsPath, 'utf8'));
metrics.results.push(entry);
metrics.model += ' + vits-piper-en_US-amy-medium';
writeFileSync(metricsPath, JSON.stringify(metrics, null, 2) + '\n');
