// Project Seoul workflow system.
// Structural validation and deterministic ordering for workflow graphs, and
// compilation of a workflow into a typed Plan so saved workflows execute
// through exactly the same bounded task system as ad-hoc goals.

#ifndef SEOUL_BROWSER_WORKFLOWS_WORKFLOW_GRAPH_H_
#define SEOUL_BROWSER_WORKFLOWS_WORKFLOW_GRAPH_H_

#include <map>
#include <string>
#include <vector>

#include "seoul/browser/tasks/plan_types.h"
#include "seoul/browser/workflows/workflow_types.h"

namespace seoul {

// Structural validation: identities, prompts, edges, bounded loops, trigger
// shape, parameter references. Tool resolvability is validated at compile
// time against the live registry (tools may come and go with connectors).
WorkflowStatusResult ValidateWorkflowStructure(
    const WorkflowDefinition& definition);

// Deterministic execution order: Kahn's topological sort over non-loop-back
// edges with lexicographic tie-breaking. Fails on cycles that are not
// explicit bounded loops.
WorkflowResult<std::vector<std::string>> TopologicalOrder(
    const WorkflowDefinition& definition);

// Lowers the workflow onto a Plan: nodes become plan steps in topological
// order, kOnSuccess/kOnFailure edges become step guards, loop-back spans
// become bounded loop groups, and "{{param:<name>}}" placeholders are
// replaced with typed values from `param_values` (falling back to declared
// defaults). The resulting plan still goes through ValidatePlan before it
// runs.
WorkflowResult<Plan> CompileWorkflow(const WorkflowDefinition& definition,
                                     const base::Value::Dict& param_values,
                                     const TaskBudgets& budgets);

}  // namespace seoul

#endif  // SEOUL_BROWSER_WORKFLOWS_WORKFLOW_GRAPH_H_
