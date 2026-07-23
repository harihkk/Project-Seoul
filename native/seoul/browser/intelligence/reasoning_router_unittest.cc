// Project Seoul hybrid intelligence.
// Unit tests for deterministic-first, cheapest-qualifying-route routing.

#include "seoul/browser/intelligence/reasoning_router.h"

#include <memory>

#include "seoul/browser/intelligence/fake_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

ModelCapabilities LocalCaps(int quality, bool vision = false) {
  ModelCapabilities caps;
  caps.locality = ModelLocality::kLocal;
  caps.tool_planning = true;
  caps.planning_quality = quality;
  caps.vision = vision;
  return caps;
}

ModelCapabilities CloudCaps(int quality,
                            int64_t out_cost_per_mtok,
                            bool vision = false) {
  ModelCapabilities caps;
  caps.locality = ModelLocality::kCloud;
  caps.tool_planning = true;
  caps.planning_quality = quality;
  caps.vision = vision;
  caps.output_cost_per_mtok_microdollars = out_cost_per_mtok;
  caps.input_cost_per_mtok_microdollars = out_cost_per_mtok / 4;
  return caps;
}

TEST(ReasoningRouterTest, DeterministicKindsNeverReachAModel) {
  FakeModelProvider local("local", LocalCaps(90));
  FakeModelProvider cloud("cloud", CloudCaps(99, 10'000'000));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = true;
  policy.remaining_budget_microdollars = 1'000'000;

  for (ReasoningKind kind :
       {ReasoningKind::kArithmetic, ReasoningKind::kSorting,
        ReasoningKind::kFiltering, ReasoningKind::kChartRendering,
        ReasoningKind::kWorkflowExecution, ReasoningKind::kExactBrowserCommand,
        ReasoningKind::kAlreadySelectedToolCall}) {
    RoutingOutcome outcome = RouteReasoning(kind, policy, &local, &cloud);
    EXPECT_EQ(outcome.decision, RouteDecision::kDeterministic);
    EXPECT_EQ(outcome.reason, RouteReason::kDeterministicKind);
  }
  EXPECT_EQ(local.generate_calls(), 0);
  EXPECT_EQ(cloud.generate_calls(), 0);
}

TEST(ReasoningRouterTest, PrefersLocalWhenItMeetsThreshold) {
  FakeModelProvider local("local", LocalCaps(80));
  FakeModelProvider cloud("cloud", CloudCaps(99, 10'000'000));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = true;
  policy.prefer_local = true;
  policy.required_quality = 70;
  policy.remaining_budget_microdollars = 1'000'000;

  RoutingOutcome outcome =
      RouteReasoning(ReasoningKind::kGeneralPlanning, policy, &local, &cloud);
  EXPECT_EQ(outcome.decision, RouteDecision::kLocal);
  EXPECT_EQ(outcome.reason, RouteReason::kLocalPreferredAndSufficient);
}

TEST(ReasoningRouterTest, EscalatesToCloudWhenLocalIsInsufficient) {
  FakeModelProvider local("local", LocalCaps(60));
  FakeModelProvider cloud("cloud", CloudCaps(95, 2'000'000));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = true;
  policy.required_quality = 85;  // local's 60 is not enough
  policy.remaining_budget_microdollars = 10'000'000;
  policy.estimated_output_tokens = 500;

  RoutingOutcome outcome =
      RouteReasoning(ReasoningKind::kGeneralPlanning, policy, &local, &cloud);
  EXPECT_EQ(outcome.decision, RouteDecision::kCloud);
  EXPECT_EQ(outcome.reason, RouteReason::kCloudNeededForQuality);
  EXPECT_GT(outcome.estimated_cost_microdollars, 0);
}

TEST(ReasoningRouterTest, DefaultsToBestQualifyingRouteWhenBothQualify) {
  FakeModelProvider local("local", LocalCaps(80));
  FakeModelProvider cloud("cloud", CloudCaps(95, 1'000'000));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = true;
  policy.required_quality = 70;  // both meet the bar
  policy.remaining_budget_microdollars = 10'000'000;

  RoutingOutcome outcome =
      RouteReasoning(ReasoningKind::kGeneralPlanning, policy, &local, &cloud);
  // prefer_local is off by default: the best qualifying route wins.
  EXPECT_EQ(outcome.decision, RouteDecision::kCloud);
  EXPECT_EQ(outcome.reason, RouteReason::kCloudNeededForQuality);
  EXPECT_GT(outcome.estimated_cost_microdollars, 0);
}

TEST(ReasoningRouterTest, BudgetCeilingBlocksCloud) {
  FakeModelProvider local("local", LocalCaps(50));
  FakeModelProvider cloud("cloud", CloudCaps(95, 100'000'000));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = true;
  policy.required_quality = 90;
  policy.remaining_budget_microdollars = 1;  // effectively no budget
  policy.estimated_output_tokens = 1000;

  RoutingOutcome outcome = RouteReasoning(ReasoningKind::kOpenEndedGeneration,
                                          policy, &local, &cloud);
  EXPECT_EQ(outcome.decision, RouteDecision::kUnavailable);
  EXPECT_EQ(outcome.reason, RouteReason::kBudgetExhausted);
}

TEST(ReasoningRouterTest, CloudDisabledFallsBackOrFails) {
  FakeModelProvider local("local", LocalCaps(80));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = false;
  policy.required_quality = 70;

  RoutingOutcome outcome =
      RouteReasoning(ReasoningKind::kSummarization, policy, &local, nullptr);
  EXPECT_EQ(outcome.decision, RouteDecision::kLocal);

  policy.required_quality = 95;  // local can't meet it and cloud is off
  outcome =
      RouteReasoning(ReasoningKind::kSummarization, policy, &local, nullptr);
  EXPECT_EQ(outcome.decision, RouteDecision::kUnavailable);
  EXPECT_EQ(outcome.reason, RouteReason::kCloudDisabled);
}

TEST(ReasoningRouterTest, VisionRequirementForcesACapableProvider) {
  FakeModelProvider local("local", LocalCaps(90, /*vision=*/false));
  FakeModelProvider cloud("cloud", CloudCaps(90, 1'000'000, /*vision=*/true));
  RoutingPolicy policy;
  policy.local_available = true;
  policy.cloud_enabled = true;
  policy.needs_vision = true;
  policy.required_quality = 80;
  policy.remaining_budget_microdollars = 10'000'000;

  RoutingOutcome outcome = RouteReasoning(ReasoningKind::kVisionUnderstanding,
                                          policy, &local, &cloud);
  EXPECT_EQ(outcome.decision, RouteDecision::kCloud);
  EXPECT_EQ(outcome.reason, RouteReason::kCloudNeededForVision);
}

TEST(ReasoningRouterTest, NoLocalModelReportedDistinctly) {
  FakeModelProvider cloud("cloud", CloudCaps(95, 1'000'000));
  RoutingPolicy policy;
  policy.local_available = false;
  policy.cloud_enabled = true;
  policy.required_quality = 99;  // cloud's 95 is not enough
  policy.remaining_budget_microdollars = 10'000'000;

  RoutingOutcome outcome =
      RouteReasoning(ReasoningKind::kGeneralPlanning, policy, nullptr, &cloud);
  EXPECT_EQ(outcome.decision, RouteDecision::kUnavailable);
  EXPECT_EQ(outcome.reason, RouteReason::kNoLocalModel);
}

}  // namespace
}  // namespace seoul
