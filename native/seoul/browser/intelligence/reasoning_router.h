// Project Seoul hybrid intelligence.
// The reasoning router chooses the cheapest execution route that meets the
// quality threshold for one reasoning step: deterministic code first, then a
// local model when it is sufficient, then an official cloud API when allowed.
// It never picks a route the policy forbids (cloud disabled, privacy too high,
// budget exhausted) and it explains its choice.

#ifndef SEOUL_BROWSER_INTELLIGENCE_REASONING_ROUTER_H_
#define SEOUL_BROWSER_INTELLIGENCE_REASONING_ROUTER_H_

#include <cstdint>
#include <string>

#include "seoul/browser/intelligence/model_provider.h"

namespace seoul {

// Kinds of reasoning steps. Deterministic kinds must never reach a model.
enum class ReasoningKind {
  // Deterministic-only: a model call for any of these is a routing bug.
  kExactBrowserCommand,
  kKnownSceneOperation,
  kArithmetic,
  kSorting,
  kFiltering,
  kChartRendering,
  kWorkflowExecution,
  kStructuredDataFormatting,
  kAlreadySelectedToolCall,
  // Model-eligible.
  kGeneralPlanning,
  kSummarization,
  kClassification,
  kOpenEndedGeneration,
  kVisionUnderstanding,
};

// True for kinds that must be handled by deterministic code.
bool IsDeterministicKind(ReasoningKind kind);

enum class PrivacyLevel {
  kPublic,        // no user data leaves the device concern
  kOrganization,  // workspace metadata
  kPersonal,      // documents/mail/calendar
  kSensitive,     // credential-adjacent; never leaves the device
};

// Live routing inputs.
struct RoutingPolicy {
  bool cloud_enabled = false;
  bool local_available = false;
  PrivacyLevel privacy = PrivacyLevel::kOrganization;
  int64_t remaining_budget_microdollars = 0;
  int estimated_input_tokens = 256;
  int estimated_output_tokens = 512;
  // Minimum planning quality the step needs (0-100). A route qualifies only
  // if its provider meets this.
  int required_quality = 0;
  bool prefer_local = true;  // user preference: keep work on-device when it
                             // meets the threshold
  bool needs_vision = false;
};

enum class RouteDecision {
  kDeterministic,
  kLocal,
  kCloud,
  kUnavailable,  // nothing legal meets the requirements
};

const char* RouteDecisionToString(RouteDecision decision);

enum class RouteReason {
  kDeterministicKind,
  kLocalMeetsThreshold,
  kLocalPreferredAndSufficient,
  kCloudNeededForQuality,
  kCloudNeededForVision,
  kSensitiveStaysLocal,
  kCloudDisabled,
  kNoLocalModel,
  kBudgetExhausted,
  kNoQualifyingRoute,
};

const char* RouteReasonToString(RouteReason reason);

struct RoutingOutcome {
  RouteDecision decision = RouteDecision::kUnavailable;
  RouteReason reason = RouteReason::kNoQualifyingRoute;
  int64_t estimated_cost_microdollars = 0;
};

// Chooses a route. `local` and `cloud` may be null when unavailable; the
// policy flags must still reflect availability (the router double-checks).
RoutingOutcome RouteReasoning(ReasoningKind kind,
                              const RoutingPolicy& policy,
                              const ModelProvider* local,
                              const ModelProvider* cloud);

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_REASONING_ROUTER_H_
