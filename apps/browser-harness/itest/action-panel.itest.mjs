// Case 1: real action and panel. End-to-end through the genuine toolbar action,
// no handler/listener calls.

import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launchBrowser, makeSink, watchPage, assertNoUnexpectedErrors } from './support/browser.mjs';
import { startFixtureServer } from './support/fixture-server.mjs';
import { installSeoul, liveSW, openPanel, tabAccess, isDisabled } from './support/extension.mjs';
import { waitFor } from './support/waits.mjs';
import { dumpOnFailure } from './support/artifacts.mjs';

const ALLOW = ['Password field is not contained in a form'];
let browser, server, ext, sw;

before(async () => {
  server = await startFixtureServer();
  browser = await launchBrowser();
  ext = await installSeoul(browser);
  sw = await liveSW(browser, ext.extId);
});

after(async () => {
  await browser?.close().catch(() => {});
  await server?.close().catch(() => {});
});

test('real action grants activeTab and opens an attachable panel', async (t) => {
  const sink = makeSink();
  let page, panel;
  try {
    page = await browser.newPage();
    await watchPage(page, 'fixture', sink);
    await page.goto(server.url(), { waitUntil: 'domcontentloaded' });
    await page.bringToFront();

    // Before the action: the fixture tab is not accessible.
    const before = (await tabAccess(sw)).find((x) => x.active);
    assert.equal(before.granted, false, 'no scripting grant before action');
    assert.equal(before.urlReadable, false, 'tab url not readable before action');

    // Genuine action invocation opens the panel and grants activeTab.
    panel = await openPanel(browser, ext.extension, page, ext.extId);
    await watchPage(panel, 'panel', sink);

    const granted = await waitFor(
      'fixture tab granted after action',
      async () => (await tabAccess(sw)).find((x) => x.granted && /interactive-page\.html/.test(x.url || '')),
      { timeoutMs: 6000 },
    );
    assert.ok(granted.urlReadable, 'tab metadata readable after grant');

    // Side-panel evidence.
    assert.ok(panel.url().includes('/sidepanel/index.html'), 'panel is the side-panel document');
    assert.equal(await panel.title(), 'Project Seoul - Development Harness');
    const origin = await panel.$eval('#tab-origin', (el) => el.textContent.trim());
    assert.equal(origin, server.base, 'panel shows the real fixture origin');
    const title = await panel.$eval('#tab-title', (el) => el.textContent.trim());
    assert.ok(title.includes('Seoul Harness Fixture'), 'panel shows the real fixture title');
    assert.equal(await isDisabled(panel, '#btn-start'), false, 'Start enabled (granted, unattached)');
    assert.equal(await isDisabled(panel, '#btn-inspect'), true, 'Inspect disabled before attachment');

    // The panel cannot nominate an arbitrary tab: the background binds to the
    // trusted active tab and rejects a fabricated tab id in a panel message.
    const spoof = await panel.evaluate(async () =>
      chrome.runtime.sendMessage({ kind: 'SESSION_START', id: 'spoof', sessionId: `spoof-${Date.now()}`, tabId: 999999999 }),
    );
    assert.equal(spoof.success, false, 'spoofed tab id is rejected');
    assert.equal(spoof.error.code, 'TAB_MISMATCH', 'arbitrary panel-supplied tab id not accepted');

    assertNoUnexpectedErrors(sink, { allow: ALLOW });
  } catch (e) {
    await dumpOnFailure(t.name, { browser, extId: ext?.extId, pages: { fixture: page, panel }, sink });
    throw e;
  }
});
