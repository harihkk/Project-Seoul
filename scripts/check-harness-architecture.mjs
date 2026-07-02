#!/usr/bin/env node
// Project Seoul Development Harness - architecture gate.
//
// Enforces the engineering-quality rules that this refactor established, so the
// repaired defects cannot silently return. It is not a line-count limit (which
// developers game); it targets specific structural regressions:
//   1. a growing action-kind dispatch chain (executeAction if/else on kind);
//   2. a default/else branch that reinterprets an unknown action as a concrete
//      content action;
//   3. module-global mutable state in the background (the lastActionTab bug);
//   4. duplicated safety constants across source files;
//   5. keep-in-sync mirror comments.
//
// Scans only the harness TypeScript sources; dist output and tests are excluded.

import { readdirSync, readFileSync, statSync } from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const ROOT = process.cwd();
const SRC = path.join(ROOT, 'apps', 'browser-harness', 'src');
const BACKGROUND_DIR = path.join(SRC, 'background');

const ACTION_KINDS = ['NAVIGATE', 'CLICK_ELEMENT', 'TYPE_TEXT', 'SCROLL_PAGE'];
const CONTENT_ACTION_VERBS = ['SEOUL_CLICK', 'SEOUL_TYPE', 'SEOUL_SCROLL'];
// Named safety constants that must have exactly one definition (validation.ts).
const SAFETY_CONSTANTS = [
  'MAX_INTERACTIVE_ELEMENTS',
  'MAX_TEXT_BLOCKS',
  'MAX_TOTAL_TEXT_CHARS',
  'MAX_TEXT_BLOCK_CHARS',
  'MAX_TYPE_TEXT_CHARS',
  'SCROLL_MAX',
  'SCROLL_DEFAULT',
];

function walk(dir) {
  const out = [];
  for (const entry of readdirSync(dir)) {
    const abs = path.join(dir, entry);
    if (statSync(abs).isDirectory()) {
      out.push(...walk(abs));
    } else if (entry.endsWith('.ts')) {
      out.push(abs);
    }
  }
  return out;
}

const files = walk(SRC);
const violations = [];
function flag(file, message) {
  violations.push(`${path.relative(ROOT, file)}: ${message}`);
}

for (const file of files) {
  const rel = path.relative(SRC, file);
  const text = readFileSync(file, 'utf8');
  const base = path.basename(file);
  const inBackground =
    file.startsWith(BACKGROUND_DIR + path.sep) || rel === 'background.ts';

  // Rule 1: action-kind dispatch chain. Comparing an action kind against a
  // literal 3+ times in one file is the executeAction if/else pattern. The
  // typed registry (action-handlers.ts) is the one legitimate place that names
  // kinds; validation.ts parses with a switch on `case`, not `=== 'KIND'`.
  if (base !== 'action-handlers.ts') {
    let comparisons = 0;
    for (const kind of ACTION_KINDS) {
      const matches = text.match(new RegExp(`===\\s*'${kind}'`, 'g'));
      if (matches) comparisons += matches.length;
    }
    if (comparisons >= 3) {
      flag(file, `action-kind dispatch chain detected (${comparisons} kind comparisons); dispatch through the typed handler registry instead`);
    }
  }

  // Rule 2: a default/else branch that reinterprets an unknown action as a
  // concrete content action (the "else => scroll" bug). Bounded windows keep
  // the match local to the branch.
  for (const verb of CONTENT_ACTION_VERBS) {
    if (new RegExp(`\\belse\\s*\\{[\\s\\S]{0,240}?${verb}`).test(text)) {
      flag(file, `an unconditional else dispatches ${verb}; unknown actions must be rejected explicitly, not reinterpreted`);
    }
    if (new RegExp(`\\bdefault\\s*:[\\s\\S]{0,200}?${verb}`).test(text)) {
      flag(file, `a switch default reaches ${verb}; the default must reject unsupported requests`);
    }
  }

  // Rule 3: module-global mutable state in the background. A module-scope
  // (column 0) let/var is the recoverable-state-lost-on-restart bug; background
  // state must be a scoped store (const class instance) or storage-backed.
  if (inBackground) {
    const lines = text.split('\n');
    lines.forEach((line, i) => {
      if (/^(let|var)\s+\w+/.test(line)) {
        flag(file, `line ${i + 1}: module-global mutable state ('${line.trim()}'); use a scoped store or chrome.storage`);
      }
    });
  }

  // Rule 5: keep-in-sync / deliberate-mirror comments.
  if (/keep\s+(?:them|these|this|it)?\s*in\s+sync/i.test(text) ||
      /deliberate\s+(?:local\s+)?mirror/i.test(text)) {
    flag(file, 'keep-in-sync / mirror comment; share one source of truth instead');
  }
}

// Rule 4: duplicated safety constants. A named safety constant defined in more
// than one source file is a hand-maintained duplicate.
for (const name of SAFETY_CONSTANTS) {
  const definers = files.filter((file) => {
    const text = readFileSync(file, 'utf8');
    return new RegExp(`\\b${name}\\s*:\\s*[0-9_]+`).test(text) ||
      new RegExp(`\\b${name}\\s*=\\s*[0-9_]+`).test(text);
  });
  if (definers.length > 1) {
    flag(
      definers[definers.length - 1],
      `safety constant ${name} is defined in ${definers.length} files (${definers
        .map((f) => path.basename(f))
        .join(', ')}); define it once and import it`,
    );
  }
}

if (violations.length > 0) {
  console.error('harness-architecture: FAILED');
  for (const v of violations) console.error(`  - ${v}`);
  process.exit(1);
}
console.log('harness-architecture: OK (no dispatch-chain, reinterpreting-default, module-global, duplicate-constant, or mirror-comment regressions)');
