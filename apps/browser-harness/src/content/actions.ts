// Project Seoul Development Harness - content: DOM actions.
//
// click / type / scroll against snapshot-bound elements. All safety decisions -
// snapshot binding, field classification, scroll clamping - come from the
// single validation source of truth (../validation.ts); this module holds no
// duplicated limits or rules. It maps a shared StructuredError into the
// content-result shape and performs the DOM mutation.

import {
  checkSnapshotBinding,
  classifyTypeTarget,
  clampScroll,
  type FieldDescriptor,
} from '../validation.ts';
import type { ContentResult, ScrollDirection } from '../protocol.ts';
import { isDisabled, isVisible } from './semantics.ts';
import { resolveElement, type HarnessState } from './document-session.ts';

// Apply the shared snapshot binding rule to the current document state.
function checkBinding(
  state: HarnessState,
  documentToken: string,
  snapshotId: string | undefined,
  requireSnapshot: boolean,
): ContentResult | null {
  const verdict = checkSnapshotBinding(
    { documentToken: state.documentToken, snapshotId: state.snapshotId },
    snapshotId === undefined ? { documentToken } : { documentToken, snapshotId },
    requireSnapshot,
  );
  if (verdict.ok) return null;
  return { ok: false, code: verdict.error.code, message: verdict.error.message };
}

export function clickElement(
  state: HarnessState,
  elementId: string,
  documentToken: string,
  snapshotId: string,
): ContentResult {
  const binding = checkBinding(state, documentToken, snapshotId, true);
  if (binding) return binding;
  const el = resolveElement(state, elementId);
  if (!el || !el.isConnected) {
    return { ok: false, code: 'ELEMENT_NOT_FOUND', message: 'Unknown or detached element id.' };
  }
  if (!isVisible(el)) {
    return { ok: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is not visible.' };
  }
  if (isDisabled(el)) {
    return { ok: false, code: 'ELEMENT_NOT_INTERACTABLE', message: 'Element is disabled.' };
  }
  el.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  (el as HTMLElement).click();
  return { ok: true, data: { clicked: true, id: elementId } };
}

function setNativeValue(el: HTMLInputElement | HTMLTextAreaElement, value: string): void {
  const proto =
    el instanceof HTMLTextAreaElement ? HTMLTextAreaElement.prototype : HTMLInputElement.prototype;
  const desc = Object.getOwnPropertyDescriptor(proto, 'value');
  if (desc && typeof desc.set === 'function') {
    desc.set.call(el, value);
  } else {
    el.value = value;
  }
}

export function typeText(
  state: HarnessState,
  elementId: string,
  text: string,
  documentToken: string,
  snapshotId: string,
): ContentResult {
  const binding = checkBinding(state, documentToken, snapshotId, true);
  if (binding) return binding;
  const el = resolveElement(state, elementId);
  if (!el) {
    return { ok: false, code: 'ELEMENT_NOT_FOUND', message: 'Unknown element id.' };
  }
  const tag = el.tagName.toLowerCase();
  const typed = el as HTMLInputElement | HTMLTextAreaElement;
  const descriptor: FieldDescriptor = {
    tag,
    type: tag === 'input' ? (el as HTMLInputElement).type.toLowerCase() : '',
    disabled: isDisabled(el),
    readOnly: 'readOnly' in typed ? Boolean(typed.readOnly) : false,
    connected: el.isConnected,
    visible: isVisible(el),
    // Length only - the field's existing value is never read into a string we
    // keep, transmit, or persist.
    valueLength: 'value' in typed ? String(typed.value).length : 0,
  };
  const verdict = classifyTypeTarget(descriptor);
  if (!verdict.allowed) {
    return { ok: false, code: verdict.error.code, message: verdict.error.message };
  }
  el.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  setNativeValue(typed, text);
  typed.dispatchEvent(new Event('input', { bubbles: true }));
  typed.dispatchEvent(new Event('change', { bubbles: true }));
  return { ok: true, data: { typed: true, id: elementId, length: text.length } };
}

export function scrollPage(
  state: HarnessState,
  direction: ScrollDirection,
  amount: number,
  documentToken: string,
): ContentResult {
  const binding = checkBinding(state, documentToken, undefined, false);
  if (binding) return binding;
  const px = clampScroll(amount);
  window.scrollBy({ top: direction === 'down' ? px : -px, left: 0, behavior: 'auto' });
  return {
    ok: true,
    data: { x: Math.round(window.scrollX), y: Math.round(window.scrollY), applied: px, direction },
  };
}
