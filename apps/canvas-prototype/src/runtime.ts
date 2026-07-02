// Project Seoul Canvas prototype - capability registry, planner, task runner.
//
// The planner selects a capability by lexical overlap between the goal and each
// descriptor's own text (id, name, description, tags) - data-driven from the
// registry, with no keyword-to-capability table. This mirrors the native
// deterministic planner (native/seoul/browser/product/planner.cc). The bundled
// capabilities return arbitrary, previously-unseen schemas on purpose: the
// adaptive compiler is never told what any of them "mean".

import type { SemanticResult } from './semantic.js';

export interface CapabilityDescriptor {
  id: string;
  name: string;
  description: string;
  tags: string[];
  run: () => SemanticResult;
}

// --- Generic bundled capabilities (arbitrary schemas, no domain routing) -----

function ts(daysAgo: number, hour: number): number {
  // Deterministic timestamps relative to a fixed anchor so the prototype is
  // reproducible (no wall-clock dependence).
  const anchor = Date.UTC(2026, 0, 15, 0, 0, 0);
  return anchor - daysAgo * 86400000 + hour * 3600000;
}

const CAPABILITIES: CapabilityDescriptor[] = [
  {
    id: 'info.pipeline.latency',
    name: 'pipeline latency timeline',
    description:
      'reads the processing pipeline queue latency samples over recent hours',
    tags: ['latency', 'queue', 'timeline', 'performance', 'pipeline'],
    run: () => {
      const rows = Array.from({ length: 24 }, (_, i) => ({
        sampled_at: ts(0, i),
        latency_ms: Math.round(120 + 70 * Math.sin(i / 3) + (i % 5) * 9),
        throughput: Math.round(800 + 120 * Math.cos(i / 4)),
      }));
      return {
        schema: {
          shape: 'time_series',
          fields: [
            { id: 'sampled_at', label: 'Sampled at', primitive: 'timestamp', role: 'timestamp' },
            { id: 'latency_ms', label: 'Latency', primitive: 'number', role: 'measure', unit: 'ms' },
            { id: 'throughput', label: 'Throughput', primitive: 'number', role: 'measure', unit: 'req/s' },
          ],
        },
        data: rows,
        provenance: { source: 'info.pipeline.latency', retrievedAt: ts(0, 23), freshness: 'near_real_time', completeness: 1 },
        state: 'complete',
      };
    },
  },
  {
    id: 'info.survey.readings',
    name: 'substrate survey readings',
    description: 'collects the substrate survey readings per station from the field log',
    tags: ['survey', 'station', 'readings', 'substrate', 'field'],
    run: () => {
      const cultivars = ['loam', 'silt', 'clay', 'peat', 'sand', 'chalk'];
      const rows = Array.from({ length: 8 }, (_, i) => ({
        station: `S-${(i + 1).toString().padStart(2, '0')}`,
        substrate: cultivars[i % cultivars.length],
        reading: Math.round((42 + 18 * Math.sin(i)) * 10) / 10,
        samples: 30 + (i % 4) * 6,
      }));
      return {
        schema: {
          shape: 'entity_collection',
          fields: [
            { id: 'station', label: 'Station', primitive: 'string', role: 'identifier' },
            { id: 'substrate', label: 'Substrate', primitive: 'string', role: 'category' },
            { id: 'reading', label: 'Reading', primitive: 'number', role: 'measure', unit: 'ppm' },
            { id: 'samples', label: 'Samples', primitive: 'integer', role: 'count' },
          ],
        },
        data: rows,
        provenance: { source: 'info.survey.readings', retrievedAt: ts(0, 12), freshness: 'cached', completeness: 1 },
        state: 'complete',
      };
    },
  },
  {
    id: 'info.node.profile',
    name: 'compute node profile',
    description: 'describes one compute node profile including region and status',
    tags: ['node', 'profile', 'region', 'status', 'record'],
    run: () => ({
      schema: {
        shape: 'record',
        fields: [
          { id: 'node_id', label: 'Node', primitive: 'string', role: 'identifier' },
          { id: 'region', label: 'Region', primitive: 'string', role: 'category' },
          { id: 'status', label: 'Status', primitive: 'string', role: 'status' },
          { id: 'cores', label: 'Cores', primitive: 'integer', role: 'count' },
          { id: 'memory_gib', label: 'Memory', primitive: 'number', role: 'measure', unit: 'GiB' },
          { id: 'uptime_days', label: 'Uptime', primitive: 'number', role: 'measure', unit: 'days' },
        ],
      },
      data: {
        node_id: 'node-7f3a',
        region: 'ap-northeast',
        status: 'healthy',
        cores: 32,
        memory_gib: 128,
        uptime_days: 84.5,
      },
      provenance: { source: 'info.node.profile', retrievedAt: ts(0, 9), freshness: 'real_time', completeness: 1 },
      state: 'complete',
    }),
  },
  {
    id: 'info.workspace.tree',
    name: 'workspace hierarchy',
    description: 'lists the workspace project hierarchy as a parent-child tree',
    tags: ['workspace', 'hierarchy', 'tree', 'project', 'structure'],
    run: () => ({
      schema: {
        shape: 'hierarchy',
        fields: [
          { id: 'id', label: 'Node', primitive: 'string', role: 'identifier' },
          { id: 'name', label: 'Name', primitive: 'string', role: 'name' },
          { id: 'parent', label: 'Parent', primitive: 'string', role: 'parent_reference' },
        ],
      },
      data: [
        { id: 'root', name: 'Atlas', parent: '' },
        { id: 'ingest', name: 'Ingest', parent: 'root' },
        { id: 'ingest-a', name: 'Collector', parent: 'ingest' },
        { id: 'ingest-b', name: 'Normalizer', parent: 'ingest' },
        { id: 'serve', name: 'Serving', parent: 'root' },
        { id: 'serve-a', name: 'Gateway', parent: 'serve' },
        { id: 'serve-b', name: 'Cache', parent: 'serve' },
        { id: 'serve-b1', name: 'Warmers', parent: 'serve-b' },
      ],
      provenance: { source: 'info.workspace.tree', retrievedAt: ts(1, 3), freshness: 'cached', completeness: 1 },
      state: 'complete',
    }),
  },
  {
    id: 'info.sources.citations',
    name: 'reference citations',
    description: 'returns the reference citations gathered for the current thread',
    tags: ['sources', 'citations', 'references', 'links'],
    run: () => ({
      schema: {
        shape: 'citations',
        fields: [
          { id: 'title', label: 'Title', primitive: 'string', role: 'name' },
          { id: 'url', label: 'URL', primitive: 'string', role: 'url' },
          { id: 'retrieved', label: 'Retrieved', primitive: 'timestamp', role: 'timestamp' },
        ],
      },
      data: [
        { title: 'Adaptive interfaces from typed data', url: 'https://example.test/adaptive', retrieved: ts(2, 10) },
        { title: 'Semantic provenance for generated UI', url: 'https://example.test/provenance', retrieved: ts(3, 14) },
        { title: 'Shape-driven visualization selection', url: 'https://example.test/shape-viz', retrieved: ts(4, 9) },
      ],
      provenance: { source: 'info.sources.citations', retrievedAt: ts(0, 8), freshness: 'cached', completeness: 1 },
      state: 'complete',
    }),
  },
  {
    id: 'info.reliability.metric',
    name: 'reliability metric',
    description: 'reports the current overall reliability metric as a single value',
    tags: ['reliability', 'metric', 'score', 'now'],
    run: () => ({
      schema: {
        shape: 'scalar',
        fields: [{ id: 'value', label: 'Reliability', primitive: 'number', role: 'percentage', unit: '%' }],
      },
      data: 99.94,
      provenance: { source: 'info.reliability.metric', retrievedAt: ts(0, 1), freshness: 'real_time', completeness: 1 },
      state: 'complete',
    }),
  },
];

