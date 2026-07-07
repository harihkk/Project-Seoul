// Project Seoul workflow system.

#include "seoul/browser/workflows/workflow_editor.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "seoul/browser/workflows/workflow_graph.h"

namespace seoul {

namespace {

// Applies `mutate` to a copy; commits (version bump + timestamp) only when
// the mutated definition still validates.
WorkflowStatusResult CommitIfValid(
    WorkflowDefinition& definition,
    const WorkflowClock& clock,
    base::OnceCallback<WorkflowStatusResult(WorkflowDefinition&)> mutate) {
  WorkflowDefinition working = definition;
  if (auto result = std::move(mutate).Run(working); !result.has_value()) {
    return result;
  }
  if (auto valid = ValidateWorkflowStructure(working); !valid.has_value()) {
    return valid;
  }
  working.version = definition.version + 1;
  working.updated_at = clock.Run();
  definition = std::move(working);
  return base::ok();
}

WorkflowNode* FindMutableNode(WorkflowDefinition& definition,
                              const std::string& id) {
  for (WorkflowNode& node : definition.nodes) {
    if (node.id == id) {
      return &node;
    }
  }
  return nullptr;
}

const char* SchemaFieldKindName(SchemaFieldKind kind) {
  switch (kind) {
    case SchemaFieldKind::kString:
      return "string";
    case SchemaFieldKind::kInteger:
      return "integer";
    case SchemaFieldKind::kNumber:
      return "number";
    case SchemaFieldKind::kBoolean:
      return "boolean";
    case SchemaFieldKind::kEnum:
      return "enum";
    case SchemaFieldKind::kUrl:
      return "url";
    case SchemaFieldKind::kList:
      return "list";
    case SchemaFieldKind::kObject:
      return "object";
  }
  return "string";
}

bool SchemaFieldKindFromName(const std::string& name, SchemaFieldKind* out) {
  static constexpr std::pair<const char*, SchemaFieldKind> kKinds[] = {
      {"string", SchemaFieldKind::kString},
      {"integer", SchemaFieldKind::kInteger},
      {"number", SchemaFieldKind::kNumber},
      {"boolean", SchemaFieldKind::kBoolean},
      {"enum", SchemaFieldKind::kEnum},
      {"url", SchemaFieldKind::kUrl},
      {"list", SchemaFieldKind::kList},
      {"object", SchemaFieldKind::kObject},
  };
  for (const auto& [kind_name, kind] : kKinds) {
    if (name == kind_name) {
      *out = kind;
      return true;
    }
  }
  return false;
}

const char* NodeKindName(WorkflowNodeKind kind) {
  switch (kind) {
    case WorkflowNodeKind::kToolStep:
      return "tool_step";
    case WorkflowNodeKind::kApproval:
      return "approval";
    case WorkflowNodeKind::kUserInput:
      return "user_input";
  }
  return "tool_step";
}

bool NodeKindFromName(const std::string& name, WorkflowNodeKind* out) {
  if (name == "tool_step") {
    *out = WorkflowNodeKind::kToolStep;
  } else if (name == "approval") {
    *out = WorkflowNodeKind::kApproval;
  } else if (name == "user_input") {
    *out = WorkflowNodeKind::kUserInput;
  } else {
    return false;
  }
  return true;
}

const char* EdgeKindName(WorkflowEdgeKind kind) {
  switch (kind) {
    case WorkflowEdgeKind::kSequence:
      return "sequence";
    case WorkflowEdgeKind::kOnSuccess:
      return "on_success";
    case WorkflowEdgeKind::kOnFailure:
      return "on_failure";
    case WorkflowEdgeKind::kLoopBack:
      return "loop_back";
  }
  return "sequence";
}

bool EdgeKindFromName(const std::string& name, WorkflowEdgeKind* out) {
  if (name == "sequence") {
    *out = WorkflowEdgeKind::kSequence;
  } else if (name == "on_success") {
    *out = WorkflowEdgeKind::kOnSuccess;
  } else if (name == "on_failure") {
    *out = WorkflowEdgeKind::kOnFailure;
  } else if (name == "loop_back") {
    *out = WorkflowEdgeKind::kLoopBack;
  } else {
    return false;
  }
  return true;
}

const char* TriggerKindName(WorkflowTriggerKind kind) {
  switch (kind) {
    case WorkflowTriggerKind::kManual:
      return "manual";
    case WorkflowTriggerKind::kSchedule:
      return "schedule";
    case WorkflowTriggerKind::kSceneActivation:
      return "scene_activation";
    case WorkflowTriggerKind::kNavigation:
      return "navigation";
    case WorkflowTriggerKind::kPageStateChange:
      return "page_state_change";
    case WorkflowTriggerKind::kServiceEvent:
      return "service_event";
    case WorkflowTriggerKind::kStartup:
      return "startup";
  }
  return "manual";
}

bool TriggerKindFromName(const std::string& name, WorkflowTriggerKind* out) {
  static constexpr std::pair<const char*, WorkflowTriggerKind> kKinds[] = {
      {"manual", WorkflowTriggerKind::kManual},
      {"schedule", WorkflowTriggerKind::kSchedule},
      {"scene_activation", WorkflowTriggerKind::kSceneActivation},
      {"navigation", WorkflowTriggerKind::kNavigation},
      {"page_state_change", WorkflowTriggerKind::kPageStateChange},
      {"service_event", WorkflowTriggerKind::kServiceEvent},
      {"startup", WorkflowTriggerKind::kStartup},
  };
  for (const auto& [kind_name, kind] : kKinds) {
    if (name == kind_name) {
      *out = kind;
      return true;
    }
  }
  return false;
}

double MillisFromTime(base::Time t) {
  return (t - base::Time::UnixEpoch()).InMillisecondsF();
}

base::Time TimeFromMillis(double ms) {
  return base::Time::UnixEpoch() + base::Milliseconds(ms);
}

}  // namespace

WorkflowStatusResult AddWorkflowNode(WorkflowDefinition& definition,
                                     WorkflowNode node,
                                     const std::string& after_node_id,
                                     const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](WorkflowNode node, const std::string& after_node_id,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            if (!after_node_id.empty()) {
              const WorkflowNode* anchor = nullptr;
              for (const WorkflowNode& existing : working.nodes) {
                if (existing.id == after_node_id) {
                  anchor = &existing;
                  break;
                }
              }
              if (!anchor) {
                return base::unexpected(WorkflowError::kUnknownNode);
              }
              // Splice into the anchor's outgoing sequence edges.
              for (WorkflowEdge& edge : working.edges) {
                if (edge.from == after_node_id &&
                    edge.kind == WorkflowEdgeKind::kSequence) {
                  WorkflowEdge continuation;
                  continuation.from = node.id;
                  continuation.to = edge.to;
                  continuation.kind = WorkflowEdgeKind::kSequence;
                  edge.to = node.id;
                  working.edges.push_back(continuation);
                  working.nodes.push_back(std::move(node));
                  return base::ok();
                }
              }
              WorkflowEdge edge;
              edge.from = after_node_id;
              edge.to = node.id;
              edge.kind = WorkflowEdgeKind::kSequence;
              working.edges.push_back(edge);
              working.nodes.push_back(std::move(node));
              return base::ok();
            }
            if (!working.nodes.empty()) {
              WorkflowEdge edge;
              edge.from = working.nodes.back().id;
              edge.to = node.id;
              edge.kind = WorkflowEdgeKind::kSequence;
              working.edges.push_back(edge);
            }
            working.nodes.push_back(std::move(node));
            return base::ok();
          },
          std::move(node), after_node_id));
}

