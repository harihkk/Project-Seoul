// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/fake_model_provider.h"

#include <utility>

namespace seoul {

FakeModelProvider::FakeModelProvider(std::string id,
                                     ModelCapabilities capabilities)
    : id_(std::move(id)), capabilities_(capabilities) {}

FakeModelProvider::~FakeModelProvider() = default;

std::string FakeModelProvider::provider_id() const {
  return id_;
}

ModelCapabilities FakeModelProvider::capabilities() const {
  return capabilities_;
}

void FakeModelProvider::Generate(const GenerationRequest& request,
                                 GenerateCallback callback) {
  ++generate_calls_;
  if (should_fail_) {
    std::move(callback).Run(base::unexpected("fake provider error"));
    return;
  }
  GenerationResult result;
  result.text = next_text_;
  result.usage.input_tokens = request.max_output_tokens / 2;
  result.usage.output_tokens = request.max_output_tokens / 2;
  result.usage.cost_microdollars = EstimateCostMicrodollars(
      result.usage.input_tokens, result.usage.output_tokens);
  std::move(callback).Run(std::move(result));
}

void FakeModelProvider::Cancel() {}

int64_t FakeModelProvider::EstimateCostMicrodollars(int input_tokens,
                                                    int output_tokens) const {
  const int64_t input_cost = static_cast<int64_t>(input_tokens) *
                             capabilities_.input_cost_per_mtok_microdollars /
                             1000000;
  const int64_t output_cost = static_cast<int64_t>(output_tokens) *
                              capabilities_.output_cost_per_mtok_microdollars /
                              1000000;
  return input_cost + output_cost;
}

}  // namespace seoul
