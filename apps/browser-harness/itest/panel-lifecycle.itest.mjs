// Case 3: real panel lifecycle, driven entirely through the actual side-panel
// DOM. The panel HTML is never loaded directly; it is only ever the genuine
// side-panel page opened by the real action.

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
  isDisabled,
  selectElement,
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

test('panel lifecycle: start, inspect, sensitive refusal, reopen neutral, act again', async (t) => {
  const sink = makeSink();
  let page, panel;
  try {
    page = await browser.newPage();
    await watchPage(page, 'fixture', sink);
    await page.goto(server.url(), { waitUntil: 'domcontentloaded' });
    await page.bringToFront();

    panel = await openPanel(browser, ext.extension, page, ext.extId);
    await watchPage(panel, 'panel', sink);

    // Start.
    await panel.click('#btn-start');
    await waitFor('attached after Start', async () => (await panelState(panel)) === 'ATTACHED');
    assert.equal(await isDisabled(panel, '#btn-inspect'), false, 'Inspect enabled once attached');

    // Inspect.
    await panel.click('#btn-inspect');
    await waitFor('elements observed', async () => (await elementCount(panel)) > 0);

    // Trigger SENSITIVE_FIELD: type into the password element is refused.
    await selectElement(panel, 'Password input');
    await panel.type('#type-input', 'should-be-blocked');
    await panel.click('#btn-type');
    await waitFor(
      'SENSITIVE_FIELD surfaced',
      async () => (await panel.$eval('#error-code', (el) => el.textContent.trim())) === 'SENSITIVE_FIELD',
    );
    // The refusal does not detach: still attached.
    assert.equal(await panelState(panel), 'ATTACHED', 'refusal keeps the attachment');

    // Close the panel via the real side-panel page lifecycle, then reopen via the
    // real action.
    await closePanel(panel);
    await waitGone('panel target after close', () => panelTarget(browser, ext.extId));
    panel = await openPanel(browser, ext.extension, page, ext.extId);
    await watchPage(panel, 'panel2', sink);

    // Neutral but attached, with all ephemeral state cleared.
    assert.equal(await panelState(panel), 'ATTACHED', 'reopened attached');
    assert.equal(await panel.$eval('#error-box', (el) => el.hidden), true, 'old error absent');
    assert.equal(await elementCount(panel), 0, 'element list cleared');
    assert.equal(await panel.$eval('#selected-value', (el) => el.textContent.trim()), 'none', 'selection cleared');
    assert.equal(await panel.$eval('#type-input', (el) => el.value), '', 'draft cleared');
    assert.equal(await panel.$$eval('#timeline-list li', (l) => l.length), 0, 'operation log cleared');
    assert.equal(await isDisabled(panel, '#btn-start'), true, 'Start disabled when attached');
    assert.equal(await isDisabled(panel, '#btn-inspect'), false, 'Inspect enabled when attached');

    // Inspect again without Start.
    await panel.click('#btn-inspect');
    await waitFor('re-observed', async () => (await elementCount(panel)) > 0);

    // Click Update status; the fixture changes to "button clicked".
    assert.equal(await page.$eval('#status', (el) => el.textContent.trim()), 'idle', 'status starts idle');
    await selectElement(panel, 'Update status');
    await panel.click('#btn-click');
    await waitFor(
      'fixture status updated',
      async () => (await page.$eval('#status', (el) => el.textContent.trim())) === 'button clicked',
    );

    assertNoUnexpectedErrors(sink, { allow: ALLOW });
  } catch (e) {
    await dumpOnFailure(t.name, { browser, extId: ext?.extId, pages: { fixture: page, panel }, sink });
    throw e;
  }
});
