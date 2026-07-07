// Seoul Canvas Design Lab - the surface store and patch engine.
// Holds every live surface by stable id and applies canonical surface-patch
// documents (protocol/surface-patch.schema.json) with the same semantics as
// the native ApplySurfacePatch (saui/saui_patch.cc): a patch first validates
// against the canonical schema, then applies to a working copy op by op, then
// the whole resulting surface is re-checked (bindings must resolve); only if
// everything holds does the stored surface advance, with its revision bumped
// once. Invalid targets, dangling bindings, or any failed op leave the
// surface untouched.

import type {
  AdaptiveSurface,
  ComponentNode,
  DataEntry,
  SeriesPoint,
  SurfacePatch,
} from './protocol.js';
import { formatErrors, validateSurface, validateSurfacePatch } from './protocol.js';
import componentCatalog from '../../../protocol/component-catalog.json';

// The component catalog and structural limits are GENERATED from the native
// sources (saui_catalog.cc, saui_limits.h) and drift-gated in CI; the Lab
// enforces the same rules the native engine does, from the same data.
interface CatalogEntry {
  container: boolean;
  binding_kinds: string[];
  binding_required: boolean;
  requires_accessible_name: boolean;
  required_props: string[];
}
const CATALOG = componentCatalog.components as unknown as Record<string, CatalogEntry>;
const LIMITS = componentCatalog.limits as Record<string, number>;

export interface AppliedPatch {
  new_revision: number;
  changed_component_ids: string[];
  changed_entry_names: string[];
  title_changed: boolean;
  actions_changed: boolean;
}

export type PatchOutcome =
  | { ok: true; applied: AppliedPatch; surface: AdaptiveSurface }
  | { ok: false; error: string };

interface StoredSurface {
  surface: AdaptiveSurface;
  revision: number;
}

function clone<T>(value: T): T {
  return structuredClone(value);
}

/** Finds a component by stable id anywhere in a surface's tree. */
export function findComponentById(surface: AdaptiveSurface, id: string): ComponentNode | undefined {
  return findComponent(surface.components, id);
}

/** The parent of a component, or undefined for top-level components. */
export function parentOfComponent(surface: AdaptiveSurface, id: string): ComponentNode | undefined {
  const walk = (nodes: ComponentNode[], parent: ComponentNode | undefined): ComponentNode | undefined => {
    for (const node of nodes) {
      if (node.id === id) return parent;
      const found = walk(node.children ?? [], node);
      if (found) return found;
    }
    return undefined;
  };
  return walk(surface.components, undefined);
}

function findComponent(nodes: ComponentNode[], id: string): ComponentNode | undefined {
  for (const node of nodes) {
    if (node.id === id) return node;
    const inChildren = findComponent(node.children ?? [], id);
    if (inChildren) return inChildren;
  }
  return undefined;
}

function removeComponent(nodes: ComponentNode[], id: string): boolean {
  const index = nodes.findIndex((n) => n.id === id);
  if (index !== -1) {
    nodes.splice(index, 1);
    return true;
  }
  return nodes.some((n) => removeComponent(n.children ?? [], id));
}

function replaceComponent(nodes: ComponentNode[], id: string, replacement: ComponentNode): boolean {
  const index = nodes.findIndex((n) => n.id === id);
  if (index !== -1) {
    nodes[index] = replacement;
    return true;
  }
  return nodes.some((n) => replaceComponent(n.children ?? [], id, replacement));
}

function countComponents(nodes: ComponentNode[]): number {
  return nodes.reduce((sum, n) => sum + 1 + countComponents(n.children ?? []), 0);
}

function subtreeHeight(node: ComponentNode): number {
  const children = node.children ?? [];
  return 1 + (children.length === 0 ? 0 : Math.max(...children.map(subtreeHeight)));
}

function depthOf(nodes: ComponentNode[], id: string, depth = 1): number | null {
  for (const node of nodes) {
    if (node.id === id) return depth;
    const found = depthOf(node.children ?? [], id, depth + 1);
    if (found !== null) return found;
  }
  return null;
}

