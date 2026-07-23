// Renders the built Canvas, runs several goals to populate a variety of
// adaptive artifacts, and captures screenshots (dark + light) so the result
// can be reviewed visually. Not a test; a reviewer aid.
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { launchBrowser } from './launch-browser.mjs';

const here = path.dirname(new URL(import.meta.url).pathname);
const htmlUrl = pathToFileURL(path.join(here, 'dist', 'index.html')).href;
const out = path.join(here, 'dist');

const goals = [
  'collect the substrate survey readings per station', // bar chart
  'report the current reliability metric', // metric
  'describe the compute node profile with region and status', // card
  'list the workspace project hierarchy tree', // tree
  'return the reference citations for the thread', // source list
];

const browser = await launchBrowser({ args: ['--no-sandbox'] });
const page = await browser.newPage();
await page.setViewport({ width: 1440, height: 1000, deviceScaleFactor: 2 });
await page.goto(htmlUrl, { waitUntil: 'networkidle0' });
await page.waitForSelector('.artifact');

for (const goal of goals) {
  await page.$eval('.composer-input', (el) => (el.value = ''));
  await page.type('.composer-input', goal);
  await page.click('.primary-btn');
  await new Promise((r) => setTimeout(r, 180));
}
await new Promise((r) => setTimeout(r, 400));

// Top of the worklog: masthead, lede, capability index, first entry.
await page.evaluate(() => document.querySelector('.stack').scrollTo({ top: 0, behavior: 'instant' }));
await new Promise((r) => setTimeout(r, 150));
await page.screenshot({ path: path.join(out, 'canvas-dark.png') });
console.log('wrote canvas-dark.png');

// Bottom of the worklog: the latest entries next to the command bar.
await page.evaluate(() => { const s = document.querySelector('.stack'); s.scrollTo({ top: s.scrollHeight, behavior: 'instant' }); });
await new Promise((r) => setTimeout(r, 150));
await page.screenshot({ path: path.join(out, 'canvas-dark-entries.png') });
console.log('wrote canvas-dark-entries.png');

// Light theme, top of the worklog.
await page.click('.ghost-btn');
await page.evaluate(() => document.querySelector('.stack').scrollTo({ top: 0, behavior: 'instant' }));
await new Promise((r) => setTimeout(r, 200));
await page.screenshot({ path: path.join(out, 'canvas-light.png') });
console.log('wrote canvas-light.png');

// Back to dark; patch entry 001 to its table representation in place.
await page.click('.ghost-btn');
await page.$$eval('.rep-btn', (btns) => btns.find((b) => b.textContent.trim() === 'Table')?.click());
await new Promise((r) => setTimeout(r, 250));
await page.evaluate(() => document.querySelector('.stack').scrollTo({ top: 430, behavior: 'instant' }));
await new Promise((r) => setTimeout(r, 150));
await page.screenshot({ path: path.join(out, 'canvas-dark-table.png') });
console.log('wrote canvas-dark-table.png');

await browser.close();
