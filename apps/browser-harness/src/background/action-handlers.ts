// Project Seoul Development Harness - typed action handler registry.
//
// Replaces the former if/else action chain in executeAction, whose final else
// treated any non-click, non-type action as a scroll. Dispatch is now a typed
// registry keyed by ActionKind: each handler owns its argument type, its risk
// and approval metadata, and a pure `plan` that turns the validated action into
// exactly one execution step. There is no fallback branch; an action kind with
// no handler yields an explicit unsupported-action error at resolution time.
//
// Handlers are pure and independently testable: `plan` performs no I/O and
// mutates no session state. The session controller owns the shared
// execute/observe/receipt flow and consumes the plan.

import type {
  ActionKind,
  BrowserAction,
  ClickElementAction,
  ContentRequest,
  NavigateAction,
  ScrollPageAction,
  StructuredError,
  TypeTextAction,
} from '../protocol.ts';
import { validateNavigationUrl } from '../validation.ts';

// Honest side-effect classification, mirroring the native capability model.
// The harness gates all actions behind an explicit user-gesture session start
// rather than a per-action approval prompt, so approval is 'never' here; the
// field exists so the structure matches native Seoul and so risk is explicit.
export type ActionRisk = 'read_only' | 'reversible' | 'external_side_effect';
export type ActionApproval = 'never' | 'first_use' | 'always';

// A planned execution step. Element/scroll actions run in the content script;
// NAVIGATE drives the tabs API and ends the session. Exactly these two modes
// exist, both explicit - not an open-ended action switch.
export type ActionPlan =
  | { mode: 'content'; request: ContentRequest }
  | { mode: 'navigate'; url: string }
  | { mode: 'rejected'; error: StructuredError };

export interface ActionHandler<A extends BrowserAction> {
  readonly kind: A['kind'];
  readonly risk: ActionRisk;
  readonly approval: ActionApproval;
  // Whether an interrupted action with an unknown outcome may be retried
  // automatically. A content mutation must never auto-retry; a pure read may.
  readonly retriableOnUnknownOutcome: boolean;
  plan(sessionId: string, action: A): ActionPlan;
}

const navigateHandler: ActionHandler<NavigateAction> = {
  kind: 'NAVIGATE',
  risk: 'reversible',
  approval: 'never',
  retriableOnUnknownOutcome: false,
  plan(_sessionId, action) {
    const v = validateNavigationUrl(action.url);
    if (!v.ok) return { mode: 'rejected', error: v.error };
    return { mode: 'navigate', url: v.url };
  },
};

const clickHandler: ActionHandler<ClickElementAction> = {
  kind: 'CLICK_ELEMENT',
  risk: 'external_side_effect',
  approval: 'never',
  retriableOnUnknownOutcome: false,
  plan(sessionId, action) {
    return {
      mode: 'content',
      request: {
        type: 'SEOUL_CLICK',
        sessionId,
        elementId: action.elementId,
        documentToken: action.documentToken,
        snapshotId: action.snapshotId,
      },
    };
  },
};

const typeHandler: ActionHandler<TypeTextAction> = {
  kind: 'TYPE_TEXT',
  risk: 'external_side_effect',
  approval: 'never',
  retriableOnUnknownOutcome: false,
  plan(sessionId, action) {
    return {
      mode: 'content',
      request: {
        type: 'SEOUL_TYPE',
        sessionId,
        elementId: action.elementId,
        text: action.text,
        documentToken: action.documentToken,
        snapshotId: action.snapshotId,
      },
    };
  },
};

const scrollHandler: ActionHandler<ScrollPageAction> = {
  kind: 'SCROLL_PAGE',
  risk: 'reversible',
  approval: 'never',
  retriableOnUnknownOutcome: true,
  plan(sessionId, action) {
    return {
      mode: 'content',
      request: {
        type: 'SEOUL_SCROLL',
        sessionId,
        direction: action.direction,
        amount: action.amount,
        documentToken: action.documentToken,
      },
    };
  },
};

// Compile-time exhaustiveness: this mapped type requires an entry for every
// ActionKind. Adding a kind to the union without adding its handler here is a
// type error, and adding a handler for a non-kind is too.
type ActionHandlerRegistry = {
  [K in ActionKind]: ActionHandler<Extract<BrowserAction, { kind: K }>>;
};

export const ACTION_HANDLERS: ActionHandlerRegistry = {
  NAVIGATE: navigateHandler,
  CLICK_ELEMENT: clickHandler,
  TYPE_TEXT: typeHandler,
  SCROLL_PAGE: scrollHandler,
};

// Resolves the handler for an action. Returns null for an unregistered kind so
// the caller emits an explicit unsupported-action error instead of guessing.
export function resolveActionHandler(
  action: BrowserAction,
): ActionHandler<BrowserAction> | null {
  const handler = ACTION_HANDLERS[action.kind] as
    | ActionHandler<BrowserAction>
    | undefined;
  return handler ?? null;
}

// The set of registered action kinds, for tests and the panel.
export const REGISTERED_ACTION_KINDS: readonly ActionKind[] = Object.keys(
  ACTION_HANDLERS,
) as ActionKind[];
