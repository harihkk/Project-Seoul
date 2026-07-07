// Seoul Canvas Design Lab - application state.
// One store instance plus the worklog entry list. Entries associate a task
// snapshot with its surface id and mounted elements so the reconciler can
// update exactly one entry in place.

import type { SemanticResult, TaskSnapshot } from './protocol.js';
import type { FixtureCapability } from './fixtures.js';
import { SurfaceStore } from './surface-store.js';

export interface WorklogEntry {
  entryNo: number;
  surfaceId: string;
  snapshot: TaskSnapshot;
  result: SemanticResult;
  capability: FixtureCapability;
  /** How many synthetic stream batches this entry has applied. */
  streamBatches: number;
  /** The <article> element; its body is patched in place, never replaced. */
  el: HTMLElement;
}

export interface AppState {
  store: SurfaceStore;
  entries: WorklogEntry[];
  taskCount: number;
}

export function createAppState(): AppState {
  return { store: new SurfaceStore(), entries: [], taskCount: 0 };
}

export function entryBySurfaceId(state: AppState, surfaceId: string): WorklogEntry | undefined {
  return state.entries.find((e) => e.surfaceId === surfaceId);
}
