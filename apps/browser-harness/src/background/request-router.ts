// Project Seoul Development Harness - trusted request router.
//
// Parses and routes one protocol request to the session controller. The
// request-kind switch is exhaustive over the closed RequestKind union (tsc
// noFallthroughCasesInSwitch + the union type guarantee coverage); there is no
// default branch. Trust enforcement lives in the entry (only the extension's
// own side panel sender is accepted); this module assumes an already-trusted
// message and validates its shape.

import type { ErrorCode, ProtocolResponse } from '../protocol.ts';
import { parseRequest } from '../validation.ts';
import type { SessionController } from './session-controller.ts';

function staticError(code: ErrorCode, message: string): ProtocolResponse {
  return {
    id: 'unknown',
    success: false,
    timestamp: Date.now(),
    sessionId: 'unknown',
    error: { code, message },
  };
}

export async function routeRequest(
  controller: SessionController,
  message: unknown,
): Promise<ProtocolResponse> {
  const parsed = parseRequest(message);
  if (!parsed.ok) return staticError(parsed.error.code, parsed.error.message);
  const req = parsed.request;
  switch (req.kind) {
    case 'SESSION_START':
      return controller.startSession(req);
    case 'SESSION_STOP':
      return controller.stopSession(req);
    case 'OBSERVE_PAGE':
      return controller.observePage(req);
    case 'EXECUTE_ACTION':
      return controller.executeAction(req);
    case 'GET_SESSION_STATE':
      return controller.getSessionState(req);
    case 'GET_PANEL_CONTEXT':
      return controller.getPanelContext(req);
  }
}
