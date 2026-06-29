// Required case 5: shipped-extension security assertions.
//
// Static checks over dist/ (the exact bytes loaded into the browser by the other
// required cases). No browser is launched here. The live trust-boundary check
// (an arbitrary panel-supplied tab id is rejected) runs in action-panel.itest.mjs,
// where a genuinely granted panel exists. Persistence safety (no typed text,
// password, input value, page HTML or semantic snapshot is ever stored) is proven
// by the control-session-store unit suite.

import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync, readdirSync, statSync } from 'node:fs';
import path from 'node:path';
import { EXT_PATH } from './support/extension.mjs';

test('shipped extension keeps the locked-down manifest and ships no test hooks', () => {
  const manifest = JSON.parse(readFileSync(path.join(EXT_PATH, 'manifest.json'), 'utf8'));

  assert.equal(manifest.manifest_version, 3, 'Manifest V3');
  assert.deepEqual(
    [...manifest.permissions].sort(),
    ['activeTab', 'scripting', 'sidePanel', 'storage'],
    'exactly the four intended permissions',
  );
  assert.equal(manifest.host_permissions, undefined, 'no host permissions');
  assert.equal(manifest.optional_host_permissions, undefined, 'no optional host permissions');
  assert.equal(manifest.content_scripts, undefined, 'no permanent (declarative) content scripts');
  assert.equal(manifest.externally_connectable, undefined, 'not externally connectable');
  assert.ok(!manifest.permissions.includes('tabs'), 'no broad tabs permission');
  assert.ok(!manifest.permissions.includes('debugger'), 'no debugger permission');

  // No remote code: the service worker is a packaged relative file.
  const sw = manifest.background?.service_worker ?? '';
  assert.ok(sw && !/^https?:/i.test(sw), 'service worker is a packaged file');

  // No test hooks or remote-code markers anywhere in the shipped bundle. The
  // remote-code scan inspects only the shipped extension files, never test code.
  const banned = [/__seoulTest/i, /__SEOUL_TEST/i, /\beval\s*\(/, /new Function\s*\(/, /https?:\/\/\S+\.js/i];
  const walk = (dir) => {
    for (const entry of readdirSync(dir)) {
      const abs = path.join(dir, entry);
      if (statSync(abs).isDirectory()) {
        walk(abs);
        continue;
      }
      if (!/\.(js|html|css|json)$/.test(entry)) continue;
      const text = readFileSync(abs, 'utf8');
      for (const re of banned) {
        assert.ok(!re.test(text), `shipped file ${path.relative(EXT_PATH, abs)} must not match ${re}`);
      }
    }
  };
  walk(EXT_PATH);
});
