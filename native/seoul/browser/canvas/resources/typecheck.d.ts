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
    setPageContext: Listener;
    pushTaskSnapshot: Listener;
  }

  export class PageHandlerRemote {
    $: {bindNewPipeAndPassReceiver(): unknown};
    requestInitialState(): void;
    notifyComponentEvent(event: unknown): void;
    submitTurn(input: {text: string}): void;
    pauseTask(taskId: string): void;
    resumeTask(taskId: string): void;
    cancelActiveTask(taskId: string): void;
    approveStep(taskId: string, stepId: string, approved: boolean): void;
    provideTaskInput(taskId: string, stepId: string, input: string): void;
    getLibrarySnapshot(): Promise<{snapshotJson: string}>;
    getSiteLayerSnapshot(): Promise<{snapshotJson: string}>;
    upsertSiteLayer(
        layerId: string, name: string, originPattern: string,
        sceneScope: string, enabled: boolean,
        adjustments: Array<{
          kind: string;
          selectors: string[];
          textValue: string;
          numericValue: number;
          density: string;
        }>): Promise<{snapshotJson: string}>;
    setSiteLayerEnabled(
        layerId: string, enabled: boolean): Promise<{snapshotJson: string}>;
    deleteSiteLayer(layerId: string): Promise<{snapshotJson: string}>;
    getStudioSnapshot(): Promise<{snapshotJson: string}>;
    saveLocalProvider(
        endpointUrl: string,
        modelId: string): Promise<{snapshotJson: string}>;
    clearLocalProvider(): Promise<{snapshotJson: string}>;
    checkLocalProvider(): Promise<{snapshotJson: string}>;
    saveCloudProvider(
        modelId: string, enabled: boolean, reasoningSecret: string,
        voiceSecret: string): Promise<{snapshotJson: string}>;
    clearCloudProvider(): Promise<{snapshotJson: string}>;
    createBoard(name: string): Promise<{snapshotJson: string}>;
    renameBoard(boardId: string, name: string): Promise<{snapshotJson: string}>;
    setBoardArchived(boardId: string, archived: boolean): Promise<{snapshotJson: string}>;
    deleteBoard(boardId: string): Promise<{snapshotJson: string}>;
    addBoardElement(
        boardId: string, elementId: string, kind: string, title: string,
        text: string, reference: string, origin: string, x: number, y: number,
        width: number, height: number,
        zIndex: number): Promise<{snapshotJson: string}>;
    updateBoardElement(
        boardId: string, elementId: string, kind: string, title: string,
        text: string, reference: string, origin: string, x: number, y: number,
        width: number, height: number,
        zIndex: number): Promise<{snapshotJson: string}>;
    removeBoardElement(
        boardId: string, elementId: string): Promise<{snapshotJson: string}>;
    createRealtimeVoiceSession(): Promise<{sessionJson: string}>;
    submitRealtimeToolCall(call: {
      callId: string;
      name: string;
      argumentsJson: string;
    }): Promise<{outputJson: string}>;
  }

  export const PageHandlerFactory: {
    getRemote(): {
      createPageHandler(page: unknown, handler: unknown): void;
    };
  };
}

declare module '*canvas.css.js' {
  export function getCss(): unknown;
}