/**
 * The same whole-surface semantic re-check the native ApplySurfacePatch runs
 * (ValidateSurface): unique component ids, catalog rules (containers,
 * required props, accessible names, binding kinds), resolvable bindings and
 * action references, and the structural limits. Exported so the compiler can
 * self-check its output against the identical rules.
 */
export function validateSurfaceSemantics(surface: AdaptiveSurface): string | null {
  if (countComponents(surface.components) > LIMITS['max_surface_components']) {
    return `surface exceeds ${LIMITS['max_surface_components']} components`;
  }
  const actionIds = new Set<string>();
  for (const action of surface.actions ?? []) {
    if (actionIds.has(action.id)) return `duplicate action id "${action.id}"`;
    actionIds.add(action.id);
  }
  const seen = new Set<string>();
  const walk = (nodes: ComponentNode[], depth: number): string | null => {
    if (depth > LIMITS['max_component_depth']) {
      return `component tree deeper than ${LIMITS['max_component_depth']}`;
    }
    for (const node of nodes) {
      if (seen.has(node.id)) return `duplicate component id "${node.id}"`;
      seen.add(node.id);
      const info = CATALOG[node.type];
      if (!info) return `unknown component type "${node.type}"`;
      const children = node.children ?? [];
      if (children.length > 0 && !info.container) {
        return `component "${node.id}" (${node.type}) is not a container`;
      }
      if (children.length > LIMITS['max_children_per_component']) {
        return `component "${node.id}" exceeds ${LIMITS['max_children_per_component']} children`;
      }
      for (const prop of info.required_props) {
        if (!(node.props && prop in node.props)) {
          return `component "${node.id}" (${node.type}) is missing required prop "${prop}"`;
        }
      }
      if (info.requires_accessible_name && !node.accessible_name) {
        return `component "${node.id}" (${node.type}) requires an accessible name`;
      }
      const bound = node.bindings?.['data'];
      if (info.binding_required && !bound) {
        return `component "${node.id}" (${node.type}) requires a data binding`;
      }
      for (const [slot, entryName] of Object.entries(node.bindings ?? {})) {
        const entry = surface.data?.[entryName];
        if (!entry) return `binding references missing data entry "${entryName}"`;
        if (slot === 'data' && info.binding_kinds.length > 0 && !info.binding_kinds.includes(entry.kind)) {
          return `component "${node.id}" (${node.type}) cannot bind a ${entry.kind} entry`;
        }
      }
      for (const actionId of node.actions ?? []) {
        if (!actionIds.has(actionId)) {
          return `component "${node.id}" references unknown action "${actionId}"`;
        }
      }
      const childError = walk(children, depth + 1);
      if (childError) return childError;
    }
    return null;
  };
  return walk(surface.components, 1);
}

export class SurfaceStore {
  private surfaces = new Map<string, StoredSurface>();

  put(surface: AdaptiveSurface): void {
    if (!surface.id) throw new Error('surface must carry an id');
    this.surfaces.set(surface.id, { surface: clone(surface), revision: 1 });
  }

  get(id: string): AdaptiveSurface | undefined {
    return this.surfaces.get(id)?.surface;
  }

  revision(id: string): number {
    return this.surfaces.get(id)?.revision ?? 0;
  }

  /**
   * Applies one canonical patch document atomically. The document is first
   * validated against surface-patch.schema.json, so a malformed patch (or an
   * op kind outside the canonical ten) never reaches the engine.
   */
  applyPatch(patchDoc: unknown): PatchOutcome {
    const valid = validateSurfacePatch(patchDoc);
    if (!valid.valid) {
      return { ok: false, error: `patch rejected by canonical schema:\n${formatErrors(valid.errors)}` };
    }
    const patch = patchDoc as SurfacePatch;
    const stored = this.surfaces.get(patch.surface_id);
    if (!stored) {
      return { ok: false, error: `no surface with id ${patch.surface_id}` };
    }

    const working = clone(stored.surface);
    const applied: AppliedPatch = {
      new_revision: 0,
      changed_component_ids: [],
      changed_entry_names: [],
      title_changed: false,
      actions_changed: false,
    };

    for (const op of patch.ops) {
      const failure = this.applyOp(working, op, applied);
      if (failure) {
        return { ok: false, error: failure };
      }
    }

    const semantic = validateSurfaceSemantics(working);
    if (semantic) {
      return { ok: false, error: `patch would leave the surface invalid: ${semantic}` };
    }
    const surfaceValid = validateSurface(working);
    if (!surfaceValid.valid) {
      return { ok: false, error: `patched surface is non-canonical:\n${formatErrors(surfaceValid.errors)}` };
    }

    stored.revision += 1;
    stored.surface = working;
    applied.new_revision = stored.revision;
    return { ok: true, applied, surface: working };
  }

