// OPTIONAL RESILIENCE: forced worker loss during an in-flight action, and no replay.
//
// This is forced worker termination (CDP Target.closeTarget). It is:
//   - NOT natural MV3 idle shutdown,
//   - NOT extension reload (chrome.runtime.reload()),
//   - NOT equivalent to Chrome's normal idle worker stop.
// It is a deliberate fault injection used only to exercise recovery code.
//
// The action is held in flight deterministically by a fixture-only synchronous
// delay (the extension is unchanged and has no test hook). At the exact moment the
// stored state is EXECUTING, Seoul's service-worker target is force-closed. The
// recovery code path then reconciles EXECUTING to STOPPED with a single
// ACTION_OUTCOME_UNKNOWN and never replays the action.

import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launchBrowser, makeSink, watchPage, assertNoUnexpectedErrors } from './support/browser.mjs';
import { startFixtureServer } from './support/fixture-server.mjs';
import {
  installSeoul,
  liveSW,
  swTargetIds,
  forceCloseWorker,
  openPanel,
  recoverViaPanelReload,
  panelState,
  selectElement,
  readSessions,
} from './support/extension.mjs';
import { waitFor } from './support/waits.mjs';
import { dumpOnFailure } from './support/artifacts.mjs';

// The forced kill severs the in-flight action message and reloads the panel, so
// the lost-port rejection and the panel-context-invalidation are expected here.
const ALLOW = [
  'Password field is not contained in a form',
  'message port closed',
  'message channel closed', // in-flight action channel severed by the forced kill
  'Extension context invalidated',
  'UNHANDLED_REJECTION',
  'Could not establish connection',
];
let browser, server, ext;

before(async () => {
  server = await startFixtureServer();
  browser = await launchBrowser();
  ext = await installSeoul(browser);
  await liveSW(browser, ext.extId);
});

after(async () => {
  await browser?.close().catch(() => {});
  await server?.close().catch(() => {});
});

function countEvent(record, type) {
  return (record?.timeline ?? []).filter((e) => e.type === type).length;
}

async function slowCount(page) {
  return Number(await page.$eval('#slow-count', (el) => el.textContent.trim()));
}

test('forced worker loss during EXECUTING: reconcile once, never replay', async (t) => {
  const sink = makeSink();
  let page, panel;
  try {
    // Fixture holds a click synchronously for 6s so EXECUTING is observable.
    page = await browser.newPage();
    await watchPage(page, 'fixture', sink);
    await page.goto(server.url('?clickDelayMs=6000'), { waitUntil: 'domcontentloaded' });
    await page.bringToFront();

    panel = await openPanel(browser, ext.extension, page, ext.extId);
    await watchPage(panel, 'panel', sink);
    await panel.click('#btn-start');
    await waitFor('attached', async () => (await panelState(panel)) === 'ATTACHED');
    await panel.click('#btn-inspect');
    await waitFor('observed', async () => (await panel.$$eval('#elements-list .element-item', (l) => l.length)) > 0);
    await selectElement(panel, 'Run slow action');
    assert.equal(await slowCount(page), 0, 'slow action has not run yet');

    // Drive the action through the actual panel; do not await the in-flight click.
    const sessionId = Object.values(await readSessions(panel)).find((r) => r.state !== 'STOPPED').sessionId;
    await panel.click('#btn-click');

    // Observe the real stored state reaching EXECUTING, then capture the pending
    // action data and prove it is non-sensitive (only the action kind is stored).
    const executing = await waitFor('state EXECUTING', async () => {
      const r = (await readSessions(panel))[sessionId];
      return r && r.state === 'EXECUTING' ? r : null;
    });
    assert.equal(executing.pendingAction, 'CLICK_ELEMENT', 'only the action kind is persisted (no text/target)');

    // At that exact point, force-close only the service-worker target.
    const [oldId] = await swTargetIds(browser, ext.extId);
    await forceCloseWorker(browser, ext.extId);
    assert.ok(!(await swTargetIds(browser, ext.extId)).includes(oldId), 'worker target force-closed');

    // Wake and reconcile through the genuine panel.
    await recoverViaPanelReload(panel);

    const after = await waitFor('reconciled to outcome-unknown', async () => {
      const r = (await readSessions(panel))[sessionId];
      return r && countEvent(r, 'ACTION_OUTCOME_UNKNOWN') >= 1 ? r : null;
    });

    // The session is safely stopped with exactly one outcome-unknown record.
    assert.equal(after.state, 'STOPPED', 'session safely stopped');
    assert.equal(countEvent(after, 'ACTION_OUTCOME_UNKNOWN'), 1, 'exactly one ACTION_OUTCOME_UNKNOWN');
    assert.equal(after.pendingAction, undefined, 'pending action cleared after reconcile (never retried)');
    assert.equal(await panelState(panel), 'IDLE', 'panel detached / neutral after reconcile');

    // The page-side effect happened at most once (the busy hold completes once),
    // and is never replayed.
    await waitFor('page effect settled', async () => (await slowCount(page)) >= 1, { timeoutMs: 9000 });
    assert.equal(await slowCount(page), 1, 'page effect occurred exactly once');

    // No automatic retry: re-running the panel context does not re-execute or add
    // a second outcome-unknown.
    await recoverViaPanelReload(panel);
    assert.equal(await slowCount(page), 1, 'still exactly once after another panel context');
    const again = (await readSessions(panel))[sessionId];
    assert.equal(countEvent(again, 'ACTION_OUTCOME_UNKNOWN'), 1, 'no duplicate outcome-unknown');
    assert.equal(again.state, 'STOPPED', 'remains stopped, no replay');

    assertNoUnexpectedErrors(sink, { allow: ALLOW });
  } catch (e) {
    await dumpOnFailure(t.name, { browser, extId: ext?.extId, pages: { fixture: page, panel }, sink });
    throw e;
  }
});
