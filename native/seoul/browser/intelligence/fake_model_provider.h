// Project Seoul hybrid intelligence.
// Deterministic fake model provider for tests. Returns scripted results and
// records requests; never performs I/O. Test support only.

#ifndef SEOUL_BROWSER_INTELLIGENCE_FAKE_MODEL_PROVIDER_H_
#define SEOUL_BROWSER_INTELLIGENCE_FAKE_MODEL_PROVIDER_H_

#include <string>

#include "seoul/browser/intelligence/model_provider.h"

namespace seoul {

class FakeModelProvider : public ModelProvider {
 public:
  FakeModelProvider(std::string id, ModelCapabilities capabilities);
  ~FakeModelProvider() override;

  std::string provider_id() const override;
  ModelCapabilities capabilities() const override;
  void Generate(const GenerationRequest& request,
                GenerateCallback callback) override;
  void Cancel() override;
  int64_t EstimateCostMicrodollars(int input_tokens,
                                   int output_tokens) const override;

  void set_next_text(const std::string& text) { next_text_ = text; }
  void set_should_fail(bool fail) { should_fail_ = fail; }
  int generate_calls() const { return generate_calls_; }

 private:
  std::string id_;
  ModelCapabilities capabilities_;
  std::string next_text_ = "ok";
  bool should_fail_ = false;
  int generate_calls_ = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_FAKE_MODEL_PROVIDER_H_
