// Helpers for driving the real shipped extension through documented Puppeteer
// APIs only. No test hooks, no direct handler/listener calls.

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { waitFor } from './waits.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
export const EXT_PATH = path.resolve(HERE, '..', '..', '..', '..', 'dist', 'browser-harness');
export const PANEL_PATH = 'sidepanel/index.html';

export async function installSeoul(browser) {
  const installedId = await browser.installExtension(EXT_PATH);
  const exts = await browser.extensions();
  const extension = exts.get(installedId);
  if (!extension) throw new Error('Seoul not present in browser.extensions() after install');
  await waitFor('service worker target', () =>
    browser.targets().find((t) => t.type() === 'service_worker' && t.url().includes(installedId)),
  );
  const swT = browser.targets().find((t) => t.type() === 'service_worker' && t.url().includes(installedId));
  const extId = new URL(swT.url()).host;
  return { extension, extId };
}

// Re-resolve the live Extension object (it can become stale after a reload).
export async function getExtension(browser, extId) {
  const exts = await browser.extensions();
  for (const [id, e] of exts) {
    if (id === extId || id.includes(extId)) return e;
  }
  return null;
}

export function swTarget(browser, extId) {
  return browser.targets().find((t) => t.type() === 'service_worker' && t.url().includes(extId));
}

// A service-worker handle whose chrome.* bindings are responsive.
export async function liveSW(browser, extId) {
  return waitFor(
    'live service worker',
    async () => {
      const t = swTarget(browser, extId);
      if (!t) return null;
      const w = await t.worker().catch(() => null);
      if (!w) return null;
      const ok = await Promise.race([
        w.evaluate(() => typeof chrome?.storage?.local?.get === 'function').catch(() => false),
        new Promise((r) => setTimeout(() => r(false), 1500)),
      ]);
      return ok ? w : null;
    },
    { timeoutMs: 20000 },
  );
}

export function panelTarget(browser, extId) {
  return browser.targets().find((t) => t.url().includes(`chrome-extension://${extId}/${PANEL_PATH}`));
}

// Fire the genuine toolbar action for `page` and return the attached, ready
// panel. The action is retried because a cold-started worker can run onClicked
// before its sidePanel.open gesture is honored; the second genuine click lands on
// a warm worker. This is real action invocation, never a direct open of the HTML.
export async function openPanel(browser, extension, page, extId) {
  let target = null;
  for (let attempt = 0; attempt < 5 && !target; attempt++) {
    await extension.triggerAction(page).catch(() => {});
    try {
      target = await waitFor('side-panel target', () => panelTarget(browser, extId), {
        timeoutMs: 3500,
        intervalMs: 150,
      });
    } catch {
      target = null;
    }
  }
  if (!target) throw new Error('side-panel target did not open after action retries');
  const panel = await target.asPage();
  await waitFor('panel finished restoring', async () => {
    const s = await panel.$eval('#state-indicator', (el) => el.textContent.trim()).catch(() => 'RESTORING');
    return s !== 'RESTORING';
  });
  return panel;
}

export async function closePanel(panel) {
  await panel.close().catch(() => {});
}

// Re-run the real panel controller after a worker restart by reloading the
// genuine side-panel document (not loading the HTML in a normal tab). Each reload
// re-issues the panel's own GET_PANEL_CONTEXT, which wakes the restarted worker
// and drives recovery. The first message after a cold start can be dropped, so the
// reload is retried until the panel leaves RESTORING. This is needed because, in
// Chrome for Testing under CDP, a side panel cannot be re-opened via the action
// after a forced worker termination, but its already-open document can be reloaded.
export async function recoverViaPanelReload(panel, { attempts = 8 } = {}) {
  for (let i = 0; i < attempts; i++) {
    await panel.reload({ waitUntil: 'domcontentloaded' }).catch(() => {});
    try {
      await waitFor(
        'panel left RESTORING',
        async () => {
          const s = await panel.$eval('#state-indicator', (el) => el.textContent.trim()).catch(() => 'RESTORING');
          return s !== 'RESTORING';
        },
        { timeoutMs: 4000, intervalMs: 150 },
      );
      return;
    } catch {
      // Cold-start message dropped; reload and try again.
    }
  }
  throw new Error('panel did not leave RESTORING after reload retries');
}

export async function panelState(panel) {
  return panel.$eval('#state-indicator', (el) => el.textContent.trim());
}

export async function isDisabled(panel, sel) {
  return panel.$eval(sel, (el) => el.disabled);
}

// Click the element-item in the panel list whose accessible name matches.
export async function selectElement(panel, name) {
  const clicked = await panel.evaluate((wanted) => {
    const items = [...document.querySelectorAll('#elements-list .element-item')];
    const match = items.find(
      (b) => (b.querySelector('.element-item__name')?.textContent || '').trim() === wanted,
    );
    if (match) {
      match.click();
      return true;
    }
    return false;
  }, name);
  if (!clicked) throw new Error(`element not found in panel list: ${name}`);
}

export async function readSessions(extPage) {
  return extPage.evaluate(async () => {
    const r = await chrome.storage.local.get('seoul.sessions.v1');
    return r['seoul.sessions.v1'] ?? {};
  });
}

export function latestSession(sessions) {
  const list = Object.values(sessions || {});
  if (!list.length) return null;
  return list.sort((a, b) => b.updatedAt - a.updatedAt)[0];
}

// Service-worker target ids as the browser actually sees them (CDP). Puppeteer's
// own target registry can lag for service workers, so worker-lifecycle readiness
// is measured here, not via browser.targets() object identity.
export async function swTargetIds(browser, extId) {
  const cdp = await browser.target().createCDPSession();
  try {
    const { targetInfos } = await cdp.send('Target.getTargets');
    return targetInfos
      .filter((t) => t.type === 'service_worker' && t.url.includes(extId))
      .map((t) => t.targetId);
  } finally {
    await cdp.detach().catch(() => {});
  }
}

// Force-close only Seoul's service-worker target via the CDP target operation.
// This is a forced-worker-failure injection, not a model of natural MV3 idle
// termination. The worker is warmed first so the close terminates a genuinely
// running execution context (closing an already-idle target is a no-op), and the
// close is confirmed by the target disappearing.
export async function forceCloseWorker(browser, extId, { timeoutMs = 12000 } = {}) {
  await liveSW(browser, extId);
  const [oldId] = await swTargetIds(browser, extId);
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const ids = await swTargetIds(browser, extId);
    if (!ids.includes(oldId) && ids.length === 0) return oldId;
    const cdp = await browser.target().createCDPSession();
    try {
      for (const id of ids) {
        await cdp.send('Target.closeTarget', { targetId: id }).catch(() => {});
      }
    } finally {
      await cdp.detach().catch(() => {});
    }
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error('service worker did not terminate after forced close');
}

// Per-tab activeTab probe via the SW: executeScript succeeds only with a grant.
export async function tabAccess(sw) {
  return sw.evaluate(async () => {
    const tabs = await chrome.tabs.query({});
    const out = [];
    for (const t of tabs) {
      let granted = false;
      try {
        const r = await chrome.scripting.executeScript({ target: { tabId: t.id }, func: () => location.href });
        granted = !!(r && r[0]);
      } catch {
        granted = false;
      }
      out.push({
        id: t.id,
        active: t.active,
        urlReadable: typeof t.url === 'string' && t.url.length > 0,
        url: t.url ?? null,
        granted,
      });
    }
    return out;
  });
}
