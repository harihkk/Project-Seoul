// Project Seoul workflow system.
// Typed edit operations for the Workflow Canvas. Every mutation validates the
// resulting graph first (an invalid edit leaves the workflow untouched) and
// bumps the version. Voice edits ("remove the email step") resolve to node
// ids at the Canvas layer and land here as the same typed operations.

#ifndef SEOUL_BROWSER_WORKFLOWS_WORKFLOW_EDITOR_H_
#define SEOUL_BROWSER_WORKFLOWS_WORKFLOW_EDITOR_H_

#include <string>

#include "base/functional/callback.h"
#include "base/values.h"
#include "seoul/browser/workflows/workflow_types.h"

namespace seoul {

using WorkflowClock = base::RepeatingCallback<base::Time()>;

// Appends `node` (after `after_node_id` when non-empty, else at the end),
// wiring sequence edges to keep a connected order.
WorkflowStatusResult AddWorkflowNode(WorkflowDefinition& definition,
                                     WorkflowNode node,
                                     const std::string& after_node_id,
                                     const WorkflowClock& clock);

// Removes a node and its edges; sequence predecessors are reconnected to
// sequence successors.
WorkflowStatusResult RemoveWorkflowNode(WorkflowDefinition& definition,
                                        const std::string& node_id,
                                        const WorkflowClock& clock);

WorkflowStatusResult SetWorkflowNodeArgs(WorkflowDefinition& definition,
                                         const std::string& node_id,
                                         base::Value::Dict args,
                                         const WorkflowClock& clock);

WorkflowStatusResult SetWorkflowNodeApproval(WorkflowDefinition& definition,
                                             const std::string& node_id,
                                             bool requires_approval,
                                             const WorkflowClock& clock);

WorkflowStatusResult SetWorkflowTrigger(WorkflowDefinition& definition,
                                        const WorkflowTrigger& trigger,
                                        const WorkflowClock& clock);

WorkflowStatusResult AddWorkflowEdge(WorkflowDefinition& definition,
                                     const WorkflowEdge& edge,
                                     const WorkflowClock& clock);

WorkflowStatusResult RemoveWorkflowEdge(WorkflowDefinition& definition,
                                        const std::string& from,
                                        const std::string& to,
                                        const WorkflowClock& clock);

// Adds a bounded loop atomically: sets `max_iterations` on the header and
// adds the loop-back edge from `from_node` to it (a bound without an edge or
// an edge without a bound never validates, so both change together).
WorkflowStatusResult AddWorkflowLoop(WorkflowDefinition& definition,
                                     const std::string& from_node,
                                     const std::string& header_node,
                                     int max_iterations,
                                     const WorkflowClock& clock);

// Removes the loop-back edge and clears the header bound when no other
// loop-back edge still targets it.
WorkflowStatusResult RemoveWorkflowLoop(WorkflowDefinition& definition,
                                        const std::string& from_node,
                                        const std::string& header_node,
                                        const WorkflowClock& clock);

// A fresh id, "<name> (copy)", version 1.
WorkflowDefinition DuplicateWorkflow(const WorkflowDefinition& definition,
                                     const WorkflowClock& clock);

base::Value::Dict ExportWorkflow(const WorkflowDefinition& definition);
WorkflowResult<WorkflowDefinition> ImportWorkflow(const base::Value& value);

}  // namespace seoul

#endif  // SEOUL_BROWSER_WORKFLOWS_WORKFLOW_EDITOR_H_
