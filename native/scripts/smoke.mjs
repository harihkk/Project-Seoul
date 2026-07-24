#!/usr/bin/env node
// Product launch smoke test for the locally built Seoul Chromium.
//
// Uses the repo's pinned puppeteer-core (no browser download) with an explicit
// executablePath pointing at the built binary. Requires no internet (uses a
// data: URL) and does not modify Chromium source. Fails on browser disconnect
// or page crash.

import { existsSync, mkdtempSync, rmSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import puppeteer from 'puppeteer-core';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '..', '..');
const siblingRoot = ['seoul-chromium.noindex', 'seoul-chromium']
  .map((name) => path.resolve(repoRoot, '..', name))
  .find((p) => existsSync(p));
const root = (process.env.SEOUL_CHROMIUM_ROOT || '').trim()
  ? path.resolve(process.env.SEOUL_CHROMIUM_ROOT)
  : (siblingRoot ?? path.resolve(repoRoot, '..', 'seoul-chromium.noindex'));
const binary = (process.env.SEOUL_CHROMIUM_BINARY || '').trim()
  ? path.resolve(process.env.SEOUL_CHROMIUM_BINARY)
  : path.join(root, 'src', 'out', 'SeoulBaseline', 'Chromium.app', 'Contents', 'MacOS', 'Chromium');
const maxLaunchMs = 15000;
const maxLocalNavigationMs = 5000;

function fail(msg) {
  console.error(`SMOKE FAIL: ${msg}`);
  process.exit(1);
}
function assert(cond, msg) {
  if (!cond) throw new Error(msg);
  console.log(`  ok: ${msg}`);
}

if (!existsSync(binary)) {
  fail(`built Chromium not found at:\n  ${binary}\nBuild it first (native/scripts/build.sh), or set SEOUL_CHROMIUM_BINARY.`);
}

const profile = mkdtempSync(path.join(os.tmpdir(), 'seoul-smoke-profile-'));
let browser;
let crashed = null;
let closingBrowser = false;
const smokeStarted = performance.now();

try {
  console.log(`launching: ${binary}`);
  console.log(`profile:   ${profile}`);
  const launchStarted = performance.now();
  browser = await puppeteer.launch({
    executablePath: binary,
    headless: process.env.SEOUL_HEADFUL ? false : true,
    userDataDir: profile,
    protocolTimeout: 60000,
    args: ['--no-first-run', '--no-default-browser-check', '--use-mock-keychain'],
  });
  browser.on('disconnected', () => {
    if (!closingBrowser) {
      crashed = crashed || 'browser disconnected unexpectedly';
    }
  });
  const launchMs = performance.now() - launchStarted;
  assert(
    launchMs < maxLaunchMs,
    `isolated browser launch stays below the ${maxLaunchMs} ms smoke ceiling (${launchMs.toFixed(0)} ms)`,
  );

  const version = await browser.version();
  console.log(`browser version: ${version}`);

  const page = await browser.newPage();
  page.on('error', (e) => {
    crashed = `page crashed: ${e.message}`;
  });

  // (3) a normal local page via data: URL (no network).
  const navigationStarted = performance.now();
  await page.goto('data:text/html,<title>Seoul Product Smoke</title><h1 id=h>hello</h1>', {
    waitUntil: 'domcontentloaded',
  });
  const navigationMs = performance.now() - navigationStarted;
  assert(
    navigationMs < maxLocalNavigationMs,
    `local navigation stays below the ${maxLocalNavigationMs} ms smoke ceiling (${navigationMs.toFixed(0)} ms)`,
  );

  // (4) JavaScript executes.
  const sum = await page.evaluate(() => 1 + 2 + 3);
  assert(sum === 6, 'JavaScript executes in the page (1+2+3===6)');
  const heading = await page.$eval('#h', (el) => el.textContent);
  assert(heading === 'hello', 'DOM is rendered and queryable');

  // (5) a second tab opens and activates.
  const page2 = await browser.newPage();
  await page2.goto('data:text/html,<title>tab2</title>second', { waitUntil: 'domcontentloaded' });
  await page2.bringToFront();
  const pages = await browser.pages();
  assert(pages.length >= 2, 'a second tab opened and is tracked');
  assert((await page2.title()) === 'tab2', 'second tab activated and reports its title');

  // (6) the shipping first-party Canvas WebUI renders its interactive shell.
  const canvas = await browser.newPage();
  const canvasErrors = [];
  canvas.on('console', (message) => {
    if (message.type() === 'error') canvasErrors.push(message.text());
  });
  canvas.on('pageerror', (error) => canvasErrors.push(String(error)));
  await canvas.goto('chrome://seoul-canvas', { waitUntil: 'domcontentloaded' });
  await canvas.waitForFunction(async () => {
    await customElements.whenDefined('seoul-canvas-app');
    const root = document.querySelector('seoul-canvas-app')?.shadowRoot;
    return Boolean(root?.querySelector('.composer input[aria-label="Message Seoul"]'));
  });
  const canvasState = await canvas.evaluate(() => {
    const root = document.querySelector('seoul-canvas-app')?.shadowRoot;
    return {
      heading: root?.querySelector('.canvas-header h1')?.textContent?.trim(),
      views: root?.querySelectorAll('.view-switcher button').length,
      voiceOff: root?.querySelector('.voice-button')?.getAttribute('aria-pressed'),
      sendDisabled: root?.querySelector('.send-button')?.disabled,
    };
  });
  assert(canvasState.heading === 'Ask, act, understand.', 'Seoul Canvas renders its product heading');
  assert(canvasState.views === 5, 'Seoul Canvas exposes all five product views');
  assert(canvasState.voiceOff === 'false', 'voice remains explicit and default-off');
  assert(canvasState.sendDisabled === true, 'empty Canvas input cannot dispatch');
  assert(canvasErrors.length === 0, `Seoul Canvas reports no console errors (${canvasErrors.join('; ')})`);

  // (7) the browser stayed alive throughout.
  assert(browser.connected === true, 'browser remained connected for the test duration');
  assert(crashed === null, 'no disconnect or page crash occurred');

  // (8) version already collected above.
  assert(/Chrom(e|ium)\/\d+/.test(version), `browser reports a Chromium version string (${version})`);

  // (9) clean close.
  closingBrowser = true;
  await browser.close();
  browser = undefined;
  console.log(`total smoke time: ${(performance.now() - smokeStarted).toFixed(0)} ms`);
  console.log('SMOKE PASS');
} catch (e) {
  const failure = crashed || (e && e.message) || String(e);
  if (browser) {
    closingBrowser = true;
    await browser.close().catch(() => {});
  }
  fail(failure);
} finally {
  // (10) remove the temporary profile.
  try {
    rmSync(profile, { recursive: true, force: true });
  } catch {}
}
