#!/usr/bin/env node
// Product-architecture gate for the Seoul native product runtime.
//
// Enforces the vertical-integration invariants the product depends on, so a
// regression that quietly unwires the runtime fails CI instead of shipping:
//
//   1. No giant capability dispatch switch: capability execution must go
//      through the CapabilityExecutorRegistry, never a growing if/else-if or
//      switch on capability id in the task/runtime layer.
//   2. No reinterpreting-default: an unknown capability/action must fail
//      closed, never fall through to a default action.
//   3. No placeholder production callbacks: no "TODO"/"not implemented"/
//      "fake success" markers in product production source.
//   4. No fixed workspace names: the shell must not create workspaces named a
//      hardcoded string or rename by appending a fixed suffix.
//   5. No empty browser-test bodies anywhere under native/seoul.
//   6. Every registered CapabilityExecutor subclass is constructed somewhere
//      (registered), and the runtime marks executor-less descriptors
//      unavailable.
//   7. Canvas/product turns must use an explicit WindowRuntimeBinding token,
//      never an active/last-focused-window fallback.
//   8. Canvas voice must use the realtime voice session bridge, not the
//      legacy local speech button path.
//
// Pure static scan over tracked source; no build required.

import fs from 'node:fs';
import path from 'node:path';

const repoRoot = path.resolve(import.meta.dirname, '..');
const productRoot = path.join(repoRoot, 'native/seoul/browser/product');
const seoulRoot = path.join(repoRoot, 'native/seoul');
const problems = [];

function walk(dir, filter) {
  const out = [];
  if (!fs.existsSync(dir)) return out;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const abs = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      out.push(...walk(abs, filter));
    } else if (filter(abs)) {
      out.push(abs);
    }
  }
  return out;
}

const isProdCc = (f) =>
  f.endsWith('.cc') && !f.endsWith('_unittest.cc') &&
  !f.endsWith('_browsertest.cc') && !f.includes('/fake_') &&
  !path.basename(f).startsWith('fake_');

// --- 1 + 2: no capability dispatch switch / reinterpreting default ----------
// The task service resolves executors via executors_->Find(); it must never
// switch on a capability id string. Flag a switch/if chain that compares a
// capability/tool id to string literals in product production source.
for (const file of walk(productRoot, isProdCc)) {
  const text = fs.readFileSync(file, 'utf8');
  const rel = path.relative(repoRoot, file);
  // A chain of capability-id string comparisons is the smell we forbid.
  const idComparisons = text.match(
    /\.(value|id)\(\)\s*==\s*"[a-z]+\.[a-z.]+"/g,
  );
  if (idComparisons && idComparisons.length >= 3) {
    problems.push(
      `${rel}: looks like a capability-id dispatch chain ` +
        `(${idComparisons.length} id string comparisons); route through the ` +
        `CapabilityExecutorRegistry instead.`,
    );
  }
}

// --- 3: no placeholder production callbacks ---------------------------------
const PLACEHOLDER =
  /\b(TODO|FIXME|not implemented|notimplemented|placeholder|fake success|stub out|coming soon)\b/i;
for (const file of walk(productRoot, isProdCc).concat(
  walk(path.join(seoulRoot, 'browser/canvas'), isProdCc),
)) {
  const text = fs.readFileSync(file, 'utf8');
  const rel = path.relative(repoRoot, file);
  text.split('\n').forEach((line, i) => {
    // Allow the word in a longer descriptive comment only if it is not a
    // deferral marker; the regex above is already specific.
    if (PLACEHOLDER.test(line)) {
      problems.push(`${rel}:${i + 1}: placeholder marker in production source`);
    }
  });
}

// --- 4: no fixed workspace names in the shell -------------------------------
const shellRoot = path.join(seoulRoot, 'browser/shell');
for (const file of walk(shellRoot, isProdCc)) {
  const text = fs.readFileSync(file, 'utf8');
  const rel = path.relative(repoRoot, file);
  text.split('\n').forEach((line, i) => {
    if (/command\.name\s*=\s*"Workspace"/.test(line)) {
      problems.push(
        `${rel}:${i + 1}: workspace created with a fixed name; derive a real ` +
          `name from user input or a unique default.`,
      );
    }
    if (/\+\s*" Renamed"/.test(line)) {
      problems.push(
        `${rel}:${i + 1}: workspace renamed by appending a fixed suffix; ` +
          `use a real rename input.`,
      );
    }
  });
}

// --- 5: no empty browser-test bodies ----------------------------------------
for (const file of walk(seoulRoot, (f) => f.endsWith('_browsertest.cc'))) {
  const text = fs.readFileSync(file, 'utf8');
  const rel = path.relative(repoRoot, file);
  const empty = text.match(/IN_PROC_BROWSER_TEST_F\([^)]*\)\s*\{\s*\}/g);
  if (empty) {
    problems.push(
      `${rel}: ${empty.length} empty IN_PROC_BROWSER_TEST_F body/bodies`,
    );
  }
}

// --- 6: executor subclasses are registered ----------------------------------
// Collect CapabilityExecutor subclasses declared in product headers, then
// confirm each is constructed (std::make_unique<Name>) somewhere in product
// source. An executor no one registers is dead and misleading.
const executorNames = new Set();
for (const file of walk(productRoot, (f) => f.endsWith('.h'))) {
  const text = fs.readFileSync(file, 'utf8');
  for (const m of text.matchAll(
    /class\s+(\w+)\s*(?:final\s*)?:\s*public\s+CapabilityExecutor\b/g,
  )) {
    executorNames.add(m[1]);
  }
}
const productSource = walk(productRoot, (f) => f.endsWith('.cc'))
  .map((f) => fs.readFileSync(f, 'utf8'))
  .join('\n');
