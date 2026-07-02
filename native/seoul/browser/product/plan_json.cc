// Project Seoul product runtime: plan serialization.

#include "seoul/browser/product/plan_json.h"

#include <utility>

namespace seoul {

namespace {

constexpr size_t kMaxSerializedSteps = 128;

const char* StepKindToString(PlanStepKind kind) {
  switch (kind) {
    case PlanStepKind::kToolCall:
      return "tool_call";
    case PlanStepKind::kApprovalGate:
      return "approval_gate";
    case PlanStepKind::kUserInput:
      return "user_input";
  }
  return "tool_call";
}

std::optional<PlanStepKind> StepKindFromString(std::string_view kind) {
  if (kind == "tool_call") {
    return PlanStepKind::kToolCall;
  }
  if (kind == "approval_gate") {
    return PlanStepKind::kApprovalGate;
  }
  if (kind == "user_input") {
    return PlanStepKind::kUserInput;
  }
  return std::nullopt;
}

}  // namespace

base::Value::Dict PlanToValue(const Plan& plan) {
  base::Value::Dict dict;
  dict.Set("goal", plan.goal);
  base::Value::List steps;
  for (const PlanStep& step : plan.steps) {
    base::Value::Dict s;
    s.Set("id", step.id);
    s.Set("kind", StepKindToString(step.kind));
    if (step.kind == PlanStepKind::kToolCall) {
      s.Set("tool", step.tool.value());
      s.Set("args", step.args.Clone());
    } else {
      s.Set("prompt", step.prompt);
    }
    if (step.requires_approval) {
      s.Set("requires_approval", true);
    }
    if (step.guard.has_value()) {
      base::Value::Dict guard;
      guard.Set("depends_on_step", step.guard->depends_on_step);
      guard.Set("require_success", step.guard->require_success);
      s.Set("guard", std::move(guard));
    }
    if (step.parallel_group != 0) {
      s.Set("parallel_group", step.parallel_group);
    }
    if (step.loop_group != 0) {
      s.Set("loop_group", step.loop_group);
      s.Set("max_iterations", step.max_iterations);
    }
    steps.Append(std::move(s));
  }
  dict.Set("steps", std::move(steps));
  return dict;
}

std::optional<Plan> PlanFromValue(const base::Value::Dict& value) {
  const std::string* goal = value.FindString("goal");
  const base::Value::List* steps = value.FindList("steps");
  if (!goal || !steps || steps->empty() ||
      steps->size() > kMaxSerializedSteps) {
    return std::nullopt;
  }
  Plan plan;
  plan.goal = *goal;
  for (const base::Value& entry : *steps) {
    const base::Value::Dict* s = entry.GetIfDict();
    if (!s) {
      return std::nullopt;
    }
    const std::string* id = s->FindString("id");
    const std::string* kind_string = s->FindString("kind");
    if (!id || !kind_string) {
      return std::nullopt;
    }
    const std::optional<PlanStepKind> kind = StepKindFromString(*kind_string);
    if (!kind.has_value()) {
      return std::nullopt;
    }
    PlanStep step;
    step.id = *id;
    step.kind = kind.value();
    if (step.kind == PlanStepKind::kToolCall) {
      const std::string* tool = s->FindString("tool");
      if (!tool || !ToolId::IsValidString(*tool)) {
        return std::nullopt;
      }
      step.tool = ToolId::FromString(*tool);
      if (const base::Value::Dict* args = s->FindDict("args")) {
        step.args = args->Clone();
      }
    } else {
      const std::string* prompt = s->FindString("prompt");
      if (!prompt || prompt->empty()) {
        return std::nullopt;
      }
      step.prompt = *prompt;
    }
    step.requires_approval = s->FindBool("requires_approval").value_or(false);
    if (const base::Value::Dict* guard = s->FindDict("guard")) {
      const std::string* depends = guard->FindString("depends_on_step");
      if (!depends || depends->empty()) {
        return std::nullopt;
      }
      StepGuard parsed_guard;
      parsed_guard.depends_on_step = *depends;
      parsed_guard.require_success =
          guard->FindBool("require_success").value_or(true);
      step.guard = std::move(parsed_guard);
    }
    step.parallel_group = s->FindInt("parallel_group").value_or(0);
    step.loop_group = s->FindInt("loop_group").value_or(0);
    step.max_iterations = s->FindInt("max_iterations").value_or(0);
    plan.steps.push_back(std::move(step));
  }
  return plan;
}

}  // namespace seoul
