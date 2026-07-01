# Seoul Workflow Specification

Status: Source complete; not compiled or runtime-verified on the authoring host.

A Seoul workflow is a versioned typed task graph over registered tools, not a
recorded click macro. This spec describes the source in
`native/seoul/browser/workflows/`. Nodes carry semantic tool arguments, never
raw coordinates or tab indices; a saved workflow compiles to the same bounded
`Plan` an ad-hoc goal uses, so its semantic targets are re-resolved each run.

## Versioned typed task graph

`native/seoul/browser/workflows/workflow_types.h` defines `WorkflowDefinition`:
a `WorkflowId`, name, description, declared `WorkflowParam` list, `WorkflowNode`
list (canvas order), `WorkflowEdge` list, a `WorkflowTrigger`, optional
`scene_scope`/`site_scope`, an integer `version`, timestamps, and an optional
`last_run` summary.

- `WorkflowNodeKind`: `kToolStep` (a `ToolId` plus an args dict), `kApproval`
  (an explicit user approval point with a prompt), and `kUserInput` (collect
  parameter values mid-run). A node may also set `requires_approval` as an extra
  gate on a tool step, and `max_iterations > 0` marks it a bounded loop header.
- `WorkflowEdgeKind`: `kSequence` (run regardless of outcome), `kOnSuccess`,
  `kOnFailure`, and `kLoopBack` (return to a bounded loop header). The header
  comment is explicit that a condition is not an expression language: a
  predicate is itself a tool step whose success or failure drives the outcome
  edges.
- `WorkflowTriggerKind`: `kManual`, `kSchedule`, `kSceneActivation`,
  `kNavigation`, `kPageStateChange`, `kServiceEvent`, and `kStartup`.
- `WorkflowParam`: a `SchemaField` plus a default value. Node args reference a
  param with the placeholder string `{{param:<name>}}`.

Bounds live in the same header: `kMaxWorkflowNodes` (100), `kMaxWorkflowEdges`
(200), `kMaxWorkflowParams` (32), `kMaxWorkflowLoopIterations` (25), and a
schedule interval clamped to 1 to 10080 minutes (one week).

## Structural validation

`ValidateWorkflowStructure` in `workflow_graph.cc` checks name and description
lengths, non-empty node set and the node/edge/param caps, and per-param schema
well-formedness. Node ids must match `[a-z][a-z0-9_-]{0,63}` and be unique;
approval and user-input nodes must carry a prompt; `max_iterations` must be in
range. Edges must reference known nodes, may not be self-edges, and may not
duplicate a `(from, to)` pair. A `kLoopBack` edge must target a header with a
nonzero bound (`kLoopUnbounded` otherwise), and every loop header must be the
target of some loop-back edge (`kLoopHeaderUnreachable` otherwise, so a bound is
never dead configuration). Triggers are validated per kind. Tool resolvability is
deliberately deferred to compile time, because tools come and go with connectors.

## Deterministic ordering and cycle detection

`TopologicalOrder` runs Kahn's algorithm over non-loop-back edges with
lexicographic tie-breaking, giving a deterministic execution order. If the
result does not cover every node, the graph has a cycle that is not an explicit
bounded loop, and it fails with `kCycleWithoutLoop`.

## Compilation onto a Plan

`CompileWorkflow` lowers a validated workflow onto the typed `Plan` in
`native/seoul/browser/tasks/plan_types.h`. Nodes become `PlanStep`s in
topological order: a tool step becomes `kToolCall` with substituted args and its
`requires_approval` flag, an approval node becomes `kApprovalGate`, and a
user-input node becomes `kUserInput`, each carrying the node prompt. Outcome
edges become `StepGuard`s: an incoming `kOnSuccess`/`kOnFailure` edge sets
`guard.depends_on_step` and `require_success` (the first incoming conditional
edge in edge order wins; richer joins are a schema bump). A loop-back edge from S
to header H forms a loop group covering every node in the topological span
`[H, S]`, stamping each with `loop_group` and the header's `max_iterations`.
Param placeholders are replaced by `SubstituteParams` with the typed run value,
falling back to a declared default; a reference with no value fails with
`kUnknownParamReference`. The header comment and code both note the resulting
plan still goes through `ValidatePlan` before it runs.

## Typed edit operations

`native/seoul/browser/workflows/workflow_editor.cc` exposes the mutations the
Workflow Canvas uses: add/remove node, set node args, set node approval, set
trigger, add/remove edge, and add/remove bounded loop. Every one runs through
`CommitIfValid`, which mutates a copy, re-validates the whole graph, and commits
only on success while bumping `version` and stamping `updated_at`; an invalid
edit leaves the workflow untouched. Add-node splices sequence edges to keep a
connected order; remove-node reconnects sequence predecessors to successors.
`AddWorkflowLoop`/`RemoveWorkflowLoop` change the header bound and the loop-back
edge together, since a bound without an edge (or the reverse) never validates.
The editor header notes that voice edits ("remove the email step") resolve to
node ids at the Canvas layer and land here as these same typed operations.

## Import and export round trip

`ExportWorkflow` serializes the definition (schema version, id, params, nodes,
edges, trigger, scopes, version, timestamps) to a `base::Value::Dict`.
`ImportWorkflow` parses it back, rejects an unsupported schema version, generates
a fresh id when none is valid, and runs `ValidateWorkflowStructure` before
returning, so an imported workflow is always structurally sound.
`DuplicateWorkflow` yields a fresh id, a "<name> (copy)" name, and version 1.
