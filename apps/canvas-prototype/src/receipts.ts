// Seoul Canvas Design Lab - task receipts.
// Renders the margin receipt for one worklog entry from the canonical task
// snapshot. The verification line shows the receipt's own words - for fixture
// execution that is "fixture contract validated", never "verified": no real
// postcondition was observed and the margin must not claim one. Every entry
// is also labeled synthetic.

import type { TaskSnapshot } from './protocol.js';
import { marginNote } from './provenance-ui.js';

export function receiptMarginNotes(entryNo: number, snapshot: TaskSnapshot): HTMLElement[] {
  const notes: HTMLElement[] = [];
  notes.push(marginNote('m-num', String(entryNo).padStart(3, '0')));
  const receipt = snapshot.receipts?.[0];
  if (receipt) {
    notes.push(marginNote('m-tool', receipt.tool));
    if (receipt.verification.verified) {
      notes.push(marginNote('m-ok', receipt.verification.detail || 'fixture contract validated'));
    } else {
      notes.push(marginNote('m-bad', receipt.status === 'failed' ? 'failed' : 'not validated'));
    }
    notes.push(marginNote('m-route', receipt.route));
  }
  notes.push(marginNote('m-synthetic', 'synthetic demo data'));
  return notes;
}
