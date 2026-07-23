#!/usr/bin/env node
// Baseline launch smoke test for the locally built Chromium.
//
// Uses the Project Seoul repo's pinned Puppeteer with an explicit executablePath
// pointing at the built binary. Requires no internet (uses a data: URL) and does
// not modify Chromium source. Fails on browser disconnect or page crash.

import { existsSync, mkdtempSync, rmSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import puppeteer from 'puppeteer';

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

try {
  console.log(`launching: ${binary}`);
  console.log(`profile:   ${profile}`);
  browser = await puppeteer.launch({
    executablePath: binary,
    headless: process.env.SEOUL_HEADFUL ? false : true,
    userDataDir: profile,
    protocolTimeout: 60000,
    args: ['--no-first-run', '--no-default-browser-check', '--use-mock-keychain'],
  });
  browser.on('disconnected', () => {
    crashed = crashed || 'browser disconnected unexpectedly';
  });

  const version = await browser.version();
  console.log(`browser version: ${version}`);

  const page = await browser.newPage();
  page.on('error', (e) => {
    crashed = `page crashed: ${e.message}`;
  });

  // (3) a normal local page via data: URL (no network).
  await page.goto('data:text/html,<title>Seoul Baseline Smoke</title><h1 id=h>hello</h1>', {
    waitUntil: 'domcontentloaded',
  });

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

  // (6) the browser stayed alive throughout.
  assert(browser.connected === true, 'browser remained connected for the test duration');
  assert(crashed === null, 'no disconnect or page crash occurred');

  // (7) version already collected above.
  assert(/Chrom(e|ium)\/\d+/.test(version), `browser reports a Chromium version string (${version})`);

  // (8) clean close.
  await browser.close();
  browser = undefined;
  console.log('SMOKE PASS');
} catch (e) {
  if (browser) await browser.close().catch(() => {});
  fail(crashed || (e && e.message) || String(e));
} finally {
  // (9) remove the temporary profile.
  try {
    rmSync(profile, { recursive: true, force: true });
  } catch {}
}
