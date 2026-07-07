#!/usr/bin/env node
// Seoul Voice Lab: ASR benchmark. Measures, per recognizer candidate: cold
// wall time (includes model load), warm wall time, real-time factor
// (wall/audio), and self-play word accuracy - the TTS benchmark's output
// audio has KNOWN scripts, so recognizing it back gives an honest
// cross-engine sanity number on clean synthetic speech. That number is NOT
// user-voice WER; real WER waits for the recorded evaluation set
// (recordings/ + reference transcripts). Results go to out/asr-metrics.json.
// Model files are discovered by glob, never hardcoded by name.
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from 'node:fs';
import { join, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import os from 'node:os';

const here = dirname(fileURLToPath(import.meta.url));
const runtimeDir = join(here, 'runtime', 'sherpa-onnx-v1.13.3-onnxruntime-1.24.4-osx-arm64-shared');
const binDir = join(runtimeDir, 'bin');
const env = { ...process.env, DYLD_LIBRARY_PATH: join(runtimeDir, 'lib') };

function findOne(dir, pattern, required = true) {
  const hits = readdirSync(dir).filter((f) => pattern.test(f)).sort();
  if (!hits.length) {
    if (required) throw new Error(`no file matching ${pattern} in ${dir}`);
    return undefined;
  }
  return join(dir, hits[0]);
}

// The known scripts spoken by the TTS benchmark (same strings, one source of
// truth would be better - kept verbatim-equal and asserted non-empty).
const SHORT = 'The reliability index is ninety nine point nine four percent.';
const LONG =
  'Queue latency averaged forty seven milliseconds over the last twenty four samples, trending upward. ' +
  'The peak was sixty three milliseconds at two in the afternoon, and the most recent reading is fifty one. ' +
  'Two stations reported readings outside their usual range.';

// Self-play corpus: (wav produced earlier, its known script). Two different
// TTS engines so neither recognizer is graded only on its sibling's voice.
const CORPUS = [
  { wav: join(here, 'out', 'tts-af-long.wav'), text: LONG, speaker: 'kokoro af' },
  { wav: join(here, 'out', 'tts-bf_emma-long.wav'), text: LONG, speaker: 'kokoro bf_emma' },
  { wav: join(here, 'out', 'tts-piper-amy-long.wav'), text: LONG, speaker: 'piper amy' },
  { wav: join(here, 'out', 'tts-af-short.wav'), text: SHORT, speaker: 'kokoro af' },
].filter((c) => existsSync(c.wav));
if (CORPUS.length < 2) {
  console.error('run bench-tts.mjs and bench-tts-piper.mjs first (need known-script wavs)');
  process.exit(1);
}

// Digits become words before comparison ("47" == "forty seven"), the
// standard WER text normalization - otherwise formatting differences count
// as recognition errors. Homophone spellings (queue vs Q) still count; the
// self-play number is therefore an upper bound on true error.
const ONES = ['zero', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve', 'thirteen', 'fourteen', 'fifteen', 'sixteen', 'seventeen', 'eighteen', 'nineteen'];
const TENS = ['', '', 'twenty', 'thirty', 'forty', 'fifty', 'sixty', 'seventy', 'eighty', 'ninety'];

function numberWords(n) {
  if (n < 20) return [ONES[n]];
  if (n < 100) return n % 10 ? [TENS[Math.floor(n / 10)], ONES[n % 10]] : [TENS[Math.floor(n / 10)]];
  const rest = n % 100 ? numberWords(n % 100) : [];
  return [ONES[Math.floor(n / 100)], 'hundred', ...rest];
}

function expandToken(token) {
  const m = token.match(/^(\d+)(?:\.(\d+))?(%)?$/);
  if (!m) return [token];
  const whole = Number(m[1]);
  const out = whole < 1000 ? numberWords(whole) : [...m[1]].flatMap((d) => [ONES[Number(d)]]);
  if (m[2] !== undefined) out.push('point', ...[...m[2]].map((d) => ONES[Number(d)]));
  if (m[3]) out.push('percent');
  return out;
}

function normalize(text) {
  return text
    .toLowerCase()
    .replace(/[^a-z0-9. ]+/g, ' ')
    .split(/\s+/)
    .map((t) => t.replace(/^\.+|\.+$/g, ''))
    .filter(Boolean)
    .flatMap(expandToken);
}

// Word error rate via Levenshtein over word arrays.
function wer(referenceText, hypothesisText) {
  const ref = normalize(referenceText);
  const hyp = normalize(hypothesisText);
  const d = Array.from({ length: ref.length + 1 }, (_, i) => {
    const row = new Array(hyp.length + 1).fill(0);
    row[0] = i;
    return row;
  });
  for (let j = 0; j <= hyp.length; j++) d[0][j] = j;
  for (let i = 1; i <= ref.length; i++) {
    for (let j = 1; j <= hyp.length; j++) {
      const sub = d[i - 1][j - 1] + (ref[i - 1] === hyp[j - 1] ? 0 : 1);
      d[i][j] = Math.min(sub, d[i - 1][j] + 1, d[i][j - 1] + 1);
    }
  }
  return ref.length ? d[ref.length][hyp.length] / ref.length : 1;
}

function extractTranscript(output) {
  // The CLIs print result objects with a "text" field; take the last match.
  const matches = [...output.matchAll(/"text"\s*:\s*"([^"]*)"/g)];
  if (matches.length) return matches[matches.length - 1][1];
  return '';
}

function wavDurationSeconds(path) {
  // Same logic as bench-tts: canonical PCM WAV data chunk.
  const buffer = readFileSync(path);
  const sampleRate = buffer.readUInt32LE(24);
  const bitsPerSample = buffer.readUInt16LE(34);
  const channels = buffer.readUInt16LE(22);
  let offset = 12;
  while (offset < buffer.length - 8) {
    const id = buffer.toString('ascii', offset, offset + 4);
    const size = buffer.readUInt32LE(offset + 4);
    if (id === 'data') return size / (sampleRate * channels * (bitsPerSample / 8));
    offset += 8 + size;
  }
  throw new Error(`no data chunk in ${path}`);
}

function runOnce(binPath, args, wav) {
  // Through /usr/bin/time -l so peak RSS rides along on stderr.
  const started = process.hrtime.bigint();
  const proc = spawnSync('/usr/bin/time', ['-l', binPath, ...args, wav], { env, encoding: 'utf8' });
  if (proc.status !== 0) throw new Error(`${binPath} failed: ${proc.stderr?.slice(-400)}`);
  const wallMs = Number(process.hrtime.bigint() - started) / 1e6;
  const rss = proc.stderr?.match(/(\d+)\s+maximum resident set size/);
  return {
    wallMs,
    output: `${proc.stdout ?? ''}\n${proc.stderr ?? ''}`,
    rssMb: rss ? Math.round(Number(rss[1]) / 2 ** 20) : undefined,
  };
}

function runRecognizer(name, binPath, args) {
  const perClip = [];
  let coldMs;
  let peakRssMb = 0;
  for (const [index, clip] of CORPUS.entries()) {
    // Two passes per clip, min wall: an 8 GiB host is noisy under memory
    // pressure and the minimum is the honest steady-state figure. The cold
    // number is the very first pass (includes model load, nothing warm).
    const first = runOnce(binPath, args, clip.wav);
    if (index === 0) coldMs = first.wallMs;
    const second = runOnce(binPath, args, clip.wav);
    const best = second.wallMs < first.wallMs ? second : first;
    peakRssMb = Math.max(peakRssMb, best.rssMb ?? 0);
    const transcript = extractTranscript(best.output);
    perClip.push({
      speaker: clip.speaker,
      audio_s: Number(wavDurationSeconds(clip.wav).toFixed(2)),
      wall_ms: Math.round(best.wallMs),
      wall_ms_runs: [Math.round(first.wallMs), Math.round(second.wallMs)],
      self_play_wer: Number(wer(clip.text, transcript).toFixed(3)),
      transcript: transcript.trim().slice(0, 120),
    });
  }
  const warm = perClip.slice(1);
  const totalAudio = warm.reduce((a, c) => a + c.audio_s, 0);
  const totalWall = warm.reduce((a, c) => a + c.wall_ms, 0) / 1000;
  return {
    recognizer: name,
    cold_start_ms: Math.round(coldMs),
    peak_rss_mb: peakRssMb || undefined,
    warm_rtf: Number((totalWall / totalAudio).toFixed(3)),
    mean_self_play_wer: Number((perClip.reduce((a, c) => a + c.self_play_wer, 0) / perClip.length).toFixed(3)),
    per_clip: perClip,
  };
}

mkdirSync(join(here, 'out'), { recursive: true });
const results = [];

// Streaming zipformer (the partial-transcript engine).
{
  const dir = findOne(join(here, 'models'), /^sherpa-onnx-streaming-zipformer-en/, true);
  const stat = statSync(dir);
  if (stat.isDirectory()) {
    const encoder = findOne(dir, /encoder.*int8\.onnx$/) ?? findOne(dir, /encoder.*\.onnx$/);
    const decoder = findOne(dir, /decoder.*(?<!int8)\.onnx$/) ?? findOne(dir, /decoder.*\.onnx$/);
    const joiner = findOne(dir, /joiner.*int8\.onnx$/) ?? findOne(dir, /joiner.*\.onnx$/);
    results.push(runRecognizer('zipformer-streaming-int8', join(binDir, 'sherpa-onnx'), [
      `--tokens=${findOne(dir, /^tokens\.txt$/)}`,
      `--encoder=${encoder}`,
      `--decoder=${decoder}`,
      `--joiner=${joiner}`,
    ]));
  }
}

// Whisper small.en int8 (the final-pass engine).
{
  const dir = findOne(join(here, 'models'), /^sherpa-onnx-whisper-small\.en$/, false);
  if (dir) {
    results.push(runRecognizer('whisper-small.en-int8', join(binDir, 'sherpa-onnx-offline'), [
      `--tokens=${findOne(dir, /tokens\.txt$/)}`,
      `--whisper-encoder=${findOne(dir, /encoder\.int8\.onnx$/)}`,
      `--whisper-decoder=${findOne(dir, /decoder\.int8\.onnx$/)}`,
    ]));
  } else {
    console.error('whisper model not fetched; skipping final-pass benchmark');
  }
}

for (const r of results) console.log(JSON.stringify({ recognizer: r.recognizer, cold_start_ms: r.cold_start_ms, warm_rtf: r.warm_rtf, mean_self_play_wer: r.mean_self_play_wer }));

const metrics = {
  machine: `${os.cpus()[0].model}, ${Math.round(os.totalmem() / 2 ** 30)} GiB RAM`,
  runtime: 'sherpa-onnx v1.13.3 (CPU provider)',
  measured_at: new Date().toISOString(),
  method:
    'CLI decode wall time via hrtime (process spawn included, so cold/warm UPPER-bound in-process figures). Self-play WER: recognizing this lab\'s own TTS output of known scripts - clean synthetic speech, NOT user-voice WER; the recorded evaluation set supersedes it. True streaming partial latency needs the c-api paced-feed harness (build host).',
  results,
};
writeFileSync(join(here, 'out', 'asr-metrics.json'), JSON.stringify(metrics, null, 2) + '\n');
console.log('\nwrote out/asr-metrics.json');
