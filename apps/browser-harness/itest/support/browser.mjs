// Real Chrome for Testing via Puppeteer 25.2.1. Puppeteer creates and removes a
// fresh temporary user-data directory per launch, so every run is isolated.

import puppeteer from 'puppeteer';

export async function launchBrowser() {
  return puppeteer.launch({
    headless: process.env.SEOUL_HEADFUL ? false : true,
    enableExtensions: true,
    protocolTimeout: 60000,
    args: ['--no-sandbox', '--no-first-run', '--no-default-browser-check'],
  });
}

export function makeSink() {
  return { console: [], consoleErrors: [], pageErrors: [] };
}

// Collect console errors, page errors, and page-side unhandled rejections.
export async function watchPage(page, label, sink) {
  page.on('console', (m) => {
    const line = `${label}:${m.type()}:${m.text()}`;
    sink.console.push(line);
    if (m.type() === 'error') sink.consoleErrors.push(`${label}:${m.text()}`);
  });
  page.on('pageerror', (e) => sink.pageErrors.push(`${label}:${e.message}`));
  await page
    .evaluate(() => {
      window.addEventListener('unhandledrejection', (e) => {
        const reason = e && e.reason && e.reason.message ? e.reason.message : String(e && e.reason);
        console.error('UNHANDLED_REJECTION:' + reason);
      });
    })
    .catch(() => {});
}

// Fail on any console/page error that is not explicitly allowed.
export function assertNoUnexpectedErrors(sink, { allow = [] } = {}) {
  const all = [...sink.consoleErrors, ...sink.pageErrors];
  const unexpected = all.filter((e) => !allow.some((a) => e.includes(a)));
  if (unexpected.length) {
    throw new Error(`unexpected console/page errors:\n  - ${unexpected.join('\n  - ')}`);
  }
}
