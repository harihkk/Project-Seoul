// Public logic surface of the Design Lab (no DOM), for tests and reuse.
export * from './protocol.js';
export * from './semantics.js';
export * from './compiler.js';
export * from './runtime.js';
export * from './surface-store.js';
export { summarizeResult } from './voice-summary.js';
export { buildRepresentationPatch, representationLabel } from './representation.js';
export { loadFixtureCatalog, SYNTHETIC_DATA_NOTICE, type FixtureCapability } from './fixtures.js';
