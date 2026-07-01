# Seoul End-to-End Test Plan

Status: Source complete; not compiled or runtime-verified on the authoring host.

This is the end-to-end fixture plan for Project Seoul: twelve scenarios that
exercise the subsystems together. These are evaluation fixtures, not hardcoded
task routes; a fixture states a goal and the environment, and the operator must
reach the outcome through the same registry, planner, validator, and executor an
ad-hoc goal uses. They become runnable browser tests only on a capable build host
(an 8 GiB host cannot build Chromium). Today they are specified, and the
underlying units are tested; the scenarios themselves have no run results yet.

Subsystems referenced live under `native/seoul/browser/`: `voice/`, `saui/`,
`tasks/`, `tools/`, `workflows/`, `scenes/`, `site_layers/`, `context/`,
`connectors/`, and `intelligence/`.

## The twelve scenarios

1. Weather, refined by voice. Exercises voice (`voice_session`), the reasoning
   router (deterministic formatting), SAUI selection, and a weather-domain
   surface. Pass: the spoken refinement updates the same surface in place via a
   typed patch, and the chart carries provenance.

2. Compare three instruments with interval changes. Exercises grounded market
   data with provenance, chart honesty, and typed patches on interval change.
   Pass: a composed surface with a chart-eligible series per instrument; changing
   the interval re-patches rather than rebuilds; truncated axes are indicated.

3. Compare product prices and filter. Exercises product-domain records, a
   comparison surface, and deterministic filtering (never a model). Pass: a
   comparison matrix or price-comparison surface; filtering runs deterministically
   and updates the surface.

4. Research to a cited report. Exercises multi-step planning, source citations,
   and a report surface. Pass: a `report` surface whose claims carry citations
   with http(s) sources; no unattributed data backs a visual.

5. Fill a form and stop before submit. Exercises the task executor's approval
   semantics. Pass: the plan prepares inputs, the submit step is approval-gated,
   rejecting the gate skips submission while keeping the prepared work
   (prepare-but-do-not-submit).

6. Organize tabs into a Scene. Exercises organization plus the Scene registry.
   Pass: a Scene referencing the workspace/theme/layers by id validates against
   resolvers and produces a deterministic ordered activation plan.

7. Create a Site Layer by voice. Exercises voice plus the site-layer compiler.
   Pass: the spoken adjustment compiles to safe scoped CSS on a valid origin
   pattern; an unsafe selector or value is rejected.

8. Build and edit a workflow. Exercises the workflow graph and editor. Pass:
   nodes and edges validate (bounded loops, no bare cycles), a voice edit
   resolves to a typed edit operation that bumps the version, and an invalid edit
   leaves the workflow untouched.

9. Run a workflow and interrupt by voice. Exercises workflow compilation to a
   Plan, execution, and voice barge-in. Pass: the workflow compiles to a
   validated Plan, runs through the task executor, and a barge-in stops speech
   immediately while the task state is preserved.

10. Connected calendar (fake connector). Exercises the connector registry and
    tool ownership. Pass: connecting registers `connector.calendar.*` tools into
    the shared registry atomically; the planner uses them under permission;
    disconnecting removes exactly those tools.

11. Monitor a changing local page. Exercises a monitoring task and live surface
    updates. Pass: the task stays visible in the Task Deck `Monitoring` filter and
    the surface refreshes via typed patches on each observed change.

12. Restart and continue a saved task. Exercises execution checkpointing. Pass:
    `Checkpoint` and `RestoreFromCheckpoint` continue the task with the same plan,
    and any step that was mid-run at restart becomes an unknown outcome that does
    not silently re-run.

## Fixtures, not routes

Each scenario is defined by a goal and a starting environment, plus the expected
observable outcome and the invariants above. The operator is not given the step
sequence; it must plan against the live tool registry and re-resolve semantic
targets at run time, so a fixture tests generalization rather than a memorized
path. No scenario embeds a hardcoded task route in source.

## Status of the underlying units

The pieces these scenarios compose are already covered by authored unit tests
(see the operator evaluation and adaptive-UI test plans): plan validation, task
execution decisions, tool-registry ownership, SAUI parse/validate/patch/selection,
workflow structure and compilation, scene validation, site-layer compilation,
theme contrast, context minimization, and reasoning routing. The end-to-end
scenarios that drive a real browser window remain open until a capable build host
is available.
