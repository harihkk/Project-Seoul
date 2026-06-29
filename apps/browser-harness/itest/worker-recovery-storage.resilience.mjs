// OPTIONAL RESILIENCE: storage persistence and control-session reconstruction
// across a FORCED service-worker termination.
//
// This is forced worker termination (CDP Target.closeTarget). It is:
//   - NOT natural MV3 idle shutdown,
//   - NOT extension reload (chrome.runtime.reload()),
//   - NOT equivalent to Chrome's normal idle worker stop.
// It is a deliberate fault injection used only to exercise recovery code.
//
// What this proves: the persisted control-session record survives the worker
// termination, and the genuine panel reconstructs in-memory state by reloading its
// own document, which re-issues the panel's own GET_PANEL_CONTEXT and drives the
// real recovery path (probe -> planRecovery -> applyRecovery) exactly once. Because
// the page document survives a worker-only termination, the safe outcome here is a
// single SESSION_RECOVERED. The interrupted-action branch is covered by
// forced-worker-loss.resilience.mjs; every planRecovery branch is unit-tested.
//
// chrome.runtime.reload() is intentionally NOT exercised: in the tested Puppeteer
// 25.2.1 + Chrome for Testing 150 combination it did not reliably reattach the
// unpacked extension, so it is excluded from any automated assertion. This suite
// makes no claim about extension reload.

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
  readSessions,
} from './support/extension.mjs';
import { waitFor } from './support/waits.mjs';
import { dumpOnFailure } from './support/artifacts.mjs';

const ALLOW = ['Password field is not contained in a form', 'message port closed'];
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

test('forced worker termination preserves the record and reconstructs the session, once', async (t) => {
  const sink = makeSink();
  let page, panel;
  try {
    page = await browser.newPage();
    await watchPage(page, 'fixture', sink);
    await page.goto(server.url(), { waitUntil: 'domcontentloaded' });
    await page.bringToFront();

    panel = await openPanel(browser, ext.extension, page, ext.extId);
    await watchPage(panel, 'panel', sink);
    await panel.click('#btn-start');
    await waitFor('attached', async () => (await panelState(panel)) === 'ATTACHED');

    // The legitimate, unique session record is the marker verified across the
    // forced termination (no test-only storage key is created).
    const before = await waitFor('session READY in storage', async () => {
      const s = Object.values(await readSessions(panel)).find((r) => r.state === 'READY');
      return s ?? null;
    });
    const sessionId = before.sessionId;
    assert.ok(sessionId && before.createdAt, 'session has an id and creation time');

    // Force-terminate the worker. Observe the old worker target disappear.
    const [oldId] = await swTargetIds(browser, ext.extId);
    await forceCloseWorker(browser, ext.extId);
    assert.ok(!(await swTargetIds(browser, ext.extId)).includes(oldId), 'old worker target gone');

    // Reconstruct through the genuine panel: reload its own document (a panel-
    // document reload, not chrome.runtime.reload()) so it re-issues the panel's own
    // GET_PANEL_CONTEXT against the revived worker.
    await recoverViaPanelReload(panel);

    // Read storage from the live extension context after recovery.
    const after = await waitFor('record recovered', async () => {
      const r = (await readSessions(panel))[sessionId];
      return r && countEvent(r, 'SESSION_RECOVERED') >= 1 ? r : null;
    });

    // Persistent record remains and is the same record.
    assert.equal(after.sessionId, sessionId, 'same session record persisted across the termination');
    assert.equal(after.createdAt, before.createdAt, 'same creation time (not a new record)');
    // Non-terminal attachment reconciled to the designed safe state, exactly once.
    assert.equal(after.state, 'READY', 'recovered to READY (document proven intact)');
    assert.equal(countEvent(after, 'SESSION_RECOVERED'), 1, 'exactly one recovery event');
    assert.equal(countEvent(after, 'RECONCILED_ON_STARTUP'), 0, 'not falsely reconciled to stopped');

    // Neutral surface; old ephemeral panel state does not return.
    assert.equal(await panelState(panel), 'ATTACHED', 'neutral but attached after recovery');
    assert.equal(await panel.$eval('#error-box', (el) => el.hidden), true, 'no stale error');
    assert.equal(await panel.$$eval('#elements-list .element-item', (l) => l.length), 0, 'no stale elements');

    // Re-running the panel context again does not produce a duplicate recovery
    // event (the session is now live again).
    await recoverViaPanelReload(panel);
    const again = (await readSessions(panel))[sessionId];
    assert.equal(countEvent(again, 'SESSION_RECOVERED'), 1, 'no duplicate recovery on second panel context');

    assertNoUnexpectedErrors(sink, { allow: ALLOW });
  } catch (e) {
    await dumpOnFailure(t.name, { browser, extId: ext?.extId, pages: { fixture: page, panel }, sink });
    throw e;
  }
});
