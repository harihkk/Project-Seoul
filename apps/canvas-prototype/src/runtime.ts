// Seoul Canvas Design Lab - the task FIXTURE runtime.
// Registers the fixture capability catalog (canonical descriptors + canned
// semantic results from the shared protocol corpus), routes goals with a
// LEXICAL TOKEN-OVERLAP planner over each descriptor's own text (id, name,
// description, tags - data-driven, no keyword table), executes the selected
// fixture, and emits a canonical task snapshot with an honest receipt.
//
// HONESTY BOUNDARY: execution here returns a canned payload. "Verification"
// is that the returned document VALIDATES against the canonical
// semantic-result schema - recorded as method "fixture_contract" with detail
// "fixture contract validated". No browser, provider, or network state is
// observed, and receipts never claim otherwise. The real observe-verify loop
// exists only in the native runtime (native/seoul/browser/product/).

import type { ActionReceipt, SemanticResult, TaskSnapshot } from './protocol.js';
import {
  formatErrors,
  validateCapabilityDescriptor,
  validateSemanticResult,
  validateTaskSnapshot,
} from './protocol.js';
import { loadFixtureCatalog, type FixtureCapability } from './fixtures.js';

export interface PlanResult {
  ok: boolean;
  capability?: FixtureCapability;
  score: number;
  failure?: string;
}

export interface TaskRun {
  ok: boolean;
  goal: string;
  snapshot: TaskSnapshot;
  result?: SemanticResult;
  capability?: FixtureCapability;
  failure?: string;
}

let catalog: FixtureCapability[] | undefined;

/**
 * The registered fixture capabilities. Each descriptor and canned result is
 * validated against the canonical schemas at first load; a fixture that
 * drifts from the contract fails loudly here rather than rendering.
 */
export function capabilities(): FixtureCapability[] {
  if (!catalog) {
    catalog = loadFixtureCatalog();
    for (const capability of catalog) {
      const descriptorCheck = validateCapabilityDescriptor(capability.descriptor);
      if (!descriptorCheck.valid) {
        throw new Error(
          `fixture descriptor ${capability.descriptor.id} violates the canonical contract:\n${formatErrors(descriptorCheck.errors)}`,
        );
      }
      const resultCheck = validateSemanticResult(capability.result);
      if (!resultCheck.valid) {
        throw new Error(
          `fixture result for ${capability.descriptor.id} violates the canonical contract:\n${formatErrors(resultCheck.errors)}`,
        );
      }
    }
  }
  return catalog;
}

function tokenize(text: string): string[] {
  return (text.toLowerCase().match(/[a-z0-9]+/g) ?? []).filter((t) => t.length >= 2);
}

/** Deterministic lexical token-overlap router over descriptor text. */
export function planGoal(goal: string): PlanResult {
  const goalTokens = new Set(tokenize(goal));
  if (goalTokens.size === 0) return { ok: false, score: 0, failure: 'Empty goal.' };

  let best: FixtureCapability | undefined;
  let bestScore = 0;
  for (const capability of capabilities()) {
    const d = capability.descriptor;
    const words = new Set([
      ...tokenize(d.id),
      ...tokenize(d.name),
      ...tokenize(d.description),
      ...(d.capability_tags ?? []).flatMap(tokenize),
    ]);
    let matches = 0;
    for (const t of goalTokens) if (words.has(t)) matches++;
    const score = matches / goalTokens.size;
    if (score > bestScore) {
      bestScore = score;
      best = capability;
    }
  }
  if (!best || bestScore <= 0) {
    return {
      ok: false,
      score: 0,
      failure:
        'No registered fixture capability matches the request. The catalog above lists every registered fixture and an example goal for each.',
    };
  }
  return { ok: true, capability: best, score: bestScore };
}

function generateTaskId(): string {
  if (typeof crypto !== 'undefined' && 'randomUUID' in crypto) return crypto.randomUUID();
  let out = '';
  for (const c of 'xxxxxxxx-xxxx-4xxx-8xxx-xxxxxxxxxxxx') {
    out += c === 'x' ? Math.floor(Math.random() * 16).toString(16) : c;
  }
  return out;
}

/**
 * Executes one goal against the fixture catalog and returns a canonical task
 * snapshot. The single receipt records fixture execution honestly: route
 * "deterministic", method "fixture_contract", and an observed summary that
 * describes the fixture payload, never a browser observation.
 */
