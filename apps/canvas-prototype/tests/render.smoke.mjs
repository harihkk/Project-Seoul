// Headless render smoke test: loads the built self-contained HTML in a real
// browser and drives the actual product loop - run a goal, confirm an adaptive
// artifact renders (a real SVG chart), switch its representation and confirm
// the SAME artifact patches to a table in place, and run a different capability
// to confirm a second, differently-shaped artifact appears. Fails on any
// console error. This proves the prototype runs, not just compiles.

import test from 'node:test';
import assert from 'node:assert/strict';
import puppeteer from 'puppeteer';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const here = path.dirname(new URL(import.meta.url).pathname);
const htmlUrl = pathToFileURL(path.join(here, '..', 'dist', 'index.html')).href;

test('canvas prototype renders and drives the full loop headlessly', async () => {
  const browser = await puppeteer.launch({ args: ['--no-sandbox'] });
  try {
    const page = await browser.newPage();
    const errors = [];
    page.on('console', (m) => { if (m.type() === 'error') errors.push(m.text()); });
    page.on('pageerror', (e) => errors.push(String(e)));
    await page.goto(htmlUrl, { waitUntil: 'networkidle0' });

    // The Canvas opens with a first artifact (the latency timeline -> a chart).
    await page.waitForSelector('.artifact', { timeout: 5000 });
    const firstIsChart = await page.$('.artifact .chart-svg');
    assert.ok(firstIsChart, 'first artifact should render a real SVG chart');

    // Real axes/series were drawn (not an empty SVG).
    const pathCount = await page.$$eval('.artifact .chart-svg path', (n) => n.length);
    assert.ok(pathCount >= 1, 'chart should draw at least one series path');

    // Switch the chart to a table, in place: same artifact count, now a table.
    const artifactsBefore = await page.$$eval('.artifact', (n) => n.length);
    const tableBtn = await page.$$eval('.rep-btn', (btns) => {
      const t = btns.find((b) => b.textContent.trim() === 'Table');
      if (t) t.click();
      return !!t;
    });
    assert.ok(tableBtn, 'a Table representation switch should be available');
    await page.waitForSelector('.artifact .data-table', { timeout: 3000 });
    const artifactsAfter = await page.$$eval('.artifact', (n) => n.length);
    assert.equal(artifactsAfter, artifactsBefore, 'switching representation must patch in place, not duplicate');

    // Run a different capability (a record -> a card), a new artifact appears.
    await page.type('.composer-input', 'describe the compute node profile with region and status');
    await page.click('.primary-btn');
    await page.waitForFunction(
      (n) => document.querySelectorAll('.artifact').length > n,
      { timeout: 3000 },
      artifactsAfter,
    );
    const hasCard = await page.$('.artifact .card-grid, .artifact .kv');
    assert.ok(hasCard, 'a record result should render as a card/details, not a chart');

    // Run a hierarchy capability -> a tree.
    await page.$eval('.composer-input', (el) => (el.value = ''));
    await page.type('.composer-input', 'list the workspace project hierarchy tree');
    await page.click('.primary-btn');
    await page.waitForSelector('.artifact .tree', { timeout: 3000 });

    assert.deepEqual(errors, [], 'no console/page errors during the loop');
  } finally {
    await browser.close();
  }
});
