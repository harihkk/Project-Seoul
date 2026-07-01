// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/reasoning_router.h"

namespace seoul {

bool IsDeterministicKind(ReasoningKind kind) {
  switch (kind) {
    case ReasoningKind::kExactBrowserCommand:
    case ReasoningKind::kKnownSceneOperation:
    case ReasoningKind::kArithmetic:
    case ReasoningKind::kSorting:
    case ReasoningKind::kFiltering:
    case ReasoningKind::kChartRendering:
    case ReasoningKind::kWorkflowExecution:
    case ReasoningKind::kStructuredDataFormatting:
    case ReasoningKind::kAlreadySelectedToolCall:
      return true;
    case ReasoningKind::kGeneralPlanning:
    case ReasoningKind::kSummarization:
    case ReasoningKind::kClassification:
    case ReasoningKind::kOpenEndedGeneration:
    case ReasoningKind::kVisionUnderstanding:
      return false;
  }
  return false;
}

namespace {

bool LocalQualifies(const RoutingPolicy& policy, const ModelProvider* local) {
  if (!policy.local_available || !local) {
    return false;
  }
  const ModelCapabilities caps = local->capabilities();
  if (policy.needs_vision && !caps.vision) {
    return false;
  }
  return caps.planning_quality >= policy.required_quality;
}

bool CloudQualifies(const RoutingPolicy& policy,
                    const ModelProvider* cloud,
                    int64_t* estimated_cost) {
  if (!policy.cloud_enabled || !cloud) {
    return false;
  }
  // Sensitive reasoning never leaves the device, regardless of cloud
  // availability.
  if (policy.privacy == PrivacyLevel::kSensitive) {
    return false;
  }
  const ModelCapabilities caps = cloud->capabilities();
  if (policy.needs_vision && !caps.vision) {
    return false;
  }
  if (caps.planning_quality < policy.required_quality) {
    return false;
  }
  const int64_t cost = cloud->EstimateCostMicrodollars(
      policy.estimated_input_tokens, policy.estimated_output_tokens);
  if (cost > policy.remaining_budget_microdollars) {
    return false;
  }
  *estimated_cost = cost;
  return true;
}

}  // namespace

RoutingOutcome RouteReasoning(ReasoningKind kind,
                              const RoutingPolicy& policy,
                              const ModelProvider* local,
                              const ModelProvider* cloud) {
  RoutingOutcome outcome;
  if (IsDeterministicKind(kind)) {
    outcome.decision = RouteDecision::kDeterministic;
    outcome.reason = RouteReason::kDeterministicKind;
    return outcome;
  }

  const bool local_ok = LocalQualifies(policy, local);
  int64_t cloud_cost = 0;
  const bool cloud_ok = CloudQualifies(policy, cloud, &cloud_cost);

  // Prefer local when it meets the threshold: it is cheaper (free) and keeps
  // data on-device. This is the "cheapest method that meets quality" rule.
  if (local_ok && (policy.prefer_local || !cloud_ok)) {
    outcome.decision = RouteDecision::kLocal;
    outcome.reason = policy.prefer_local
                         ? RouteReason::kLocalPreferredAndSufficient
                         : RouteReason::kLocalMeetsThreshold;
    return outcome;
  }

  if (cloud_ok) {
    outcome.decision = RouteDecision::kCloud;
    outcome.reason = policy.needs_vision ? RouteReason::kCloudNeededForVision
                                         : RouteReason::kCloudNeededForQuality;
    outcome.estimated_cost_microdollars = cloud_cost;
    return outcome;
  }

  // Local qualifies but was not preferred and cloud is unavailable: still use
  // local rather than failing.
  if (local_ok) {
    outcome.decision = RouteDecision::kLocal;
    outcome.reason = RouteReason::kLocalMeetsThreshold;
    return outcome;
  }

  // Nothing qualified: report the most specific reason.
  outcome.decision = RouteDecision::kUnavailable;
  if (policy.privacy == PrivacyLevel::kSensitive && policy.cloud_enabled &&
      cloud) {
    outcome.reason = RouteReason::kSensitiveStaysLocal;
  } else if (!policy.local_available || !local) {
    outcome.reason = policy.cloud_enabled ? RouteReason::kNoLocalModel
                                          : RouteReason::kCloudDisabled;
  } else if (!policy.cloud_enabled) {
    outcome.reason = RouteReason::kCloudDisabled;
  } else if (cloud &&
             cloud->EstimateCostMicrodollars(policy.estimated_input_tokens,
                                             policy.estimated_output_tokens) >
                 policy.remaining_budget_microdollars) {
    outcome.reason = RouteReason::kBudgetExhausted;
  } else {
    outcome.reason = RouteReason::kNoQualifyingRoute;
  }
  return outcome;
}

const char* RouteDecisionToString(RouteDecision decision) {
  switch (decision) {
    case RouteDecision::kDeterministic:
      return "deterministic";
    case RouteDecision::kLocal:
      return "local";
    case RouteDecision::kCloud:
      return "cloud";
    case RouteDecision::kUnavailable:
      return "unavailable";
  }
  return "unavailable";
}

const char* RouteReasonToString(RouteReason reason) {
  switch (reason) {
    case RouteReason::kDeterministicKind:
      return "deterministic_kind";
    case RouteReason::kLocalMeetsThreshold:
      return "local_meets_threshold";
    case RouteReason::kLocalPreferredAndSufficient:
      return "local_preferred_and_sufficient";
    case RouteReason::kCloudNeededForQuality:
      return "cloud_needed_for_quality";
    case RouteReason::kCloudNeededForVision:
      return "cloud_needed_for_vision";
    case RouteReason::kSensitiveStaysLocal:
      return "sensitive_stays_local";
    case RouteReason::kCloudDisabled:
      return "cloud_disabled";
    case RouteReason::kNoLocalModel:
      return "no_local_model";
    case RouteReason::kBudgetExhausted:
      return "budget_exhausted";
    case RouteReason::kNoQualifyingRoute:
      return "no_qualifying_route";
  }
  return "no_qualifying_route";
}

}  // namespace seoul
