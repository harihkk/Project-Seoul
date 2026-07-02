import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, existsSync, readdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Section C: the content script must consume the single shared safety source
// (validation.ts), not a hand-maintained mirror. Two guarantees:
//   1. Source-level: no content module redefines the shared limits, field
//      classifier, or snapshot-binding rule, and the actions module imports
//      them from ../validation.ts. (Checked always; no build needed.)
//   2. Build-level: when a build is present, the bundled content.js actually
//      contains the shared implementation. (The build script also enforces
//      this and fails without it.)

const dir = path.dirname(fileURLToPath(import.meta.url));
const contentDir = path.join(dir, '..', 'src', 'content');
const distContent = path.join(dir, '..', '..', '..', 'dist', 'browser-harness', 'content.js');

function contentSources() {
  return readdirSync(contentDir)
    .filter((f) => f.endsWith('.ts'))
    .map((f) => ({ name: f, text: readFileSync(path.join(contentDir, f), 'utf8') }));
}

test('no content module redefines the shared safety logic', () => {
  // These identifiers are defined once, in validation.ts. A content module that
  // declares its own copy has reintroduced the duplication this refactor
  // removed.
  const forbiddenDefinitions = [
    /function\s+classifyTypeTarget\b/,
    /const\s+TEXT_CAPABLE_INPUT_TYPES\b/,
    /const\s+LIMITS\s*=/,
    /function\s+clampScroll\b/,
    /function\s+checkSnapshotBinding\b/,
  ];
  for (const { name, text } of contentSources()) {
    for (const pattern of forbiddenDefinitions) {
      assert.equal(
        pattern.test(text),
        false,
        `${name} redefines shared safety logic (${pattern}); import it from ../validation.ts instead`,
      );
    }
  }
});

test('the content actions module imports the shared safety source', () => {
  const actions = readFileSync(path.join(contentDir, 'actions.ts'), 'utf8');
  assert.match(actions, /from '\.\.\/validation\.ts'/);
  for (const symbol of ['classifyTypeTarget', 'clampScroll', 'checkSnapshotBinding']) {
    assert.match(actions, new RegExp(symbol));
  }
});

test('no content module carries a keep-in-sync instruction', () => {
  for (const { name, text } of contentSources()) {
    assert.equal(/keep .*in sync/i.test(text), false, `${name} has a keep-in-sync comment`);
    assert.equal(/deliberate .*mirror/i.test(text), false, `${name} claims to mirror validation`);
  }
});

test('when built, content.js contains the shared field-safety implementation', () => {
  if (!existsSync(distContent)) {
    // The build (npm run build) is the authoritative gate and fails without the
    // marker; this test only adds coverage when a build is present.
    return;
  }
  const bundled = readFileSync(distContent, 'utf8');
  // This exact message is defined only in validation.ts (classifyTypeTarget).
  assert.match(bundled, /Refusing to type into a password field\./);
  // And the bundle is a classic script: no ES import/export survived.
  assert.equal(/^import\s/m.test(bundled), false);
  assert.equal(/^export\s/m.test(bundled), false);
});
