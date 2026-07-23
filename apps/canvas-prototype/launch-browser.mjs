// Launch a system-installed Chrome/Chromium through puppeteer-core.
// Nothing is downloaded: the browser must already exist on the host
// (Google Chrome locally, google-chrome on CI runners), or be named
// explicitly via SEOUL_CHROME_BINARY.
import { existsSync } from 'node:fs';
import puppeteer from 'puppeteer-core';

const CANDIDATES = [
  process.env.SEOUL_CHROME_BINARY,
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  '/Applications/Chromium.app/Contents/MacOS/Chromium',
  '/usr/bin/google-chrome',
  '/usr/bin/google-chrome-stable',
  '/usr/bin/chromium-browser',
  '/usr/bin/chromium',
].filter(Boolean);

export function resolveChromeBinary() {
  const found = CANDIDATES.find((p) => existsSync(p));
  if (!found) {
    throw new Error(
      'No Chrome/Chromium binary found on this host. Install Google Chrome ' +
        'or point SEOUL_CHROME_BINARY at a browser executable.',
    );
  }
  return found;
}

export function launchBrowser(options = {}) {
  return puppeteer.launch({ executablePath: resolveChromeBinary(), ...options });
}
