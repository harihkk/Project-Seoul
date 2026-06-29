// Case 2: exact two-tab isolation. The fixture tab's URL (and its ?tab marker)
// is only readable once that tab is granted, so the set of granted markers is a
// direct, genuine measure of which tabs the extension can access.

import test, { before, after } from 'node:test';
import assert from 'node:assert/strict';
import { launchBrowser, makeSink, watchPage, assertNoUnexpectedErrors } from './support/browser.mjs';
import { startFixtureServer } from './support/fixture-server.mjs';
import { installSeoul, liveSW, tabAccess } from './support/extension.mjs';
import { waitFor } from './support/waits.mjs';
import { dumpOnFailure } from './support/artifacts.mjs';

const ALLOW = ['Password field is not contained in a form'];
let browser, server, server2, ext, sw;

before(async () => {
  server = await startFixtureServer();
  server2 = await startFixtureServer(); // a different origin (different port)
  browser = await launchBrowser();
  ext = await installSeoul(browser);
  sw = await liveSW(browser, ext.extId);
});

after(async () => {
  await browser?.close().catch(() => {});
  await server?.close().catch(() => {});
  await server2?.close().catch(() => {});
});

function grantedMarkers(access) {
  return access
    .filter((t) => t.granted && t.url)
    .map((t) => {
      try {
        return new URL(t.url).searchParams.get('tab');
      } catch {
        return null;
      }
    })
    .filter(Boolean)
    .sort();
}

test('the action grants only the targeted tab, separately per tab', async (t) => {
  const sink = makeSink();
  let pageA, pageB;
  try {
    pageA = await browser.newPage();
    await watchPage(pageA, 'tabA', sink);
    await pageA.goto(server.url('?tab=a'), { waitUntil: 'domcontentloaded' });
    pageB = await browser.newPage();
    await watchPage(pageB, 'tabB', sink);
    await pageB.goto(server.url('?tab=b'), { waitUntil: 'domcontentloaded' });

    // Before either action: neither tab is accessible.
    assert.deepEqual(grantedMarkers(await tabAccess(sw)), [], 'no tab granted before any action');

    // Trigger the action for A only (the action grants whichever tab is active).
    await pageA.bringToFront();
    await ext.extension.triggerAction(pageA);
    await waitFor('A granted', async () => grantedMarkers(await tabAccess(sw)).includes('a'));
    assert.deepEqual(grantedMarkers(await tabAccess(sw)), ['a'], 'only A granted; B did not inherit');

    // Trigger the action for B; it is granted separately. A keeps its grant.
    await pageB.bringToFront();
    await ext.extension.triggerAction(pageB);
    await waitFor('B granted', async () => grantedMarkers(await tabAccess(sw)).includes('b'));
    assert.deepEqual(grantedMarkers(await tabAccess(sw)), ['a', 'b'], 'A and B granted independently');

    // Navigating A to another origin revokes A's grant; B is unaffected.
    await pageA.goto(server2.url(), { waitUntil: 'domcontentloaded' });
    await waitFor('A grant revoked', async () => !grantedMarkers(await tabAccess(sw)).includes('a'));
    assert.deepEqual(grantedMarkers(await tabAccess(sw)), ['b'], 'B unaffected by A navigation');

    assertNoUnexpectedErrors(sink, { allow: ALLOW });
  } catch (e) {
    await dumpOnFailure(t.name, { browser, extId: ext?.extId, pages: { tabA: pageA, tabB: pageB }, sink });
    throw e;
  }
});
