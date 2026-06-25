// Project Seoul Development Harness - pure validation and bounded limits.
//
// This module is the single source of truth for protocol parsing, action
// validation, URL/scheme rules, field classification, scroll clamping, snapshot
// binding, and the observation/limit constants. It is imported by the
// background worker, the side panel, and the Node tests. The content script
// cannot import ES modules, so it mirrors a small subset of this logic locally
// and points back here.

import {
  REQUEST_KINDS,
  ACTION_KINDS,
  SCROLL_DIRECTIONS,
} from './protocol.ts';
import type {
  ProtocolRequest,
  BrowserAction,
  StructuredError,
  ErrorCode,
  ScrollDirection,
  RequestKind,
  ActionKind,
  TimelineEvent,
} from './protocol.ts';

// Centralized, tested limits for bounded observation and persistence.
export const LIMITS = {
  MAX_INTERACTIVE_ELEMENTS: 200,
  MAX_TEXT_BLOCKS: 100,
  MAX_TOTAL_TEXT_CHARS: 30_000,
  MAX_TEXT_BLOCK_CHARS: 2_000,
  MAX_TIMELINE_EVENTS: 200,
  MAX_TYPE_TEXT_CHARS: 5_000,
  SCROLL_MIN: 0,
  SCROLL_MAX: 20_000,
  SCROLL_DEFAULT: 600,
} as const;

// Input types (plus the empty default) that may receive free typed text.
export const TEXT_CAPABLE_INPUT_TYPES = [
  '',
  'text',
  'search',
  'url',
  'tel',
  'email',
  'number',
] as const;

function invalid(message: string): { ok: false; error: StructuredError } {
  return { ok: false, error: { code: 'INVALID_MESSAGE', message } };
}

function isNonEmptyString(v: unknown): v is string {
  return typeof v === 'string' && v.length > 0;
}

// --- URL / navigation ---

export function isHttpUrl(raw: unknown): raw is string {
  if (typeof raw !== 'string') return false;
  let parsed: URL;
  try {
    parsed = new URL(raw);
  } catch {
    return false;
  }
  return parsed.protocol === 'http:' || parsed.protocol === 'https:';
}

export function validateNavigationUrl(
  raw: unknown,
): { ok: true; url: string } | { ok: false; error: StructuredError } {
  if (typeof raw !== 'string' || raw.length === 0) {
    return {
      ok: false,
      error: { code: 'NAVIGATION_REJECTED', message: 'URL must be a non-empty string.' },
    };
  }
  let parsed: URL;
  try {
    parsed = new URL(raw);
  } catch {
    return {
      ok: false,
      error: { code: 'NAVIGATION_REJECTED', message: 'URL must be absolute and well-formed.' },
    };
  }
  if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
    return {
      ok: false,
      error: {
        code: 'NAVIGATION_REJECTED',
        message: `Scheme "${parsed.protocol}" is not allowed; use http: or https:.`,
      },
    };
  }
  return { ok: true, url: parsed.toString() };
}

// --- Scroll ---

export function isScrollDirection(value: unknown): value is ScrollDirection {
  return typeof value === 'string' && (SCROLL_DIRECTIONS as readonly string[]).includes(value);
}

export function clampScroll(amount: unknown): number {
  const n = typeof amount === 'number' ? amount : Number(amount);
  if (!Number.isFinite(n)) return LIMITS.SCROLL_DEFAULT;
  return Math.min(Math.max(Math.round(n), LIMITS.SCROLL_MIN), LIMITS.SCROLL_MAX);
}

// --- Typed text ---

export function validateTypeText(
  text: unknown,
): { ok: true; text: string } | { ok: false; error: StructuredError } {
  if (typeof text !== 'string') {
    return invalid('TYPE_TEXT "text" must be a string.');
  }
  if (text.length > LIMITS.MAX_TYPE_TEXT_CHARS) {
    return invalid('TYPE_TEXT "text" exceeds the maximum allowed length.');
  }
  return { ok: true, text };
}

// --- Document token / snapshot binding ---

// A document token is a random, opaque identifier minted per document. Accept
// any non-empty token of plausible length; the real authority is equality with
// the live content script's token, checked by checkSnapshotBinding.
export function isValidDocumentToken(token: unknown): token is string {
  return typeof token === 'string' && token.length >= 16 && token.length <= 128;
}

