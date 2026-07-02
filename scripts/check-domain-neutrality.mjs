#!/usr/bin/env node
// Domain-neutrality gate for the Seoul core runtime.
//
// The capability-driven architecture requires that core production modules
// contain no business-domain contracts and no phrase-routing. This check
// fails when a forbidden industry symbol or a keyword-routing pattern
// appears in core production source. Tests, fakes, test-support targets, and
// example/fixture packages are excluded: domain words are legitimate there
// as held-out evaluation data.
//
// If a term here produces a false positive on a genuinely generic concept,
// prefer renaming the symbol to the generic concept over weakening the list.

import fs from 'node:fs';
import path from 'node:path';

const repoRoot = path.resolve(import.meta.dirname, '..');
const coreRoot = path.join(repoRoot, 'native/seoul/browser');

// Word-boundary, case-insensitive industry terms forbidden in core
// production source (identifiers, strings, and comments alike).
const forbiddenTerms = [
  'weather', 'forecast', 'precipitation', 'humidity', 'sunrise', 'sunset',
  'stock', 'ticker', 'dividend', 'watchlist', 'portfolio',
  'merchant', 'shopping', 'seller', 'checkout_cart',
  'hotel', 'flight', 'airline', 'itinerary',
  'standings', 'league', 'election',
  'medical', 'diagnosis', 'prescription',
  'recipe',
];

// Keyword-routing patterns: goal text matched against a domain keyword to
// pick behavior. Any of these shapes in production is a routing table.
const routingPatterns = [
  /(contains|includes|find|StartsWith|EqualsCaseInsensitive)\s*\(\s*"[^"]*(weather|stock|hotel|flight|shop|amazon|price of)/i,
  /query\s*[=!]=\s*"(weather|stocks?|shopping|travel)"/i,
];

// Hardcoded consumer-website selectors/URLs in core production source.
const websitePattern =
  /https?:\/\/(www\.)?(amazon|google|facebook|booking|expedia|yahoo)\./i;

const excludedFile =
  /(_unittest\.cc|_browsertest\.cc|_test\.cc|fake_[a-z_]+\.(cc|h|mm))$/;
const excludedDir = /\/(examples?|fixtures?|test_support)\//;

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      out.push(...walk(full));
    } else if (entry.isFile() && /\.(cc|h|mm|mojom)$/.test(entry.name)) {
      out.push(full);
    }
  }
  return out;
}

const termRegexes = forbiddenTerms.map(
  (term) => new RegExp(`\\b${term}`, 'i'),
);

let violations = 0;
for (const file of walk(coreRoot)) {
  const rel = path.relative(repoRoot, file);
  if (excludedFile.test(rel) || excludedDir.test(`/${rel}/`)) {
    continue;
  }
  const lines = fs.readFileSync(file, 'utf8').split('\n');
  lines.forEach((line, index) => {
    for (let i = 0; i < termRegexes.length; i++) {
      if (termRegexes[i].test(line)) {
        console.error(
          `${rel}:${index + 1}: forbidden domain term "${forbiddenTerms[i]}"` +
            ` in core production source`,
        );
        violations++;
      }
    }
    for (const pattern of routingPatterns) {
      if (pattern.test(line)) {
        console.error(
          `${rel}:${index + 1}: keyword-routing pattern in core production ` +
            `source`,
        );
        violations++;
      }
    }
    if (websitePattern.test(line)) {
      console.error(
        `${rel}:${index + 1}: hardcoded consumer-website reference in core`,
      );
      violations++;
    }
  });
}

if (violations > 0) {
  console.error(`domain-neutrality: FAILED (${violations} violation(s))`);
  process.exit(1);
}
console.log('domain-neutrality: OK (core production source is domain-neutral)');