export function runTask(goal: string, now: () => number = Date.now): TaskRun {
  const startedAt = now();
  const plan = planGoal(goal);
  const taskId = generateTaskId();

  if (!plan.ok || !plan.capability) {
    const snapshot: TaskSnapshot = {
      schema_version: 1,
      id: taskId,
      goal,
      state: 'failed',
      failure: 'step_failed',
      plan_origin: 'deterministic',
      receipts: [],
      usage: { steps_executed: 0, model_calls: 0, navigations: 0, cloud_cost_microdollars: 0, replans_used: 0 },
      has_semantic_result: false,
      replans_used: 0,
    };
    return { ok: false, goal, snapshot, failure: plan.failure ?? 'Planning failed.' };
  }

  const capability = plan.capability;
  const result = capability.result;
  const contract = validateSemanticResult(result);
  const failedResult = result.state === 'failed';
  const verified = contract.valid;

  const receipt: ActionReceipt = {
    step_id: 'step-run',
    tool: capability.descriptor.id,
    status: failedResult ? 'failed' : verified ? 'succeeded' : 'failed',
    started_at_ms: startedAt,
    finished_at_ms: now(),
    observed_summary: failedResult
      ? `Fixture reported a structured ${result.errors?.[0]?.code ?? 'error'} result.`
      : `Fixture returned the registered ${result.schema.shape} payload (synthetic demo data).`,
    verification: {
      verified: verified && !failedResult,
      method: 'fixture_contract',
      detail: verified
        ? failedResult
          ? 'fixture contract validated; the fixture reports a structured error result'
          : 'fixture contract validated'
        : `fixture violates the canonical contract:\n${formatErrors(contract.errors)}`,
    },
    route: 'deterministic',
    cost_microdollars: 0,
    model_calls: 0,
    navigations: 0,
  };

  const snapshot: TaskSnapshot = {
    schema_version: 1,
    id: taskId,
    goal,
    state: failedResult ? 'failed' : 'completed',
    failure: failedResult ? 'step_failed' : 'none',
    plan_origin: 'deterministic',
    receipts: [receipt],
    usage: { steps_executed: 1, model_calls: 0, navigations: 0, cloud_cost_microdollars: 0, replans_used: 0 },
    has_semantic_result: true,
    replans_used: 0,
  };
  const snapshotCheck = validateTaskSnapshot(snapshot);
  if (!snapshotCheck.valid) {
    throw new Error(`Design Lab runtime emitted a non-canonical snapshot:\n${formatErrors(snapshotCheck.errors)}`);
  }

  return { ok: verified, goal, snapshot, result, capability };
}

/**
 * Appends rows to a streaming/partial list-shaped result, mirroring the
 * native MergeStreamingRows: the merge is rejected (returning undefined)
 * unless the merged document still validates against the canonical schema.
 */
export function mergeStreamingRows(
  result: SemanticResult,
  rows: Record<string, unknown>[],
): SemanticResult | undefined {
  if (result.state !== 'streaming' && result.state !== 'partial') return undefined;
  if (!Array.isArray(result.data)) return undefined;
  const merged: SemanticResult = { ...result, data: [...(result.data as unknown[]), ...rows] };
  return validateSemanticResult(merged).valid ? merged : undefined;
}

/**
 * The next synthetic batch of rows for a streaming fixture: the fixture's
 * append rows with timestamps shifted forward per batch. The caller derives
 * BOTH the rows-table update and the per-measure series points from these
 * same rows, so the two representations of one surface can never disagree.
 */
export function nextStreamRows(
  capability: FixtureCapability,
  batchIndex: number,
): Record<string, unknown>[] {
  const appendRows = capability.streamAppendRows ?? [];
  if (appendRows.length === 0) return [];
  const temporal = (capability.result.schema.fields ?? []).find((f) => f.role === 'timestamp');
  if (!temporal) return [];
  const hour = 3600000;
  return appendRows.map((row) => {
    const shifted: Record<string, unknown> = { ...row };
    if (typeof row[temporal.id] === 'number') {
      shifted[temporal.id] = (row[temporal.id] as number) + batchIndex * appendRows.length * hour;
    }
    return shifted;
  });
}
