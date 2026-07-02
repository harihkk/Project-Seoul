// Project Seoul Development Harness - content: visibility and semantics.
//
// Pure DOM inspection helpers plus interactive-element collection. No session
// state and no extension APIs; every input is a DOM Element and the current
// registry. Shared limits come from the single validation source of truth.

import { LIMITS } from '../validation.ts';
import type { InteractiveElement } from '../protocol.ts';
import { assignId, type Registry } from './document-session.ts';

export function isVisible(el: Element): boolean {
  if (!el.isConnected) return false;
  if (el.getClientRects().length === 0) return false;
  const style = getComputedStyle(el);
  if (style.display === 'none') return false;
  if (style.visibility === 'hidden' || style.visibility === 'collapse') return false;
  if (parseFloat(style.opacity || '1') === 0) return false;
  const r = el.getBoundingClientRect();
  return r.width > 0 && r.height > 0;
}

export function isDisabled(el: Element): boolean {
  const anyEl = el as unknown as { disabled?: boolean };
  if (typeof anyEl.disabled === 'boolean' && anyEl.disabled) return true;
  return el.getAttribute('aria-disabled') === 'true';
}

export function clip(text: string, max: number): string {
  const t = text.replace(/\s+/g, ' ').trim();
  return t.length > max ? t.slice(0, max) : t;
}

export function accessibleName(el: Element): string | undefined {
  const aria = el.getAttribute('aria-label');
  if (aria && aria.trim()) return clip(aria, 120);

  const labelledby = el.getAttribute('aria-labelledby');
  if (labelledby) {
    const text = labelledby
      .split(/\s+/)
      .map((id) => document.getElementById(id)?.textContent ?? '')
      .join(' ')
      .trim();
    if (text) return clip(text, 120);
  }

  const labelled = el as unknown as { labels?: ArrayLike<Element> | null };
  if (labelled.labels && labelled.labels.length > 0) {
    const text = Array.from(labelled.labels)
      .map((l) => l.textContent ?? '')
      .join(' ')
      .trim();
    if (text) return clip(text, 120);
  }

  const placeholder = el.getAttribute('placeholder');
  if (placeholder && placeholder.trim()) return clip(placeholder, 120);

  const title = el.getAttribute('title');
  if (title && title.trim()) return clip(title, 120);

  const alt = el.getAttribute('alt');
  if (alt && alt.trim()) return clip(alt, 120);

  const tag = el.tagName.toLowerCase();
  const role = el.getAttribute('role');
  if (tag === 'a' || tag === 'button' || role === 'button' || role === 'link') {
    const t = (el.textContent ?? '').trim();
    if (t) return clip(t, 120);
  }
  return undefined;
}

export function semanticRole(el: Element): string {
  const explicit = el.getAttribute('role');
  if (explicit && explicit.trim()) return explicit.trim().toLowerCase();
  const tag = el.tagName.toLowerCase();
  if (tag === 'a') return 'link';
  if (tag === 'button') return 'button';
  if (tag === 'select') return el.hasAttribute('multiple') ? 'listbox' : 'combobox';
  if (tag === 'textarea') return 'textbox';
  if (tag === 'input') {
    const type = (el as HTMLInputElement).type.toLowerCase();
    if (type === 'checkbox') return 'checkbox';
    if (type === 'radio') return 'radio';
    if (type === 'button' || type === 'submit' || type === 'reset') return 'button';
    if (type === 'range') return 'slider';
    if (type === 'search') return 'searchbox';
    return 'textbox';
  }
  return 'generic';
}

const INTERACTIVE_ROLE_ATTRS = new Set([
  'button',
  'link',
  'checkbox',
  'radio',
  'textbox',
  'combobox',
  'listbox',
  'menuitem',
  'tab',
  'switch',
  'option',
  'slider',
  'searchbox',
]);

export function collectInteractive(reg: Registry): {
  elements: InteractiveElement[];
  truncated: boolean;
} {
  const selector = 'a[href], button, input, textarea, select, [role], [tabindex]';
  const seen = new Set<Element>();
  const out: InteractiveElement[] = [];
  let truncated = false;

  const candidates = Array.from(document.querySelectorAll(selector));
  for (const el of candidates) {
    if (seen.has(el)) continue;
    seen.add(el);

    const tag = el.tagName.toLowerCase();
    if (tag === 'input' && (el as HTMLInputElement).type.toLowerCase() === 'hidden') continue;

    const role = semanticRole(el);
    const roleAttr = el.getAttribute('role');
    const tabindexAttr = el.getAttribute('tabindex');

    const isNativeInteractive =
      tag === 'a' || tag === 'button' || tag === 'input' || tag === 'textarea' || tag === 'select';
    const hasInteractiveRole = roleAttr ? INTERACTIVE_ROLE_ATTRS.has(role) : false;
    const hasNonNegativeTabIndex = tabindexAttr !== null && Number(tabindexAttr) >= 0;

    if (!isNativeInteractive && !hasInteractiveRole && !hasNonNegativeTabIndex) continue;
    if (!isVisible(el)) continue;

    if (out.length >= LIMITS.MAX_INTERACTIVE_ELEMENTS) {
      truncated = true;
      break;
    }

    const rect = el.getBoundingClientRect();
    const item: InteractiveElement = {
      id: assignId(reg, el),
      role,
      tag,
      disabled: isDisabled(el),
      visible: true,
      rect: {
        x: Math.round(rect.x),
        y: Math.round(rect.y),
        width: Math.round(rect.width),
        height: Math.round(rect.height),
      },
    };

    const name = accessibleName(el);
    if (name) item.name = name;

    if (tag === 'input') {
      const input = el as HTMLInputElement;
      item.inputType = input.type.toLowerCase();
      if (input.type === 'checkbox' || input.type === 'radio') item.checked = input.checked;
    }
    if (tag === 'option') item.selected = (el as HTMLOptionElement).selected;

    out.push(item);
  }
  return { elements: out, truncated };
}
