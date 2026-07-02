// Builds the Seoul Canvas prototype into a single self-contained HTML file that
// runs offline in any browser (file:// works). JS is bundled to an IIFE and the
// CSS is inlined, so there are no external requests - the same local-only,
// no-remote-asset posture the native Canvas requires.

import esbuild from 'esbuild';
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';

const here = path.dirname(new URL(import.meta.url).pathname);
const outDir = path.join(here, 'dist');
mkdirSync(outDir, { recursive: true });

const result = await esbuild.build({
  entryPoints: [path.join(here, 'src/main.ts')],
  bundle: true,
  format: 'iife',
  target: 'es2020',
  platform: 'browser',
  write: false,
  minify: false,
  logLevel: 'info',
});

const js = result.outputFiles[0].text;

// Embed the display + mono typefaces as base64 so the bundle stays fully
// offline (no CDN, no runtime font request). Both are OFL-licensed.
function fontFace(family, file, weights) {
  const b64 = readFileSync(path.join(here, 'fonts', file)).toString('base64');
  return `@font-face{font-family:'${family}';src:url(data:font/woff2;base64,${b64}) format('woff2');font-weight:${weights};font-style:normal;font-display:swap;}`;
}
const fonts =
  fontFace('Space Grotesk', 'space-grotesk.woff2', '300 700') +
  fontFace('Geist Mono', 'geist-mono.woff2', '400 600');

const css = fonts + '\n' + readFileSync(path.join(here, 'src/styles.css'), 'utf8');

const html = `<!doctype html>
<html lang="en" data-theme="dark">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Seoul Canvas - adaptive interface prototype</title>
<style>${css}</style>
</head>
<body>
<div id="root"></div>
<script>${js}</script>
</body>
</html>
`;

const outFile = path.join(outDir, 'index.html');
writeFileSync(outFile, html, 'utf8');
console.log(`canvas-prototype: wrote ${outFile} (${(html.length / 1024).toFixed(1)} KiB, self-contained)`);
