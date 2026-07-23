// Project Seoul workflows.

#include "seoul/browser/workflows/workflow_types.h"

namespace seoul {

WorkflowTrigger::WorkflowTrigger() = default;
WorkflowTrigger::WorkflowTrigger(const WorkflowTrigger&) = default;
WorkflowTrigger::WorkflowTrigger(WorkflowTrigger&&) = default;
WorkflowTrigger& WorkflowTrigger::operator=(const WorkflowTrigger&) = default;
WorkflowTrigger& WorkflowTrigger::operator=(WorkflowTrigger&&) = default;
WorkflowTrigger::~WorkflowTrigger() = default;

WorkflowDefinition::WorkflowDefinition() = default;
WorkflowDefinition::WorkflowDefinition(const WorkflowDefinition&) = default;
WorkflowDefinition::WorkflowDefinition(WorkflowDefinition&&) = default;
WorkflowDefinition& WorkflowDefinition::operator=(const WorkflowDefinition&) = default;
WorkflowDefinition& WorkflowDefinition::operator=(WorkflowDefinition&&) = default;
WorkflowDefinition::~WorkflowDefinition() = default;

}  // namespace seoul
