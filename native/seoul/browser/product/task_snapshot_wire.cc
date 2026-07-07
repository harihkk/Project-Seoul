// Project Seoul product runtime.

#include "seoul/browser/product/task_snapshot_wire.h"

#include <cmath>
#include <utility>

#include "base/time/time.h"

namespace seoul {

namespace {

constexpr int kWireProtocolVersion = 1;

base::Time TimeFromMillis(double ms) {
  return base::Time::UnixEpoch() + base::Milliseconds(ms);
}

double MillisFromTime(base::Time t) {
  return (t - base::Time::UnixEpoch()).InMillisecondsF();
}

// Costs are int64 in the structs; base::Value has no int64, so the wire
// carries them as JSON numbers (exact for integers up to 2^53, far above any
// budget ceiling). Parsing rejects non-integral, negative, or unsafe values
// instead of silently truncating through a 32-bit read.
constexpr double kMaxSafeWireInteger = 9007199254740991.0;  // 2^53 - 1

bool ReadCost(const base::DictValue& dict,
              std::string_view key,
              int64_t* out) {
  std::optional<double> value = dict.FindDouble(key);
  if (!value.has_value()) {
    *out = 0;
    return true;
  }
  if (!std::isfinite(*value) || *value < 0.0 ||
      *value > kMaxSafeWireInteger || *value != std::floor(*value)) {
    return false;
  }
  *out = static_cast<int64_t>(*value);
  return true;
}

base::DictValue ReceiptToValue(const ActionReceipt& receipt) {
  base::DictValue dict;
  dict.Set("step_id", receipt.step_id);
  dict.Set("tool", receipt.tool.value());
  dict.Set("status", StepStatusToString(receipt.status));
  if (!receipt.started_at.is_null()) {
    dict.Set("started_at_ms", MillisFromTime(receipt.started_at));
  }
  if (!receipt.finished_at.is_null()) {
    dict.Set("finished_at_ms", MillisFromTime(receipt.finished_at));
  }
  if (!receipt.observed_summary.empty()) {
    dict.Set("observed_summary", receipt.observed_summary);
  }
  base::DictValue verification;
  verification.Set("verified", receipt.verification.verified);
  verification.Set("method", receipt.verification.method);
  if (!receipt.verification.detail.empty()) {
    verification.Set("detail", receipt.verification.detail);
  }
  dict.Set("verification", std::move(verification));
  dict.Set("route", ExecutionRouteToString(receipt.route));
  dict.Set("cost_microdollars",
           static_cast<double>(receipt.cost_microdollars));
  dict.Set("model_calls", receipt.model_calls);
  dict.Set("navigations", receipt.navigations);
  return dict;
}

base::expected<ActionReceipt, std::string> ParseReceipt(
    const base::DictValue& dict) {
  ActionReceipt receipt;
  const std::string* step_id = dict.FindString("step_id");
  const std::string* tool = dict.FindString("tool");
  const std::string* status = dict.FindString("status");
  const std::string* route = dict.FindString("route");
  const base::DictValue* verification = dict.FindDict("verification");
  if (!step_id || step_id->empty()) {
    return base::unexpected("receipt.step_id required");
  }
  receipt.step_id = *step_id;
  if (!tool) {
    return base::unexpected("receipt.tool required");
  }
  receipt.tool = ToolId::FromString(*tool);
  if (!receipt.tool.is_valid()) {
    return base::unexpected("receipt.tool invalid: " + *tool);
  }
  if (!status || !StepStatusFromWire(*status, &receipt.status)) {
    return base::unexpected("receipt.status invalid");
  }
  if (std::optional<double> started = dict.FindDouble("started_at_ms")) {
    if (!std::isfinite(*started)) {
      return base::unexpected("receipt.started_at_ms not finite");
    }
    receipt.started_at = TimeFromMillis(*started);
  }
  if (std::optional<double> finished = dict.FindDouble("finished_at_ms")) {
    if (!std::isfinite(*finished)) {
      return base::unexpected("receipt.finished_at_ms not finite");
    }
    receipt.finished_at = TimeFromMillis(*finished);
  }
  if (const std::string* summary = dict.FindString("observed_summary")) {
    receipt.observed_summary = *summary;
  }
  if (!verification) {
    return base::unexpected("receipt.verification required");
  }
  std::optional<bool> verified = verification->FindBool("verified");
  const std::string* method = verification->FindString("method");
  if (!verified.has_value() || !method) {
    return base::unexpected("receipt.verification.verified/method required");
  }
  receipt.verification.verified = *verified;
  receipt.verification.method = *method;
  if (const std::string* detail = verification->FindString("detail")) {
    receipt.verification.detail = *detail;
  }
  if (!route || !ExecutionRouteFromWire(*route, &receipt.route)) {
    return base::unexpected("receipt.route invalid");
  }
  if (!ReadCost(dict, "cost_microdollars", &receipt.cost_microdollars)) {
    return base::unexpected("receipt.cost_microdollars out of range");
  }
  receipt.model_calls = dict.FindInt("model_calls").value_or(0);
  receipt.navigations = dict.FindInt("navigations").value_or(0);
  if (receipt.model_calls < 0 || receipt.navigations < 0) {
    return base::unexpected("receipt counters must be non-negative");
  }
  return receipt;
}

}  // namespace

const char* PlanOriginToWire(PlanOrigin origin) {
  switch (origin) {
    case PlanOrigin::kDeterministic:
      return "deterministic";
    case PlanOrigin::kLocalModel:
      return "local";
    case PlanOrigin::kCloudModel:
      return "cloud";
  }
  return "deterministic";
}

bool PlanOriginFromWire(std::string_view s, PlanOrigin* out) {
  if (s == "deterministic") {
    *out = PlanOrigin::kDeterministic;
    return true;
  }
  if (s == "local") {
    *out = PlanOrigin::kLocalModel;
    return true;
  }
  if (s == "cloud") {
    *out = PlanOrigin::kCloudModel;
    return true;
  }
  return false;
}

bool TaskStateFromWire(std::string_view s, TaskState* out) {
  constexpr TaskState kAll[] = {
      TaskState::kDraft,     TaskState::kPlanning,
      TaskState::kAwaitingApproval, TaskState::kExecuting,
      TaskState::kPaused,    TaskState::kMonitoring,
      TaskState::kCompleted, TaskState::kFailed,
      TaskState::kCancelled};
  for (TaskState state : kAll) {
    if (s == TaskStateToString(state)) {
      *out = state;
      return true;
    }
  }
  return false;
}

bool StepStatusFromWire(std::string_view s, StepStatus* out) {
  constexpr StepStatus kAll[] = {
      StepStatus::kPending,   StepStatus::kAwaitingApproval,
      StepStatus::kAwaitingInput, StepStatus::kRunning,
      StepStatus::kSucceeded, StepStatus::kFailed,
      StepStatus::kOutcomeUnknown, StepStatus::kSkipped,
      StepStatus::kCancelled};
  for (StepStatus status : kAll) {
    if (s == StepStatusToString(status)) {
      *out = status;
      return true;
    }
  }
  return false;
}

bool ExecutionRouteFromWire(std::string_view s, ExecutionRoute* out) {
  constexpr ExecutionRoute kAll[] = {ExecutionRoute::kDeterministic,
                                     ExecutionRoute::kLocalModel,
                                     ExecutionRoute::kCloudModel};
  for (ExecutionRoute route : kAll) {
    if (s == ExecutionRouteToString(route)) {
      *out = route;
      return true;
    }
  }
  return false;
}

bool TaskFailureReasonFromWire(std::string_view s, TaskFailureReason* out) {
  constexpr TaskFailureReason kAll[] = {
      TaskFailureReason::kNone,          TaskFailureReason::kStepFailed,
      TaskFailureReason::kBudgetExhausted,
      TaskFailureReason::kAssumptionInvalid,
      TaskFailureReason::kUserStopped,
      TaskFailureReason::kProviderUnavailable};
  for (TaskFailureReason reason : kAll) {
    if (s == TaskFailureReasonToString(reason)) {
      *out = reason;
      return true;
    }
  }
  return false;
}

base::DictValue TaskSnapshotToValue(const TaskSnapshot& snapshot) {
  base::DictValue dict;
  dict.Set("schema_version", kWireProtocolVersion);
  dict.Set("id", snapshot.id.value());
  dict.Set("goal", snapshot.goal);
  dict.Set("state", TaskStateToString(snapshot.state));
  if (snapshot.failure != TaskFailureReason::kNone) {
    dict.Set("failure", TaskFailureReasonToString(snapshot.failure));
  }
  dict.Set("plan_origin", PlanOriginToWire(snapshot.plan_origin));
  if (!snapshot.receipts.empty()) {
    base::ListValue receipts;
    for (const ActionReceipt& receipt : snapshot.receipts) {
      receipts.Append(ReceiptToValue(receipt));
    }
    dict.Set("receipts", std::move(receipts));
  }
  base::DictValue usage;
  usage.Set("steps_executed", snapshot.usage.steps_executed);
  usage.Set("model_calls", snapshot.usage.model_calls);
  usage.Set("navigations", snapshot.usage.navigations);
  usage.Set("cloud_cost_microdollars",
            static_cast<double>(snapshot.usage.cloud_cost_microdollars));
  usage.Set("replans_used", snapshot.usage.replans_used);
  dict.Set("usage", std::move(usage));
  if (!snapshot.pending_approval_step.empty()) {
    dict.Set("pending_approval_step", snapshot.pending_approval_step);
    dict.Set("pending_approval_prompt", snapshot.pending_approval_prompt);
  }
  dict.Set("has_semantic_result", snapshot.has_semantic_result);
  if (snapshot.window.is_valid()) {
    dict.Set("window", snapshot.window.value());
  }
  dict.Set("replans_used", snapshot.replans_used);
  return dict;
}

base::expected<TaskSnapshot, std::string> ParseTaskSnapshot(
    const base::Value& value) {
  const base::DictValue* dict = value.GetIfDict();
  if (!dict) {
    return base::unexpected("snapshot must be an object");
  }
  std::optional<int> schema_version = dict->FindInt("schema_version");
  if (!schema_version.has_value() ||
      *schema_version != kWireProtocolVersion) {
    return base::unexpected("unsupported snapshot schema_version");
  }
  TaskSnapshot snapshot;
  const std::string* id = dict->FindString("id");
  if (!id) {
    return base::unexpected("id required");
  }
  snapshot.id = TaskId::FromString(*id);
  if (!snapshot.id.is_valid()) {
    return base::unexpected("invalid task id: " + *id);
  }
  const std::string* goal = dict->FindString("goal");
  if (!goal || goal->size() > kMaxGoalLength) {
    return base::unexpected("goal required and bounded");
  }
  snapshot.goal = *goal;
  const std::string* state = dict->FindString("state");
  if (!state || !TaskStateFromWire(*state, &snapshot.state)) {
    return base::unexpected("state invalid");
  }
  if (const std::string* failure = dict->FindString("failure")) {
    if (!TaskFailureReasonFromWire(*failure, &snapshot.failure)) {
      return base::unexpected("failure invalid");
    }
  }
  if (const std::string* origin = dict->FindString("plan_origin")) {
    if (!PlanOriginFromWire(*origin, &snapshot.plan_origin)) {
      return base::unexpected("plan_origin invalid");
    }
  }
  if (const base::ListValue* receipts = dict->FindList("receipts")) {
    if (receipts->size() > kMaxReceiptsPerTask) {
      return base::unexpected("too many receipts");
    }
    for (const base::Value& receipt_value : *receipts) {
      const base::DictValue* receipt_dict = receipt_value.GetIfDict();
      if (!receipt_dict) {
        return base::unexpected("receipt must be an object");
      }
      auto receipt = ParseReceipt(*receipt_dict);
      if (!receipt.has_value()) {
        return base::unexpected(receipt.error());
      }
      snapshot.receipts.push_back(std::move(receipt.value()));
    }
  }
  if (const base::DictValue* usage = dict->FindDict("usage")) {
    snapshot.usage.steps_executed =
        usage->FindInt("steps_executed").value_or(0);
    snapshot.usage.model_calls = usage->FindInt("model_calls").value_or(0);
    snapshot.usage.navigations = usage->FindInt("navigations").value_or(0);
    if (!ReadCost(*usage, "cloud_cost_microdollars",
                  &snapshot.usage.cloud_cost_microdollars)) {
      return base::unexpected("usage.cloud_cost_microdollars out of range");
    }
    snapshot.usage.replans_used = usage->FindInt("replans_used").value_or(0);
    if (snapshot.usage.steps_executed < 0 || snapshot.usage.model_calls < 0 ||
        snapshot.usage.navigations < 0 || snapshot.usage.replans_used < 0) {
      return base::unexpected("usage counters must be non-negative");
    }
  }
  if (const std::string* step = dict->FindString("pending_approval_step")) {
    snapshot.pending_approval_step = *step;
  }
  if (const std::string* prompt =
          dict->FindString("pending_approval_prompt")) {
    snapshot.pending_approval_prompt = *prompt;
  }
  snapshot.has_semantic_result =
      dict->FindBool("has_semantic_result").value_or(false);
  if (const std::string* window = dict->FindString("window")) {
    snapshot.window = LiveWindowKey::Parse(*window);
    if (!snapshot.window.is_valid()) {
      return base::unexpected("window key invalid: " + *window);
    }
  }
  snapshot.replans_used = dict->FindInt("replans_used").value_or(0);
  if (snapshot.replans_used < 0) {
    return base::unexpected("replans_used must be non-negative");
  }
  return snapshot;
}

}  // namespace seoul
