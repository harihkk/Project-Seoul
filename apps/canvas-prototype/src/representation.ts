// Seoul Canvas Design Lab - representation controls.
// Renders the plain-text representation switch for an entry and turns a
// selection into a canonical replace_component patch on the SAME surface (the
// compiler recompiles with the same surface id and the stable root component
// id). The patch flows through the surface store's atomic engine and the
// reconciler updates only the root component's element - the artifact, its
// margin receipt, and every unaffected sibling keep their DOM identity.

import type { ComponentType, SemanticResult } from './protocol.js';
import { availableRepresentations, compileInterface } from './compiler.js';
import { elt } from './renderers.js';

const REP_LABEL: Partial<Record<ComponentType, string>> = {
  sortable_table: 'Table',
  bar_chart: 'Bar',
  line_chart: 'Line',
  area_chart: 'Area',
  scatter_chart: 'Scatter',
  entity_card: 'Card',
  key_value_card: 'Details',
  metric: 'Metric',
  comparison_matrix: 'Compare',
  network_graph: 'Graph',
};

export function representationLabel(rep: ComponentType): string {
  return REP_LABEL[rep] ?? rep.replace(/_/g, ' ');
}

/**
 * Builds the canonical patch document that switches `surfaceId` to `rep`.
 * Exposed for tests: the switch is a real surface patch, not a re-render.
 */
export function buildRepresentationPatch(
  result: SemanticResult,
  surfaceId: string,
  rep: ComponentType,
  title: string,
): { patch: unknown; reasons: string[] } {
  const recompiled = compileInterface(result, { title, requestedRepresentation: rep }, surfaceId);
  return {
    patch: {
      surface_id: surfaceId,
      ops: [{ op: 'replace_component', target: 'root', component: recompiled.surface.components[0] }],
    },
    reasons: recompiled.reasons,
  };
}

export function renderRepresentationRow(
  result: SemanticResult,
  current: ComponentType,
  onSwitch: (rep: ComponentType) => void,
): HTMLElement | null {
  const reps = availableRepresentations(result);
  if (reps.length <= 1) return null;
  const row = elt('div', 'rep-row');
  reps.forEach((rep) => {
    const b = elt('button', 'rep-btn', representationLabel(rep)) as HTMLButtonElement;
    if (rep === current) b.classList.add('active');
    b.addEventListener('click', () => onSwitch(rep));
    row.appendChild(b);
  });
  return row;
}
