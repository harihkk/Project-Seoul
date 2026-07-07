// Project Seoul hybrid intelligence.
// Provider-neutral model interface. A provider is deterministic-free: it does
// text/structured generation, planning, and (optionally) vision, and reports
// its capabilities, context limits, cost estimate, and data-retention policy.
// Core logic never depends on a specific SDK; local runtimes and official
// cloud APIs both implement this seam. Consumer chat subscriptions and website
// scraping are explicitly out of scope.

#ifndef SEOUL_BROWSER_INTELLIGENCE_MODEL_PROVIDER_H_
#define SEOUL_BROWSER_INTELLIGENCE_MODEL_PROVIDER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "base/values.h"

namespace seoul {

enum class ModelLocality {
  kLocal,
  kCloud,
};

// Provider data-retention posture, surfaced before any cloud call so the
// router and the user can weigh privacy.
enum class RetentionPolicy {
  kNoRetention,
  kEphemeralRetention,  // transient, not used for training
  kRetainedNotTrained,
  kUnknown,
};

struct ModelCapabilities {
  bool text_generation = true;
  bool structured_generation = false;
  bool tool_planning = false;
  bool vision = false;
  bool streaming = false;
  int max_context_tokens = 0;
  ModelLocality locality = ModelLocality::kLocal;
  RetentionPolicy retention = RetentionPolicy::kUnknown;
  // Estimated output quality on Seoul's planning eval, 0-100. Set from
  // measured benchmarks, never from marketing; 0 when unmeasured.
  int planning_quality = 0;
  // Per-mega-token pricing in microdollars (0 for local).
  int64_t input_cost_per_mtok_microdollars = 0;
  int64_t output_cost_per_mtok_microdollars = 0;
};

struct GenerationRequest {
  std::string system_prompt;
  std::string user_prompt;
  // When non-empty, the provider must return JSON conforming to this schema
  // (a ToolSchema serialized to JSON); used for plans and structured surfaces.
  base::DictValue response_schema;
  int max_output_tokens = 1024;
  double temperature = 0.2;
};

struct UsageStats {
  int input_tokens = 0;
  int output_tokens = 0;
  int64_t cost_microdollars = 0;
};

struct GenerationResult {
  std::string text;
  base::Value structured;  // set when response_schema was provided
  UsageStats usage;
  bool truncated = false;
};

class ModelProvider {
 public:
  virtual ~ModelProvider() = default;

  virtual std::string provider_id() const = 0;  // "local:llama-3.2-3b" etc.
  virtual ModelCapabilities capabilities() const = 0;

  using GenerateCallback = base::OnceCallback<void(
      base::expected<GenerationResult, std::string> result)>;
  virtual void Generate(const GenerationRequest& request,
                        GenerateCallback callback) = 0;
  virtual void Cancel() = 0;

  // Up-front cost estimate in microdollars for a request of the given
  // approximate token sizes (0 for local providers).
  virtual int64_t EstimateCostMicrodollars(int input_tokens,
                                           int output_tokens) const = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_MODEL_PROVIDER_H_