WorkflowStatusResult RemoveWorkflowNode(WorkflowDefinition& definition,
                                        const std::string& node_id,
                                        const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const std::string& node_id,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            auto node_it = std::find_if(
                working.nodes.begin(), working.nodes.end(),
                [&node_id](const WorkflowNode& n) { return n.id == node_id; });
            if (node_it == working.nodes.end()) {
              return base::unexpected(WorkflowError::kUnknownNode);
            }
            working.nodes.erase(node_it);
            std::vector<std::string> predecessors;
            std::vector<std::string> successors;
            for (const WorkflowEdge& edge : working.edges) {
              if (edge.to == node_id &&
                  edge.kind == WorkflowEdgeKind::kSequence) {
                predecessors.push_back(edge.from);
              }
              if (edge.from == node_id &&
                  edge.kind == WorkflowEdgeKind::kSequence) {
                successors.push_back(edge.to);
              }
            }
            std::erase_if(working.edges, [&node_id](const WorkflowEdge& edge) {
              return edge.from == node_id || edge.to == node_id;
            });
            for (const std::string& from : predecessors) {
              for (const std::string& to : successors) {
                if (from == to) {
                  continue;
                }
                const bool exists =
                    std::any_of(working.edges.begin(), working.edges.end(),
                                [&from, &to](const WorkflowEdge& edge) {
                                  return edge.from == from && edge.to == to;
                                });
                if (!exists) {
                  WorkflowEdge edge;
                  edge.from = from;
                  edge.to = to;
                  edge.kind = WorkflowEdgeKind::kSequence;
                  working.edges.push_back(edge);
                }
              }
            }
            return base::ok();
          },
          node_id));
}

