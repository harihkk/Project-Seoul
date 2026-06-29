// Self-contained Node static server for the local fixture, on an OS-assigned
// port. No dependency on Python or a manually running server.

import http from 'node:http';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const FIXTURES = path.resolve(HERE, '..', '..', '..', '..', 'fixtures');

export async function startFixtureServer() {
  const server = http.createServer(async (req, res) => {
    try {
      const u = new URL(req.url, 'http://localhost');
      if (u.pathname === '/favicon.ico') {
        res.writeHead(204);
        res.end();
        return;
      }
      const rel = u.pathname === '/' ? 'interactive-page.html' : u.pathname.replace(/^\/+/, '');
      const file = path.join(FIXTURES, rel);
      if (!file.startsWith(FIXTURES)) {
        res.writeHead(403);
        res.end('forbidden');
        return;
      }
      const data = await readFile(file);
      res.writeHead(200, { 'content-type': 'text/html; charset=utf-8' });
      res.end(data);
    } catch {
      res.writeHead(404);
      res.end('not found');
    }
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;
  const base = `http://localhost:${port}`;
  return {
    base,
    url: (query = '') => `${base}/interactive-page.html${query}`,
    async close() {
      await new Promise((resolve) => server.close(resolve));
    },
  };
}