  private applyOp(
    surface: AdaptiveSurface,
    op: SurfacePatch['ops'][number],
    applied: AppliedPatch,
  ): string | null {
    switch (op.op) {
      case 'set_props': {
        const node = findComponent(surface.components, op.target);
        if (!node) return `set_props: no component "${op.target}"`;
        node.props = { ...(node.props ?? {}), ...op.props };
        applied.changed_component_ids.push(node.id);
        return null;
      }
      case 'set_state': {
        const node = findComponent(surface.components, op.target);
        if (!node) return `set_state: no component "${op.target}"`;
        node.state = op.state;
        // Native semantics: an absent message CLEARS the previous one (the
        // native parser defaults it to empty and assigns unconditionally).
        if (op.message !== undefined) node.state_message = op.message;
        else delete node.state_message;
        applied.changed_component_ids.push(node.id);
        return null;
      }
      case 'set_title': {
        surface.title = op.title;
        applied.title_changed = true;
        return null;
      }
      case 'set_bindings': {
        const node = findComponent(surface.components, op.target);
        if (!node) return `set_bindings: no component "${op.target}"`;
        node.bindings = { ...op.bindings };
        applied.changed_component_ids.push(node.id);
        return null;
      }
      case 'upsert_data_entry': {
        surface.data = surface.data ?? {};
        surface.data[op.entry] = op.value as DataEntry;
        applied.changed_entry_names.push(op.entry);
        return null;
      }
      case 'append_series_points': {
        const entry = surface.data?.[op.entry];
        if (!entry) return `append_series_points: no data entry "${op.entry}"`;
        if (entry.kind !== 'series') return `append_series_points: entry "${op.entry}" is not a series`;
        entry.points = [...(entry.points ?? []), ...(op.points as SeriesPoint[])];
        applied.changed_entry_names.push(op.entry);
        return null;
      }
      case 'append_child': {
        const parent = findComponent(surface.components, op.target);
        if (!parent) return `append_child: no component "${op.target}"`;
        if (!CATALOG[parent.type]?.container) {
          return `append_child: "${op.target}" (${parent.type}) is not a container`;
        }
        if (findComponent(surface.components, op.component.id)) {
          return `append_child: component id "${op.component.id}" already exists`;
        }
        const child = op.component as ComponentNode;
        const parentDepth = depthOf(surface.components, parent.id) ?? 1;
        if (parentDepth + subtreeHeight(child) > LIMITS['max_component_depth']) {
          return `append_child: would exceed depth ${LIMITS['max_component_depth']}`;
        }
        parent.children = [...(parent.children ?? []), child];
        applied.changed_component_ids.push(parent.id, op.component.id);
        return null;
      }
      case 'remove_component': {
        if (!removeComponent(surface.components, op.target)) {
          return `remove_component: no component "${op.target}"`;
        }
        if (surface.components.length === 0) {
          return 'remove_component: a surface must keep at least one component';
        }
        applied.changed_component_ids.push(op.target);
        return null;
      }
      case 'replace_component': {
        if (!replaceComponent(surface.components, op.target, op.component as ComponentNode)) {
          return `replace_component: no component "${op.target}"`;
        }
        // When the replacement changes the id, the old id is reported too so
        // renderers drop the stale element (mirrored natively).
        if (op.component.id !== op.target) applied.changed_component_ids.push(op.target);
        applied.changed_component_ids.push(op.component.id);
        return null;
      }
      case 'set_actions': {
        surface.actions = op.actions as AdaptiveSurface['actions'];
        applied.actions_changed = true;
        return null;
      }
    }
  }
}