export function capabilities(): CapabilityDescriptor[] {
  return CAPABILITIES.slice();
}

// --- Deterministic planner (lexical, data-driven) ---------------------------

function tokenize(text: string): string[] {
  return (text.toLowerCase().match(/[a-z0-9]+/g) || []).filter((t) => t.length >= 2);
}

export interface PlanResult {
  ok: boolean;
  capability?: CapabilityDescriptor;
  score: number;
  failure?: string;
}

export function planGoal(goal: string): PlanResult {
  const goalTokens = new Set(tokenize(goal));
  if (goalTokens.size === 0) return { ok: false, score: 0, failure: 'Empty goal.' };

  let best: CapabilityDescriptor | undefined;
  let bestScore = 0;
  for (const cap of CAPABILITIES) {
    const words = new Set([
      ...tokenize(cap.id),
      ...tokenize(cap.name),
      ...tokenize(cap.description),
      ...cap.tags.flatMap(tokenize),
    ]);
    let matches = 0;
    for (const t of goalTokens) if (words.has(t)) matches++;
    const score = matches / goalTokens.size;
    if (score > bestScore) {
      bestScore = score;
      best = cap;
    }
  }
  if (!best || bestScore <= 0) {
    return {
      ok: false,
      score: 0,
      failure:
        'No available capability matches the request. Try words like latency, survey, node, workspace tree, citations, or reliability.',
    };
  }
  return { ok: true, capability: best, score: bestScore };
}

export interface TaskReceipt {
  capabilityId: string;
  verified: boolean;
  observedSummary: string;
  route: 'deterministic';
}

export interface TaskRun {
  ok: boolean;
  goal: string;
  plan: PlanResult;
  result?: SemanticResult;
  receipt?: TaskReceipt;
  failure?: string;
}

// Runs the planned capability, "observes" and "verifies" its output (here the
// verification is that the capability returned a well-formed complete result),
// and returns the semantic result plus a receipt - the same observe-verify
// contract the native task engine enforces.
export function runTask(goal: string): TaskRun {
  const plan = planGoal(goal);
  if (!plan.ok || !plan.capability) {
    return { ok: false, goal, plan, failure: plan.failure };
  }
  const result = plan.capability.run();
  const verified = !!result && result.state === 'complete';
  return {
    ok: verified,
    goal,
    plan,
    result,
    receipt: {
      capabilityId: plan.capability.id,
      verified,
      observedSummary: `${plan.capability.name} returned a ${result.schema.shape} result`,
      route: 'deterministic',
    },
    failure: verified ? undefined : 'The capability did not return a complete result.',
  };
}
