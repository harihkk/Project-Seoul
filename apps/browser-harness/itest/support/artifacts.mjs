// Failure-only artifacts under a gitignored directory.

import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ART_ROOT = path.resolve(HERE, '..', 'artifacts');

function safeName(name) {
  return name.replace(/[^a-z0-9_-]+/gi, '_').slice(0, 80);
}

export async function dumpOnFailure(name, { browser, extId, pages = {}, sink, extra = {} } = {}) {
  const dir = path.join(ART_ROOT, safeName(name));
  await mkdir(dir, { recursive: true });

  try {
    if (browser) {
      const targets = browser.targets().map((t) => ({ type: t.type(), url: t.url() }));
      await writeFile(path.join(dir, 'targets.json'), JSON.stringify(targets, null, 2));
    }
  } catch {}

  for (const [key, page] of Object.entries(pages)) {
    try {
      if (page && !page.isClosed?.()) {
        await page.screenshot({ path: path.join(dir, `${safeName(key)}.png`) });
      }
    } catch {}
  }

  try {
    if (sink) await writeFile(path.join(dir, 'console.json'), JSON.stringify(sink, null, 2));
  } catch {}

  try {
    const meta = { extId, ...extra };
    await writeFile(path.join(dir, 'meta.json'), JSON.stringify(meta, null, 2));
  } catch {}

  return dir;
}