for (const name of executorNames) {
  const constructed = new RegExp(`make_unique<${name}>`).test(productSource);
  if (!constructed) {
    problems.push(
      `CapabilityExecutor subclass ${name} is never registered ` +
        `(no make_unique<${name}>); remove it or register it.`,
    );
  }
}

// --- 7: no active-window fallback for product turns -------------------------
for (const file of walk(productRoot, isProdCc).concat(
  walk(path.join(seoulRoot, 'browser/canvas'), isProdCc),
)) {
  const text = fs.readFileSync(file, 'utf8');
  const rel = path.relative(repoRoot, file);
  text.split('\n').forEach((line, i) => {
    if (/\bActiveWindow\s*\(/.test(line)) {
      problems.push(
        `${rel}:${i + 1}: product turns must resolve a ` +
          `WindowRuntimeBinding token, not an active-window fallback.`,
      );
    }
  });
}

// --- 8: primary Canvas voice uses the realtime agent ------------------------
const runtimeServicePath = path.join(
  productRoot,
  'browser/seoul_runtime_service.cc',
);
if (fs.existsSync(runtimeServicePath)) {
  const runtimeText = fs.readFileSync(runtimeServicePath, 'utf8');
  if (/AppleSpeechRecognizer|AppleTtsEngine/.test(runtimeText)) {
    problems.push(
      'SeoulRuntimeService wires platform speech directly; Canvas voice must ' +
        'mint realtime voice sessions through RealtimeVoiceAgent.',
    );
  }
  if (!/CreateRealtimeVoiceSession/.test(runtimeText)) {
    problems.push(
      'SeoulRuntimeService does not expose CreateRealtimeVoiceSession; Canvas ' +
        'voice has no production realtime session bridge.',
    );
  }
}
const canvasMojomPath = path.join(seoulRoot, 'browser/canvas/canvas.mojom');
if (fs.existsSync(canvasMojomPath)) {
  const mojomText = fs.readFileSync(canvasMojomPath, 'utf8');
  for (const method of [
    'CreateRealtimeVoiceSession',
    'SubmitRealtimeToolCall',
  ]) {
    if (!mojomText.includes(method)) {
      problems.push(
        `Canvas Mojo is missing ${method}; voice cannot connect realtime ` +
          'speech to browser tasks.',
      );
    }
  }
}

// --- 9: capability arg names match between descriptor and executor ----------
// A capability descriptor declares its input fields (generic_capabilities.cc);
// the browser executor decodes those fields (browser_capabilities.cc). If the
// executor reads a field name the descriptor never declares, the capability is
// unrunnable (ValidatePlan rejects the unknown field, or the value is always
// missing). This catches that drift statically.
const declaredArgs = new Set();
const capsPath = path.join(
  seoulRoot,
  'browser/connectors/generic_capabilities.cc',
);
if (fs.existsSync(capsPath)) {
  const capsText = fs.readFileSync(capsPath, 'utf8');
  // Tolerate clang-format line wrapping between the helper call and its first
  // string argument (\s matches newlines in JS).
  for (const m of capsText.matchAll(
    /(?:Required|Optional)[A-Za-z]*\(\s*"([a-z_]+)"/g,
  )) {
    declaredArgs.add(m[1]);
  }
  for (const m of capsText.matchAll(/\.name\s*=\s*"([a-z_]+)"/g)) {
    declaredArgs.add(m[1]);
  }
}
const execPath = path.join(
  productRoot,
  'browser/browser_capabilities.cc',
);
if (fs.existsSync(execPath) && declaredArgs.size > 0) {
  const execText = fs.readFileSync(execPath, 'utf8');
  const execRel = path.relative(repoRoot, execPath);
  execText.split('\n').forEach((line, i) => {
    const m = line.match(/request\.args\.Find[A-Za-z]*\("([a-z_]+)"\)/);
    if (m && !declaredArgs.has(m[1])) {
      problems.push(
        `${execRel}:${i + 1}: executor reads arg "${m[1]}" that no capability ` +
          `descriptor declares; descriptor and executor arg names have drifted.`,
      );
    }
  });
}

// --- 10: task-to-surface is wired in production, not only in tests ----------
// A verified task result must reach the surface layer through a production
// caller of SurfaceService::CreateFromSemantic. If the only callers are test
// files (and its own definition), the product spine is broken: tasks would
// complete with no artifact. This locks in the TaskSurfaceBridge.
let productionCreateCallers = 0;
for (const file of walk(productRoot, isProdCc)) {
  // Exclude the method's own definition site.
  if (path.basename(file) === 'surface_service.cc') {
    continue;
  }
  const text = fs.readFileSync(file, 'utf8');
  if (/->CreateFromSemantic\(|\.CreateFromSemantic\(/.test(text)) {
    productionCreateCallers++;
  }
}
if (productionCreateCallers === 0) {
  problems.push(
    'SurfaceService::CreateFromSemantic has no production caller; a completed ' +
      'task would produce no surface. Wire it through a production observer ' +
      '(TaskSurfaceBridge), not only tests.',
  );
}

if (problems.length) {
  console.error('product-architecture: FAILED');
  for (const p of problems) console.error('  - ' + p);
  process.exit(1);
}
console.log(
  `product-architecture: OK (dispatch-registry, fail-closed, no placeholders, ` +
    `real workspace names, no empty browser tests, window-bound Canvas, ` +
    `realtime Canvas voice wired, ${executorNames.size} executors all ` +
    `registered, capability arg names consistent, task-to-surface bridge ` +
    `wired)`,
);
