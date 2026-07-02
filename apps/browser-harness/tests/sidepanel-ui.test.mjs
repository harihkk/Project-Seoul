import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

// Structural tests over the side-panel and protocol source (copied verbatim into
// the build) for the error-container fix and the honest panel/attachment model.
// They stay deterministic without a browser test framework.

const dir = path.dirname(fileURLToPath(import.meta.url));
const panelDir = path.join(dir, '..', 'src', 'sidepanel');
const srcDir = path.join(dir, '..', 'src');
const css = readFileSync(path.join(panelDir, 'index.css'), 'utf8');
const html = readFileSync(path.join(panelDir, 'index.html'), 'utf8');
const ts = readFileSync(path.join(panelDir, 'index.ts'), 'utf8');
const protocol = readFileSync(path.join(srcDir, 'protocol.ts'), 'utf8');
const background = readFileSync(path.join(srcDir, 'background.ts'), 'utf8');
const sessionSrc = readFileSync(path.join(srcDir, 'session.ts'), 'utf8');
// getPanelContext moved from background.ts into the session controller during
// the module-split refactor; read it from its new home.
const controller = readFileSync(
  path.join(srcDir, 'background', 'session-controller.ts'),
  'utf8',
);
const manifest = JSON.parse(readFileSync(path.join(dir, '..', 'manifest.json'), 'utf8'));

const applyFn = ts.match(/function applyPanelContext\b[\s\S]*?\n\}/)[0];
const initFn = ts.match(/async function init\b[\s\S]*?\n\}/)[0];
const updateFn = ts.match(/function updateControls\b[\s\S]*?\n\}/)[0];
// The controller method is indented; match up to its 2-space-indented brace.
const contextFn = controller.match(/async getPanelContext\b[\s\S]*?\n {2}\}/)[0];

// --- Error-container rendering ---

test('the error box starts hidden in the markup', () => {
  assert.match(html, /id="error-box"[^>]*\bhidden\b/);
});

test('the hidden attribute always wins, even over .error { display: flex }', () => {
  assert.match(css, /\[hidden\]\s*\{[^}]*display:\s*none\s*!important/i);
});

test('only showError ever reveals the error box', () => {
  const reveals = ts.match(/errorBox\.hidden\s*=\s*false/g) ?? [];
  assert.equal(reveals.length, 1);
});

test('a real error shows the box with its code and message', () => {
  const fn = ts.match(/function showError\b[\s\S]*?\n\}/)[0];
  assert.match(fn, /errorBox\.hidden\s*=\s*false/);
  assert.match(fn, /errorCode\.textContent\s*=\s*err\.code/);
  assert.match(fn, /errorMessage\.textContent\s*=\s*err\.message/);
});

test('showError refuses to render without a message', () => {
  const fn = ts.match(/function showError\b[\s\S]*?\n\}/)[0];
  assert.match(fn, /!err\.message/);
  assert.match(fn, /clearError\(\)/);
});

test('clearError hides the box and removes its content', () => {
  const fn = ts.match(/function clearError\b[\s\S]*?\n\}/)[0];
  assert.match(fn, /errorBox\.hidden\s*=\s*true/);
  assert.match(fn, /errorCode\.textContent\s*=\s*''/);
  assert.match(fn, /errorMessage\.textContent\s*=\s*''/);
});

test('access notices use a separate element, not the error container', () => {
  const fn = ts.match(/function showAccessNotice\b[\s\S]*?\n\}/)[0];
  assert.match(fn, /accessNotice/);
  assert.equal(/errorBox/.test(fn), false);
});

// --- No speculative runtime-task abstraction remains ---

test('no speculative runtime-task types remain in the source', () => {
  const forbidden = [
    'RuntimeTaskState',
    'ActiveTaskView',
    'resolvePanelView',
    'isActiveTaskState',
    'ACTIVE_TASK_STATES',
    'RUNTIME_TASK_STATES',
    'PanelView',
  ];
  for (const file of [protocol, background, sessionSrc, controller, ts]) {
    for (const id of forbidden) {
      assert.equal(file.includes(id), false, `${id} should be removed`);
    }
  }
});

test('the panel-context response has no task field', () => {
  const iface = protocol.match(/export interface PanelContextResult \{[\s\S]*?\n\}/)[0];
  assert.equal(/\btask\b/.test(iface), false);
  const resultLiteral = contextFn.match(/const result: PanelContextResult = \{[\s\S]*?\n {4}\};/)[0];
  assert.equal(/\btask\b/.test(resultLiteral), false);
});

// --- Panel reopen renders a neutral surface (panel state is ephemeral) ---

test('reopen clears the transient error, selection, snapshot, draft, and operation log', () => {
  assert.match(applyFn, /clearError\(\)/);
  assert.match(applyFn, /clearSnapshot\(\)/); // clears selection, snapshot binding, typed draft, lists
  assert.match(applyFn, /timelineList\.replaceChildren\(\)/);
});

test('reopen never presents a stored control-session error or record as current', () => {
  assert.equal(/lastError/.test(applyFn), false);
  assert.equal(/result\.session\b/.test(applyFn), false);
});

test('reopening starts no new control session', () => {
  assert.equal(/SESSION_START/.test(initFn), false);
  assert.equal(/SESSION_START/.test(applyFn), false);
});

// --- Page attachment is kept internally and gates the controls ---

test('a valid attachment is kept internally for continuing on the same tab', () => {
  assert.match(applyFn, /result\.attachment/);
  assert.match(applyFn, /session\.sessionId\s*=\s*result\.attachment\.attached/);
});

test('Start is gated on access and a missing attachment; page actions on attachment', () => {
  assert.match(updateFn, /btnStart\.disabled\s*=\s*!granted[\s\S]*?attached/);
  assert.match(updateFn, /btnInspect\.disabled\s*=\s*!attached/);
});

test('the background reports attached true only for a live or recovered attachment', () => {
  assert.match(contextFn, /classification\.live[\s\S]*?attached: true/);
  assert.match(contextFn, /rc\.recovered[\s\S]*?attached: true/);
  // A terminal record is not deleted and yields no attachment (history).
  assert.equal(/store\.(delete|remove)/.test(contextFn), false);
});

// --- Permissions unchanged ---

test('no permission expansion: manifest permissions remain exactly the four', () => {
  assert.deepEqual(
    [...manifest.permissions].sort(),
    ['activeTab', 'scripting', 'sidePanel', 'storage'].sort(),
  );
  assert.equal('host_permissions' in manifest, false);
  assert.equal('optional_host_permissions' in manifest, false);
});