WorkflowStatusResult SetWorkflowNodeArgs(WorkflowDefinition& definition,
                                         const std::string& node_id,
                                         base::DictValue args,
                                         const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const std::string& node_id, base::DictValue args,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            WorkflowNode* node = FindMutableNode(working, node_id);
            if (!node) {
              return base::unexpected(WorkflowError::kUnknownNode);
            }
            node->args = std::move(args);
            return base::ok();
          },
          node_id, std::move(args)));
}

WorkflowStatusResult SetWorkflowNodeApproval(WorkflowDefinition& definition,
                                             const std::string& node_id,
                                             bool requires_approval,
                                             const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const std::string& node_id, bool requires_approval,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            WorkflowNode* node = FindMutableNode(working, node_id);
            if (!node) {
              return base::unexpected(WorkflowError::kUnknownNode);
            }
            node->requires_approval = requires_approval;
            return base::ok();
          },
          node_id, requires_approval));
}

WorkflowStatusResult SetWorkflowTrigger(WorkflowDefinition& definition,
                                        const WorkflowTrigger& trigger,
                                        const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const WorkflowTrigger& trigger,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            working.trigger = trigger;
            return base::ok();
          },
          trigger));
}

WorkflowStatusResult AddWorkflowEdge(WorkflowDefinition& definition,
                                     const WorkflowEdge& edge,
                                     const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const WorkflowEdge& edge,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            working.edges.push_back(edge);
            return base::ok();
          },
          edge));
}

WorkflowStatusResult RemoveWorkflowEdge(WorkflowDefinition& definition,
                                        const std::string& from,
                                        const std::string& to,
                                        const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const std::string& from, const std::string& to,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            const size_t removed = std::erase_if(
                working.edges, [&from, &to](const WorkflowEdge& edge) {
                  return edge.from == from && edge.to == to;
                });
            if (removed == 0) {
              return base::unexpected(WorkflowError::kEdgeUnknownNode);
            }
            return base::ok();
          },
          from, to));
}

WorkflowStatusResult AddWorkflowLoop(WorkflowDefinition& definition,
                                     const std::string& from_node,
                                     const std::string& header_node,
                                     int max_iterations,
                                     const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const std::string& from_node, const std::string& header_node,
             int max_iterations,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            WorkflowNode* header = FindMutableNode(working, header_node);
            if (!header || !FindMutableNode(working, from_node)) {
              return base::unexpected(WorkflowError::kUnknownNode);
            }
            header->max_iterations = max_iterations;
            WorkflowEdge edge;
            edge.from = from_node;
            edge.to = header_node;
            edge.kind = WorkflowEdgeKind::kLoopBack;
            working.edges.push_back(edge);
            return base::ok();
          },
          from_node, header_node, max_iterations));
}

WorkflowStatusResult RemoveWorkflowLoop(WorkflowDefinition& definition,
                                        const std::string& from_node,
                                        const std::string& header_node,
                                        const WorkflowClock& clock) {
  return CommitIfValid(
      definition, clock,
      base::BindOnce(
          [](const std::string& from_node, const std::string& header_node,
             WorkflowDefinition& working) -> WorkflowStatusResult {
            const size_t removed = std::erase_if(
                working.edges,
                [&from_node, &header_node](const WorkflowEdge& edge) {
                  return edge.kind == WorkflowEdgeKind::kLoopBack &&
                         edge.from == from_node && edge.to == header_node;
                });
            if (removed == 0) {
              return base::unexpected(WorkflowError::kEdgeUnknownNode);
            }
            bool still_targeted = false;
            for (const WorkflowEdge& edge : working.edges) {
              if (edge.kind == WorkflowEdgeKind::kLoopBack &&
                  edge.to == header_node) {
                still_targeted = true;
                break;
              }
            }
            if (!still_targeted) {
              if (WorkflowNode* header =
                      FindMutableNode(working, header_node)) {
                header->max_iterations = 0;
              }
            }
            return base::ok();
          },
          from_node, header_node));
}

WorkflowDefinition DuplicateWorkflow(const WorkflowDefinition& definition,
                                     const WorkflowClock& clock) {
  WorkflowDefinition copy = definition;
  copy.id = WorkflowId::GenerateNew();
  copy.name = definition.name + " (copy)";
  copy.version = 1;
  copy.created_at = clock.Run();
  copy.updated_at = copy.created_at;
  copy.last_run.reset();
  return copy;
}

