// Seoul Canvas Design Lab - canonical protocol access.
// The single import point for the cross-language wire contract. Types are
// GENERATED from protocol/*.schema.json; the validator interprets the actual
// schema documents (bundled at build time), so the Design Lab validates
// against the same files the native C++ conformance tests consume. The Design
// Lab maintains no protocol model of its own.

import semanticResultSchema from '../../../protocol/semantic-result.schema.json';
import adaptiveSurfaceSchema from '../../../protocol/adaptive-surface.schema.json';
import surfacePatchSchema from '../../../protocol/surface-patch.schema.json';
import componentEventSchema from '../../../protocol/component-event.schema.json';
import taskSnapshotSchema from '../../../protocol/task-snapshot.schema.json';
import capabilityDescriptorSchema from '../../../protocol/capability-descriptor.schema.json';
import { validate, formatErrors, type ValidationResult } from '../../../protocol/ts/validate.mjs';

export * from '../../../protocol/ts/types.js';
export { formatErrors };
export type { ValidationResult };

const registry: Record<string, object> = {
  'semantic-result.schema.json': semanticResultSchema,
  'adaptive-surface.schema.json': adaptiveSurfaceSchema,
  'surface-patch.schema.json': surfacePatchSchema,
  'component-event.schema.json': componentEventSchema,
  'task-snapshot.schema.json': taskSnapshotSchema,
  'capability-descriptor.schema.json': capabilityDescriptorSchema,
};

export function validateSemanticResult(doc: unknown): ValidationResult {
  return validate(doc, semanticResultSchema, registry);
}

export function validateSurface(doc: unknown): ValidationResult {
  return validate(doc, adaptiveSurfaceSchema, registry);
}

export function validateSurfacePatch(doc: unknown): ValidationResult {
  return validate(doc, surfacePatchSchema, registry);
}

export function validateComponentEvent(doc: unknown): ValidationResult {
  return validate(doc, componentEventSchema, registry);
}

export function validateTaskSnapshot(doc: unknown): ValidationResult {
  return validate(doc, taskSnapshotSchema, registry);
}

export function validateCapabilityDescriptor(doc: unknown): ValidationResult {
  return validate(doc, capabilityDescriptorSchema, registry);
}