export interface SnapshotBinding {
  documentToken: string | null;
  snapshotId: string | null;
}

// Decide whether an element/scroll action may run against the current document
// and observation. SCROLL_PAGE sets requireSnapshot=false (no element involved).
export function checkSnapshotBinding(
  current: SnapshotBinding,
  provided: { documentToken: string; snapshotId?: string },
  requireSnapshot: boolean,
): { ok: true } | { ok: false; error: StructuredError } {
  if (!current.documentToken || current.documentToken !== provided.documentToken) {
    return {
      ok: false,
      error: {
        code: 'STALE_DOCUMENT',
        message: 'Document token does not match the current document.',
      },
    };
  }
  if (requireSnapshot) {
    if (!current.snapshotId) {
      return {
        ok: false,
        error: {
          code: 'STALE_SNAPSHOT',
          message: 'No current observation snapshot; observe the page first.',
        },
      };
    }
    if (provided.snapshotId === undefined || current.snapshotId !== provided.snapshotId) {
      return {
        ok: false,
        error: {
          code: 'STALE_SNAPSHOT',
          message: 'Snapshot id does not match the latest observation.',
        },
      };
    }
  }
  return { ok: true };
}

// --- Field classification (shared decision for TYPE_TEXT safety) ---

export interface FieldDescriptor {
  tag: string; // lowercased tag name
  type: string; // lowercased input type ('' for non-input elements)
  disabled: boolean;
  readOnly: boolean;
  connected: boolean;
  visible: boolean;
  valueLength: number;
}

function deny(code: ErrorCode, message: string): { allowed: false; error: StructuredError } {
  return { allowed: false, error: { code, message } };
}

export function classifyTypeTarget(
  field: FieldDescriptor,
): { allowed: true } | { allowed: false; error: StructuredError } {
  const tag = field.tag.toLowerCase();
  if (tag !== 'input' && tag !== 'textarea') {
    return deny('ELEMENT_NOT_INTERACTABLE', 'Only <input> and <textarea> accept typed text.');
  }
  const type = (field.type || '').toLowerCase();
  if (tag === 'input') {
    if (type === 'password') {
      return deny('SENSITIVE_FIELD', 'Refusing to type into a password field.');
    }
    if (type === 'file') {
      return deny('SENSITIVE_FIELD', 'Refusing to interact with a file input.');
    }
    if (type === 'hidden') {
      return deny('ELEMENT_NOT_INTERACTABLE', 'Hidden inputs cannot receive typed text.');
    }
  }
  if (!field.connected) {
    return deny('ELEMENT_NOT_FOUND', 'Element is no longer connected to the document.');
  }
  if (!field.visible) {
    return deny('ELEMENT_NOT_INTERACTABLE', 'Element is not visible.');
  }
  if (field.disabled) {
    return deny('ELEMENT_NOT_INTERACTABLE', 'Element is disabled.');
  }
  if (field.readOnly) {
    return deny('ELEMENT_NOT_INTERACTABLE', 'Element is read-only.');
  }
  if (tag === 'input' && !(TEXT_CAPABLE_INPUT_TYPES as readonly string[]).includes(type)) {
    return deny('ELEMENT_NOT_INTERACTABLE', `Input type "${type}" does not accept free text.`);
  }
  if (field.valueLength > 0) {
    return deny(
      'ACTION_FAILED',
      'Field already contains a value; clearing is not supported in this milestone.',
    );
  }
  return { allowed: true };
}

// --- Timeline ---

export function truncateTimeline(
  events: readonly TimelineEvent[],
  max: number = LIMITS.MAX_TIMELINE_EVENTS,
): TimelineEvent[] {
  if (events.length <= max) return events.slice();
  return events.slice(events.length - max);
}

// --- Action validation ---