base::DictValue ExportWorkflow(const WorkflowDefinition& definition) {
  base::DictValue dict;
  dict.Set("schema_version", kWorkflowSchemaVersion);
  dict.Set("id", definition.id.value());
  dict.Set("name", definition.name);
  dict.Set("description", definition.description);
  base::ListValue params;
  for (const WorkflowParam& param : definition.params) {
    base::DictValue param_dict;
    param_dict.Set("name", param.field.name);
    param_dict.Set("kind", SchemaFieldKindName(param.field.kind));
    param_dict.Set("required", param.field.required);
    if (!param.field.enum_values.empty()) {
      base::ListValue enum_values;
      for (const std::string& value : param.field.enum_values) {
        enum_values.Append(value);
      }
      param_dict.Set("enum_values", std::move(enum_values));
    }
    if (!param.default_value.is_none()) {
      param_dict.Set("default", param.default_value.Clone());
    }
    params.Append(std::move(param_dict));
  }
  dict.Set("params", std::move(params));
  base::ListValue nodes;
  for (const WorkflowNode& node : definition.nodes) {
    base::DictValue node_dict;
    node_dict.Set("id", node.id);
    node_dict.Set("kind", NodeKindName(node.kind));
    node_dict.Set("label", node.label);
    if (node.tool.is_valid()) {
      node_dict.Set("tool", node.tool.value());
    }
    if (!node.args.empty()) {
      node_dict.Set("args", node.args.Clone());
    }
    if (!node.prompt.empty()) {
      node_dict.Set("prompt", node.prompt);
    }
    if (node.requires_approval) {
      node_dict.Set("requires_approval", true);
    }
    if (node.max_iterations > 0) {
      node_dict.Set("max_iterations", node.max_iterations);
    }
    nodes.Append(std::move(node_dict));
  }
  dict.Set("nodes", std::move(nodes));
  base::ListValue edges;
  for (const WorkflowEdge& edge : definition.edges) {
    base::DictValue edge_dict;
    edge_dict.Set("from", edge.from);
    edge_dict.Set("to", edge.to);
    edge_dict.Set("kind", EdgeKindName(edge.kind));
    edges.Append(std::move(edge_dict));
  }
  dict.Set("edges", std::move(edges));
  base::DictValue trigger;
  trigger.Set("kind", TriggerKindName(definition.trigger.kind));
  if (definition.trigger.interval_minutes > 0) {
    trigger.Set("interval_minutes", definition.trigger.interval_minutes);
  }
  if (!definition.trigger.scene_id.empty()) {
    trigger.Set("scene_id", definition.trigger.scene_id);
  }
  if (!definition.trigger.origin_pattern.empty()) {
    trigger.Set("origin_pattern", definition.trigger.origin_pattern);
  }
  if (!definition.trigger.event_name.empty()) {
    trigger.Set("event_name", definition.trigger.event_name);
  }
  dict.Set("trigger", std::move(trigger));
  if (!definition.scene_scope.empty()) {
    dict.Set("scene_scope", definition.scene_scope);
  }
  if (!definition.site_scope.empty()) {
    dict.Set("site_scope", definition.site_scope);
  }
  dict.Set("version", definition.version);
  dict.Set("created_at_ms", MillisFromTime(definition.created_at));
  dict.Set("updated_at_ms", MillisFromTime(definition.updated_at));
  return dict;
}

