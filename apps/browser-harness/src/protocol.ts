// Project Seoul Development Harness - wire protocol and shared types.
//
// Pure module: no DOM, no extension APIs, no side effects beyond reading the
// clock in the response builders. It is imported by the background service
// worker, the side panel, and the Node test runner (via type stripping), so it
// must stay free of any browser-only globals.

export const CONTROL_SESSION_STATES = [
  'IDLE',
  'STARTING',
  'READY',
  'OBSERVING',
  'EXECUTING',
  'STOPPED',
  'FAILED',
] as const;
export type ControlSessionState = (typeof CONTROL_SESSION_STATES)[number];

export const REQUEST_KINDS = [
  'SESSION_START',
  'SESSION_STOP',
  'OBSERVE_PAGE',
  'EXECUTE_ACTION',
  'GET_SESSION_STATE',
  'GET_PANEL_CONTEXT',
] as const;
export type RequestKind = (typeof REQUEST_KINDS)[number];

export const ACTION_KINDS = [
  'NAVIGATE',
  'CLICK_ELEMENT',
  'TYPE_TEXT',
  'SCROLL_PAGE',
] as const;
export type ActionKind = (typeof ACTION_KINDS)[number];

export const SCROLL_DIRECTIONS = ['up', 'down'] as const;
export type ScrollDirection = (typeof SCROLL_DIRECTIONS)[number];

export const ERROR_CODES = [
  'INVALID_MESSAGE',
  'INVALID_STATE',
  'UNSUPPORTED_PAGE',
  'SESSION_NOT_FOUND',
  'STALE_SESSION',
  'TAB_MISMATCH',
  'INJECTION_FAILED',
  'ELEMENT_NOT_FOUND',
  'ELEMENT_NOT_INTERACTABLE',
  'SENSITIVE_FIELD',
  'NAVIGATION_REJECTED',
  'STALE_DOCUMENT',
  'STALE_SNAPSHOT',
  'STORAGE_FAILED',
  'ACTION_OUTCOME_UNKNOWN',
  'ACCESS_REQUIRED',
  'ACTION_FAILED',
] as const;
export type ErrorCode = (typeof ERROR_CODES)[number];

export interface StructuredError {
  code: ErrorCode;
  message: string;
}

// --- Typed browser actions (discriminated union on `kind`) ---
//
// Element actions and SCROLL_PAGE carry the document token and (for element
// actions) the snapshot id they were derived from. The content script rejects
// the action if those no longer match the live document/observation.

export interface NavigateAction {
  kind: 'NAVIGATE';
  url: string;
}
export interface ClickElementAction {
  kind: 'CLICK_ELEMENT';
  elementId: string;
  documentToken: string;
  snapshotId: string;
}
export interface TypeTextAction {
  kind: 'TYPE_TEXT';
  elementId: string;
  text: string;
  documentToken: string;
  snapshotId: string;
}
export interface ScrollPageAction {
  kind: 'SCROLL_PAGE';
  direction: ScrollDirection;
  amount: number;
  documentToken: string;
}
export type BrowserAction =
  | NavigateAction
  | ClickElementAction
  | TypeTextAction
  | ScrollPageAction;

// --- Requests (side panel -> background), discriminated on `kind` ---

export interface SessionStartRequest {
  kind: 'SESSION_START';
  id: string;
  sessionId: string;
  tabId: number;
}
export interface SessionStopRequest {
  kind: 'SESSION_STOP';
  id: string;
  sessionId: string;
}
export interface ObservePageRequest {
  kind: 'OBSERVE_PAGE';
  id: string;
  sessionId: string;
}
export interface ExecuteActionRequest {
  kind: 'EXECUTE_ACTION';
  id: string;
  sessionId: string;
  action: BrowserAction;
}
export interface GetSessionStateRequest {
  kind: 'GET_SESSION_STATE';
  id: string;
  sessionId: string;
}
// GET_PANEL_CONTEXT carries no tab id. The side panel cannot choose a tab; the
// background determines the active tab itself. The sessionId here is only a
// correlation token and is never used to select a tab or attachment.
export interface GetPanelContextRequest {
  kind: 'GET_PANEL_CONTEXT';
  id: string;
  sessionId: string;
}
export type ProtocolRequest =
  | SessionStartRequest
  | SessionStopRequest
  | ObservePageRequest
  | ExecuteActionRequest
  | GetSessionStateRequest
  | GetPanelContextRequest;

// --- Responses (background -> side panel) ---

export interface SuccessResponse<T = unknown> {
  id: string;
  success: true;
  timestamp: number;
  sessionId: string;
  tabId?: number;
  result: T;
}
export interface ErrorResponse {
  id: string;
  success: false;
  timestamp: number;
  sessionId: string;
  tabId?: number;
  error: StructuredError;
}
export type ProtocolResponse<T = unknown> = SuccessResponse<T> | ErrorResponse;

// --- Semantic page snapshot ---

