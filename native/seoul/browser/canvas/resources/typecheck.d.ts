// Host-only declarations for checking the shipping WebUI without a generated
// Chromium out directory. Chromium's real build uses the authoritative Lit and
// Mojo declarations; this file is excluded from build_webui.

declare module '*canvas.mojom-webui.js' {
  export const ComponentEventKind: {
    kActivate: number;
    kValueChanged: number;
    kSubmit: number;
    kSelect: number;
    kDismiss: number;
  };

  interface Listener {
    addListener(callback: (...args: never[]) => void): void;
  }

  export class PageCallbackRouter {
    $: {bindNewPipeAndPassRemote(): unknown};
    pushSurface: Listener;
    applySurfacePatch: Listener;
    setStatus: Listener;
    pushTaskSnapshot: Listener;
  }

  export interface PageHandler {
    createPageHandler(remote: unknown): void;
    requestInitialState(): void;
    notifyComponentEvent(event: unknown): void;
    submitTurn(input: {text: string}): void;
    pauseTask(taskId: string): void;
    resumeTask(taskId: string): void;
    cancelActiveTask(taskId: string): void;
    approveStep(taskId: string, stepId: string, approved: boolean): void;
    provideTaskInput(taskId: string, stepId: string, input: string): void;
    getLibrarySnapshot(): Promise<{snapshotJson: string}>;
    getStudioSnapshot(): Promise<{snapshotJson: string}>;
    createBoard(name: string): Promise<{snapshotJson: string}>;
    setBoardArchived(boardId: string, archived: boolean): Promise<{snapshotJson: string}>;
    deleteBoard(boardId: string): Promise<{snapshotJson: string}>;
    createRealtimeVoiceSession(): Promise<{sessionJson: string}>;
    submitRealtimeToolCall(call: {
      callId: string;
      name: string;
      argumentsJson: string;
    }): Promise<{outputJson: string}>;
  }

  export const PageHandlerFactory: {getRemote(): PageHandler};
}

declare module '*canvas.css.js' {
  export function getCss(): unknown;
}