WorkflowResult<WorkflowDefinition> ImportWorkflow(const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected(WorkflowError::kUnsupportedSchema);
  }
  if (dict->FindInt("schema_version").value_or(0) != kWorkflowSchemaVersion) {
    return base::unexpected(WorkflowError::kUnsupportedSchema);
  }
  WorkflowDefinition definition;
  if (const std::string* id = dict->FindString("id")) {
    definition.id = WorkflowId::FromString(*id);
  }
  if (!definition.id.is_valid()) {
    definition.id = WorkflowId::GenerateNew();
  }
  const std::string* name = dict->FindString("name");
  if (!name) {
    return base::unexpected(WorkflowError::kInvalidName);
  }
  definition.name = *name;
  if (const std::string* description = dict->FindString("description")) {
    definition.description = *description;
  }
  if (const base::ListValue* params = dict->FindList("params")) {
    for (const base::Value& param_value : *params) {
      const base::DictValue* param_dict = param_value.GetIfDict();
      if (!param_dict) {
        return base::unexpected(WorkflowError::kInvalidParam);
      }
      WorkflowParam param;
      const std::string* param_name = param_dict->FindString("name");
      const std::string* kind = param_dict->FindString("kind");
      if (!param_name || !kind ||
          !SchemaFieldKindFromName(*kind, &param.field.kind)) {
        return base::unexpected(WorkflowError::kInvalidParam);
      }
      param.field.name = *param_name;
      param.field.required = param_dict->FindBool("required").value_or(false);
      if (const base::ListValue* enum_values =
              param_dict->FindList("enum_values")) {
        for (const base::Value& enum_value : *enum_values) {
          if (!enum_value.is_string()) {
            return base::unexpected(WorkflowError::kInvalidParam);
          }
          param.field.enum_values.push_back(enum_value.GetString());
        }
      }
      if (const base::Value* default_value = param_dict->Find("default")) {
        param.default_value = default_value->Clone();
      }
      definition.params.push_back(std::move(param));
    }
  }
  const base::ListValue* nodes = dict->FindList("nodes");
  if (!nodes) {
    return base::unexpected(WorkflowError::kEmptyWorkflow);
  }
  for (const base::Value& node_value : *nodes) {
    const base::DictValue* node_dict = node_value.GetIfDict();
    if (!node_dict) {
      return base::unexpected(WorkflowError::kInvalidNodeId);
    }
    WorkflowNode node;
    const std::string* id = node_dict->FindString("id");
    const std::string* kind = node_dict->FindString("kind");
    if (!id || !kind || !NodeKindFromName(*kind, &node.kind)) {
      return base::unexpected(WorkflowError::kInvalidNodeId);
    }
    node.id = *id;
    if (const std::string* label = node_dict->FindString("label")) {
      node.label = *label;
    }
    if (const std::string* tool = node_dict->FindString("tool")) {
      node.tool = ToolId::FromString(*tool);
    }
    if (const base::DictValue* args = node_dict->FindDict("args")) {
      node.args = args->Clone();
    }
    if (const std::string* prompt = node_dict->FindString("prompt")) {
      node.prompt = *prompt;
    }
    node.requires_approval =
        node_dict->FindBool("requires_approval").value_or(false);
    node.max_iterations = node_dict->FindInt("max_iterations").value_or(0);
    definition.nodes.push_back(std::move(node));
  }
  if (const base::ListValue* edges = dict->FindList("edges")) {
    for (const base::Value& edge_value : *edges) {
      const base::DictValue* edge_dict = edge_value.GetIfDict();
      if (!edge_dict) {
        return base::unexpected(WorkflowError::kEdgeUnknownNode);
      }
      WorkflowEdge edge;
      const std::string* from = edge_dict->FindString("from");
      const std::string* to = edge_dict->FindString("to");
      const std::string* kind = edge_dict->FindString("kind");
      if (!from || !to || !kind || !EdgeKindFromName(*kind, &edge.kind)) {
        return base::unexpected(WorkflowError::kEdgeUnknownNode);
      }
      edge.from = *from;
      edge.to = *to;
      definition.edges.push_back(edge);
    }
  }
  if (const base::DictValue* trigger = dict->FindDict("trigger")) {
    const std::string* kind = trigger->FindString("kind");
    if (!kind || !TriggerKindFromName(*kind, &definition.trigger.kind)) {
      return base::unexpected(WorkflowError::kInvalidTrigger);
    }
    definition.trigger.interval_minutes =
        trigger->FindInt("interval_minutes").value_or(0);
    if (const std::string* scene_id = trigger->FindString("scene_id")) {
      definition.trigger.scene_id = *scene_id;
    }
    if (const std::string* origin = trigger->FindString("origin_pattern")) {
      definition.trigger.origin_pattern = *origin;
    }
    if (const std::string* event_name = trigger->FindString("event_name")) {
      definition.trigger.event_name = *event_name;
    }
  }
  if (const std::string* scene_scope = dict->FindString("scene_scope")) {
    definition.scene_scope = *scene_scope;
  }
  if (const std::string* site_scope = dict->FindString("site_scope")) {
    definition.site_scope = *site_scope;
  }
  definition.version = dict->FindInt("version").value_or(1);
  definition.created_at =
      TimeFromMillis(dict->FindDouble("created_at_ms").value_or(0.0));
  definition.updated_at =
      TimeFromMillis(dict->FindDouble("updated_at_ms").value_or(0.0));
  if (auto valid = ValidateWorkflowStructure(definition); !valid.has_value()) {
    return base::unexpected(valid.error());
  }
  return definition;
}

}  // namespace seoul