export function validateAction(
  raw: unknown,
): { ok: true; action: BrowserAction } | { ok: false; error: StructuredError } {
  if (typeof raw !== 'object' || raw === null) {
    return invalid('Action must be an object.');
  }
  const a = raw as Record<string, unknown>;
  if (typeof a.kind !== 'string' || !(ACTION_KINDS as readonly string[]).includes(a.kind)) {
    return invalid('Unknown or missing action kind.');
  }
  const kind = a.kind as ActionKind;
  switch (kind) {
    case 'NAVIGATE': {
      const v = validateNavigationUrl(a.url);
      if (!v.ok) return { ok: false, error: v.error };
      return { ok: true, action: { kind: 'NAVIGATE', url: v.url } };
    }
    case 'CLICK_ELEMENT': {
      if (!isNonEmptyString(a.elementId)) return invalid('CLICK_ELEMENT requires "elementId".');
      if (!isNonEmptyString(a.documentToken)) return invalid('CLICK_ELEMENT requires "documentToken".');
      if (!isNonEmptyString(a.snapshotId)) return invalid('CLICK_ELEMENT requires "snapshotId".');
      return {
        ok: true,
        action: {
          kind: 'CLICK_ELEMENT',
          elementId: a.elementId,
          documentToken: a.documentToken,
          snapshotId: a.snapshotId,
        },
      };
    }
    case 'TYPE_TEXT': {
      if (!isNonEmptyString(a.elementId)) return invalid('TYPE_TEXT requires "elementId".');
      if (!isNonEmptyString(a.documentToken)) return invalid('TYPE_TEXT requires "documentToken".');
      if (!isNonEmptyString(a.snapshotId)) return invalid('TYPE_TEXT requires "snapshotId".');
      const t = validateTypeText(a.text);
      if (!t.ok) return { ok: false, error: t.error };
      return {
        ok: true,
        action: {
          kind: 'TYPE_TEXT',
          elementId: a.elementId,
          text: t.text,
          documentToken: a.documentToken,
          snapshotId: a.snapshotId,
        },
      };
    }
    case 'SCROLL_PAGE': {
      if (!isScrollDirection(a.direction)) {
        return invalid('SCROLL_PAGE requires direction "up" or "down".');
      }
      if (!isNonEmptyString(a.documentToken)) return invalid('SCROLL_PAGE requires "documentToken".');
      return {
        ok: true,
        action: {
          kind: 'SCROLL_PAGE',
          direction: a.direction,
          amount: clampScroll(a.amount),
          documentToken: a.documentToken,
        },
      };
    }
  }
  return invalid('Unhandled action kind.');
}

// --- Request parsing ---

export function parseRequest(
  raw: unknown,
): { ok: true; request: ProtocolRequest } | { ok: false; error: StructuredError } {
  if (typeof raw !== 'object' || raw === null) {
    return invalid('Request must be an object.');
  }
  const r = raw as Record<string, unknown>;
  if (typeof r.id !== 'string' || r.id.length === 0) {
    return invalid('Request requires a non-empty string "id".');
  }
  if (typeof r.sessionId !== 'string' || r.sessionId.length === 0) {
    return invalid('Request requires a non-empty string "sessionId".');
  }
  if (typeof r.kind !== 'string' || !(REQUEST_KINDS as readonly string[]).includes(r.kind)) {
    return invalid('Unknown or missing request kind.');
  }
  const kind = r.kind as RequestKind;
  switch (kind) {
    case 'SESSION_START': {
      if (typeof r.tabId !== 'number' || !Number.isInteger(r.tabId)) {
        return invalid('SESSION_START requires an integer "tabId".');
      }
      return {
        ok: true,
        request: { kind: 'SESSION_START', id: r.id, sessionId: r.sessionId, tabId: r.tabId },
      };
    }
    case 'EXECUTE_ACTION': {
      const v = validateAction(r.action);
      if (!v.ok) return { ok: false, error: v.error };
      return {
        ok: true,
        request: { kind: 'EXECUTE_ACTION', id: r.id, sessionId: r.sessionId, action: v.action },
      };
    }
    case 'SESSION_STOP':
      return { ok: true, request: { kind: 'SESSION_STOP', id: r.id, sessionId: r.sessionId } };
    case 'OBSERVE_PAGE':
      return { ok: true, request: { kind: 'OBSERVE_PAGE', id: r.id, sessionId: r.sessionId } };
    case 'GET_TASK_STATE':
      return { ok: true, request: { kind: 'GET_TASK_STATE', id: r.id, sessionId: r.sessionId } };
    case 'GET_PANEL_CONTEXT':
      // Deliberately ignore any tabId on the raw payload: the side panel must
      // not be able to choose the tab the context is computed for.
      return { ok: true, request: { kind: 'GET_PANEL_CONTEXT', id: r.id, sessionId: r.sessionId } };
  }
  return invalid('Unhandled request kind.');
}
