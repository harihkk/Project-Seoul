// Project Seoul Development Harness - content: page observation.
//
// Builds a bounded PageSnapshot: interactive elements plus visible text blocks,
// under the shared observation limits. A new observation regenerates the
// snapshot id and resets element identity so prior ephemeral ids cannot be
// reused. No extension APIs; consumes the per-document state.

import { LIMITS } from '../validation.ts';
import type { PageSnapshot } from '../protocol.ts';
import { freshRegistry, randomToken, type HarnessState } from './document-session.ts';
import { clip, collectInteractive, isVisible } from './semantics.ts';

const SKIP_TEXT_TAGS = new Set(['SCRIPT', 'STYLE', 'NOSCRIPT', 'TEMPLATE']);
const TEXT_BLOCK_SELECTOR =
  'p, h1, h2, h3, h4, h5, h6, li, blockquote, td, th, figcaption, summary, label, dt, dd';

function collectText(): { blocks: string[]; truncated: boolean } {
  const blocks: string[] = [];
  const seen = new Set<string>();
  let total = 0;
  let truncated = false;

  const nodes = Array.from(document.body?.querySelectorAll(TEXT_BLOCK_SELECTOR) ?? []);
  for (const el of nodes) {
    if (SKIP_TEXT_TAGS.has(el.tagName)) continue;
    if (!isVisible(el)) continue;

    const normalized = clip(el.textContent ?? '', LIMITS.MAX_TEXT_BLOCK_CHARS);
    if (!normalized) continue;
    if (seen.has(normalized)) continue;
    seen.add(normalized);

    if (blocks.length >= LIMITS.MAX_TEXT_BLOCKS) {
      truncated = true;
      break;
    }
    if (total + normalized.length > LIMITS.MAX_TOTAL_TEXT_CHARS) {
      truncated = true;
      break;
    }
    blocks.push(normalized);
    total += normalized.length;
  }
  return { blocks, truncated };
}

export function observe(state: HarnessState): PageSnapshot {
  state.registry = freshRegistry();
  state.snapshotId = randomToken();
  const interactive = collectInteractive(state.registry);
  const text = collectText();
  return {
    url: location.href,
    title: document.title,
    lang: document.documentElement.lang || '',
    viewport: { width: window.innerWidth, height: window.innerHeight },
    elements: interactive.elements,
    textBlocks: text.blocks,
    timestamp: Date.now(),
    truncated: interactive.truncated || text.truncated,
    documentToken: state.documentToken,
    snapshotId: state.snapshotId,
  };
}
