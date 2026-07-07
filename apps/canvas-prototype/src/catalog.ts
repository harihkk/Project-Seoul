// Seoul Canvas Design Lab - the fixture capability catalog UI.
// The index rows are GENERATED from the registered fixture capability
// descriptors (protocol/fixtures/catalog.json + capability descriptors);
// nothing here is hand-listed. The heading names the data as synthetic.

import { capabilities } from './runtime.js';
import { elt } from './renderers.js';

export function renderCatalogIndex(onRun: (goal: string) => void): HTMLElement {
  const index = elt('section', 'cap-index');
  index.appendChild(elt('div', 'index-heading', 'Index of fixture capabilities'));
  index.appendChild(
    elt('div', 'index-note', 'Synthetic demo data: each row runs a registered fixture that returns a canned payload. No real browser observation occurs in the Design Lab.'),
  );
  capabilities().forEach((capability, i) => {
    const row = elt('button', 'index-row') as HTMLButtonElement;
    row.appendChild(elt('span', 'index-num', String(i + 1).padStart(2, '0')));
    row.appendChild(elt('span', 'index-name', capability.descriptor.name));
    row.appendChild(elt('span', 'index-shape', capability.result.schema.shape.replace(/_/g, ' ')));
    row.addEventListener('click', () => onRun(capability.exampleGoal));
    index.appendChild(row);
  });
  return index;
}
