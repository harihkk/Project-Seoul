// Case 4: navigation invalidation. After the bound document's identity changes,
// the old binding must not be usable and no action may reach the new document.
//
// In the real system a top-level reload fires the background's onUpdated('loading')
// invalidation, which ends the live session, so the stale action is rejected at the
// session boundary (STALE_SESSION) before it can reach the new document's content
// script. The content-level document-token / snapshot-id rejections are the inner
// defense covered by the unit suite (checkSnapshotBinding). This test proves the
// end-to-end safe outcome: the old selection cannot act, the page is untouched, and
// a fresh inspect rebinds cleanly.

import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launchBrowser, makeSink, watchPage, assertNoUnexpectedErrors } from './support/browser.mjs';
import { startFixtureServer } from './support/fixture-server.mjs';
import {
  installSeoul,
  liveSW,
  openPanel,
  closePanel,
  panelTarget,
  panelState,
  selectElement,
  readSessions,
  latestSession,
} from './support/extension.mjs';
import { waitFor, waitGone } from './support/waits.mjs';
import { dumpOnFailure } from './support/artifacts.mjs';

const ALLOW = ['Password field is not contained in a form'];
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

async function elementCount(panel) {
  return panel.$$eval('#elements-list .element-item', (els) => els.length);
}

test('navigation invalidates the binding; stale action is rejected, rebind is clean', async (t) => {
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
    await panel.click('#btn-inspect');
    await waitFor('observed', async () => (await elementCount(panel)) > 0);
    await selectElement(panel, 'Update status');

    const before = latestSession(await readSessions(panel));
    assert.ok(before && before.state === 'READY', 'session READY before navigation');
    assert.equal(await page.$eval('#status', (el) => el.textContent.trim()), 'idle');

    // Change the document identity: reload the fixture.
    await page.reload({ waitUntil: 'domcontentloaded' });

    // The background invalidates the live session on the top-level load.
    await waitFor('session invalidated by navigation', async () => {
      const s = (await readSessions(panel))[before.sessionId];
      return s && s.state === 'STOPPED';
    });

    // Acting on the old selection is rejected and never reaches the new document.
    await panel.click('#btn-click');
    await waitFor(
      'stale action rejected',
      async () => (await panel.$eval('#error-code', (el) => el.textContent.trim())) === 'STALE_SESSION',
    );
    assert.equal(await page.$eval('#status', (el) => el.textContent.trim()), 'idle', 'page untouched by stale action');
    assert.equal(await panelState(panel), 'IDLE', 'panel detached after stale action');

    // A fresh action invocation rebinds cleanly and a new binding works.
    await closePanel(panel);
    await waitGone('panel target', () => panelTarget(browser, ext.extId));
    await page.bringToFront();
    panel = await openPanel(browser, ext.extension, page, ext.extId);
    await watchPage(panel, 'panel2', sink);
    await panel.click('#btn-start');
    await waitFor('reattached', async () => (await panelState(panel)) === 'ATTACHED');
    await panel.click('#btn-inspect');
    await waitFor('re-observed', async () => (await elementCount(panel)) > 0);

    const after = latestSession(await readSessions(panel));
    assert.notEqual(after.sessionId, before.sessionId, 'a new session id backs the fresh binding');

    await selectElement(panel, 'Update status');
    await panel.click('#btn-click');
    await waitFor(
      'fresh binding acts on the new document',
      async () => (await page.$eval('#status', (el) => el.textContent.trim())) === 'button clicked',
    );

    assertNoUnexpectedErrors(sink, { allow: ALLOW });
  } catch (e) {
    await dumpOnFailure(t.name, { browser, extId: ext?.extId, pages: { fixture: page, panel }, sink });
    throw e;
  }
});