export interface ElementRect {
  x: number;
  y: number;
  width: number;
  height: number;
}
export interface InteractiveElement {
  id: string;
  role: string;
  name?: string;
  tag: string;
  inputType?: string;
  disabled: boolean;
  checked?: boolean;
  selected?: boolean;
  visible: boolean;
  rect: ElementRect;
}
export interface PageSnapshot {
  url: string;
  title: string;
  lang: string;
  viewport: { width: number; height: number };
  elements: InteractiveElement[];
  textBlocks: string[];
  timestamp: number;
  truncated: boolean;
  // Binding identifiers: every element action must echo both of these back.
  documentToken: string;
  snapshotId: string;
}

// --- Persisted control-session record (non-sensitive only) ---

export interface SessionTimelineEvent {
  at: number;
  type: string;
  detail?: string;
  state?: ControlSessionState;
}
export interface ControlSessionRecord {
  sessionId: string;
  tabId: number;
  // Origin the session started on, used to confirm a recovered document is
  // still compatible. Never the full URL with path or query.
  origin?: string;
  documentId?: string;
  // Non-sensitive action kind persisted while in EXECUTING, so an interrupted
  // action can be reported. Only the kind, never the payload, and cleared
  // atomically with the final state transition.
  pendingAction?: string;
  state: ControlSessionState;
  createdAt: number;
  updatedAt: number;
  timeline: SessionTimelineEvent[];
  lastError: StructuredError | null;
}

// --- Active-session lookup (panel rehydration) and access state ---
//
// Permission absence and unsupported scheme are different conditions:
//   GRANTED          - activeTab access is available; title/origin are real
//   ACCESS_REQUIRED  - no activeTab grant; the user must click the toolbar action
//   UNSUPPORTED_PAGE - granted, but the scheme (chrome://, etc.) is not supported
//   NO_TAB           - no active tab is available
export type AccessState = 'GRANTED' | 'ACCESS_REQUIRED' | 'UNSUPPORTED_PAGE' | 'NO_TAB';

// Internal page-control attachment for the active tab. This is the live
// content-script binding (a low-level browser-control session).
// It is kept across panel reopen so the user can continue on a granted tab
// without starting another control session.
export interface AttachmentInfo {
  sessionId: string | null;
  attached: boolean;
  // Optional structured reason, surfaced only when an attachment failure is
  // currently relevant.
  reason?: string;
}

// Panel context returned to the side panel: only real, implemented data. The
// full historical control-session record (timeline, stored errors) is never
// exposed to the panel.
export interface PanelContextResult {
  tabId: number | null;
  // Real title/origin only when access is granted; null otherwise (never fake
  // placeholders such as "(untitled)").
  title: string | null;
  origin: string | null;
  access: AccessState;
  accessGranted: boolean;
  supportedScheme: boolean;
  reason: string | null;
  attachment: AttachmentInfo;
}

// --- Background <-> content-script messages ---
//
// SEOUL_INIT and SEOUL_PROBE are internal background-to-content operations. They
// are never reachable from the page: the content listener only accepts messages
// from this extension's own runtime.

export interface ContentInit {
  type: 'SEOUL_INIT';
  sessionId: string;
}
export interface ContentProbe {
  type: 'SEOUL_PROBE';
  sessionId: string;
}
export interface ContentObserve {
  type: 'SEOUL_OBSERVE';
  sessionId: string;
}
export interface ContentClick {
  type: 'SEOUL_CLICK';
  sessionId: string;
  elementId: string;
  documentToken: string;
  snapshotId: string;
}
export interface ContentType {
  type: 'SEOUL_TYPE';
  sessionId: string;
  elementId: string;
  text: string;
  documentToken: string;
  snapshotId: string;
}
export interface ContentScroll {
  type: 'SEOUL_SCROLL';
  sessionId: string;
  direction: ScrollDirection;
  amount: number;
  documentToken: string;
}
export type ContentRequest =
  | ContentInit
  | ContentProbe
  | ContentObserve
  | ContentClick
  | ContentType
  | ContentScroll;
export type ContentResult =
  | { ok: true; data: unknown }
  | { ok: false; code: ErrorCode; message: string };

// Shape the content script returns to a SEOUL_PROBE.
export interface ProbeData {
  sessionId: string | null;
  documentToken: string;
  url: string;
  initialized: boolean;
  snapshotId: string | null;
}

// --- Response builders ---

export function errorOf(code: ErrorCode, message: string): StructuredError {
  return { code, message };
}

export function makeSuccess<T>(
  req: { id: string; sessionId: string },
  result: T,
  tabId?: number,
): SuccessResponse<T> {
  return {
    id: req.id,
    success: true,
    timestamp: Date.now(),
    sessionId: req.sessionId,
    tabId,
    result,
  };
}

export function makeError(
  req: { id: string; sessionId: string },
  error: StructuredError,
  tabId?: number,
): ErrorResponse {
  return {
    id: req.id,
    success: false,
    timestamp: Date.now(),
    sessionId: req.sessionId,
    tabId,
    error,
  };
}
