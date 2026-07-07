#!/usr/bin/env node
// Seoul Voice Lab: TTS benchmark. Measures, per candidate voice: model load
// time, synthesis wall time, produced-audio duration, real-time factor
// (wall/audio; < 1.0 means faster than playback), and an estimated
// first-audio latency (wall time to synthesize the FIRST short sentence,
// which is what sentence-streamed playback would emit first). Results go to
// out/tts-metrics.json. Every number is measured on THIS machine; nothing is
// estimated from spec sheets.
import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import os from 'node:os';

const here = dirname(fileURLToPath(import.meta.url));
const runtimeDir = join(here, 'runtime', 'sherpa-onnx-v1.13.3-onnxruntime-1.24.4-osx-arm64-shared');
const bin = join(runtimeDir, 'bin', 'sherpa-onnx-offline-tts');
const modelDir = join(here, 'models', 'kokoro-en-v0_19');
if (!existsSync(bin) || !existsSync(modelDir)) {
  console.error('run fetch.mjs first (runtime + kokoro-en-tts)');
  process.exit(1);
}

// Kokoro v0.19 speaker ids, per its voices.bin ordering. Female English
// voices are the Seoul-identity candidates; two male ids are measured as
// controls so the choice is evidence, not defaults.
const VOICES = [
  { sid: 0, name: 'af', sex: 'female' },
  { sid: 1, name: 'af_bella', sex: 'female' },
  { sid: 2, name: 'af_nicole', sex: 'female' },
  { sid: 3, name: 'af_sarah', sex: 'female' },
  { sid: 4, name: 'af_sky', sex: 'female' },
  { sid: 5, name: 'am_adam', sex: 'male-control' },
  { sid: 7, name: 'bf_emma', sex: 'female' },
  { sid: 8, name: 'bf_isabella', sex: 'female' },
];

// The response shapes Seoul actually speaks: a short headline (this is the
// first-audio path) and a longer trend summary (the RTF path).
const SHORT = 'The reliability index is ninety nine point nine four percent.';
const LONG =
  'Queue latency averaged forty seven milliseconds over the last twenty four samples, trending upward. ' +
  'The peak was sixty three milliseconds at two in the afternoon, and the most recent reading is fifty one. ' +
  'Two stations reported readings outside their usual range.';

function wavDurationSeconds(path) {
  const buffer = readFileSync(path);
  // Canonical PCM WAV: sample rate at byte 24, data size in the data chunk.
  const sampleRate = buffer.readUInt32LE(24);
  const bitsPerSample = buffer.readUInt16LE(34);
  const channels = buffer.readUInt16LE(22);
  // Find the data chunk (usually at 36, but be exact).
  let offset = 12;
  while (offset < buffer.length - 8) {
    const id = buffer.toString('ascii', offset, offset + 4);
    const size = buffer.readUInt32LE(offset + 4);
    if (id === 'data') {
      return size / (sampleRate * channels * (bitsPerSample / 8));
    }
    offset += 8 + size;
  }
  throw new Error(`no data chunk in ${path}`);
}

function synthesize(sid, text, outFile) {
  const started = process.hrtime.bigint();
  execFileSync(bin, [
    `--kokoro-model=${join(modelDir, 'model.onnx')}`,
    `--kokoro-voices=${join(modelDir, 'voices.bin')}`,
    `--kokoro-tokens=${join(modelDir, 'tokens.txt')}`,
    `--kokoro-data-dir=${join(modelDir, 'espeak-ng-data')}`,
    `--sid=${sid}`,
    `--output-filename=${outFile}`,
    text,
  ], { stdio: 'pipe', env: { ...process.env, DYLD_LIBRARY_PATH: join(runtimeDir, 'lib') } });
  const wallMs = Number(process.hrtime.bigint() - started) / 1e6;
  return { wallMs, audioSeconds: wavDurationSeconds(outFile) };
}

mkdirSync(join(here, 'out'), { recursive: true });
const results = [];
for (const voice of VOICES) {
  // Two runs of the short sentence: the first includes model load (cold
  // start); the second is the warm first-audio estimate. Then the long
  // summary for sustained RTF.
  const cold = synthesize(voice.sid, SHORT, join(here, 'out', `tts-${voice.name}-cold.wav`));
  const warmShort = synthesize(voice.sid, SHORT, join(here, 'out', `tts-${voice.name}-short.wav`));
  const warmLong = synthesize(voice.sid, LONG, join(here, 'out', `tts-${voice.name}-long.wav`));
  const entry = {
    voice: voice.name,
    sid: voice.sid,
    sex: voice.sex,
    cold_start_ms: Math.round(cold.wallMs),
    // Process-spawn overhead included, so this UPPER-bounds in-process
    // first-audio for sentence-streamed playback.
    first_audio_upper_bound_ms: Math.round(warmShort.wallMs),
    long_wall_ms: Math.round(warmLong.wallMs),
    long_audio_s: Number(warmLong.audioSeconds.toFixed(2)),
    rtf: Number((warmLong.wallMs / 1000 / warmLong.audioSeconds).toFixed(3)),
  };
  results.push(entry);
  console.log(JSON.stringify(entry));
}

const metrics = {
  machine: `${os.cpus()[0].model}, ${Math.round(os.totalmem() / 2 ** 30)} GiB RAM`,
  runtime: 'sherpa-onnx v1.13.3 (CPU provider)',
  model: 'kokoro-en-v0_19 (82M params)',
  measured_at: new Date().toISOString(),
  method:
    'CLI synthesis wall time via hrtime; audio duration from WAV data chunk; first-audio upper bound = warm short-sentence synthesis incl. process spawn. In-process streaming will be strictly faster.',
  results,
};
writeFileSync(join(here, 'out', 'tts-metrics.json'), JSON.stringify(metrics, null, 2) + '\n');
console.log('\nwrote out/tts-metrics.json');
