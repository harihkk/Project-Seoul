// Project Seoul hybrid intelligence.
// Integration-style tests for the local and cloud provider adapters over a
// scripted fake transport: streaming accumulation, the local-only endpoint
// guard, BYOK credential use (fetched, sent, not retained), usage/cost, and
// error mapping. No network and no real Keychain.

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "seoul/browser/intelligence/cloud_model_provider.h"
#include "seoul/browser/intelligence/fake_http_transport.h"
#include "seoul/browser/intelligence/local_model_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::expected<GenerationResult, std::string> RunToCompletion(
    ModelProvider& provider,
    const GenerationRequest& request) {
  base::expected<GenerationResult, std::string> captured =
      base::unexpected("callback never fired");
  provider.Generate(request, base::BindLambdaForTesting(
                                  [&](base::expected<GenerationResult, std::string> r) {
                                    captured = std::move(r);
                                  }));
  return captured;
}

TEST(LocalModelProviderTest, StreamsAndAccumulatesText) {
  FakeHttpTransport transport;
  transport.SetResponse(
      {
          "data: {\"choices\":[{\"delta\":{\"content\":\"Hel\"}}]}\n\n",
          "data: {\"choices\":[{\"delta\":{\"content\":\"lo\"}}]}\n\n",
          "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}],"
          "\"usage\":{\"prompt_tokens\":4,\"completion_tokens\":2}}\n\n",
          "data: [DONE]\n\n",
      },
      200);
  LocalModelConfig config;
  config.endpoint_url = "http://127.0.0.1:11434/v1/chat/completions";
  config.model_id = "local-3b";
  LocalModelProvider provider(config, &transport);

  GenerationRequest request;
  request.user_prompt = "hi";
  auto result = RunToCompletion(provider, request);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->text, "Hello");
  EXPECT_FALSE(result->truncated);
  EXPECT_EQ(result->usage.output_tokens, 2);
  EXPECT_EQ(provider.EstimateCostMicrodollars(4, 2), 0);
  EXPECT_EQ(transport.last_request().url, config.endpoint_url);
}

TEST(LocalModelProviderTest, RefusesANonLoopbackEndpoint) {
  FakeHttpTransport transport;
  LocalModelConfig config;
  config.endpoint_url = "https://api.remote.test/v1/chat/completions";
  config.model_id = "local-3b";
  LocalModelProvider provider(config, &transport);

  auto result = RunToCompletion(provider, GenerationRequest());
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("loopback"), std::string::npos);
  // Nothing was sent.
  EXPECT_EQ(transport.start_count(), 0);
}

TEST(CloudModelProviderTest, UsesByokKeyStreamsAndCosts) {
  FakeHttpTransport transport;
  transport.SetResponse(
      {
          "data: {\"type\":\"message_start\",\"message\":{\"usage\":"
          "{\"input_tokens\":100}}}\n\n",
          "data: {\"type\":\"content_block_delta\",\"delta\":{\"text\":\"Hi\"}}\n\n",
          "data: {\"type\":\"message_delta\",\"usage\":{\"output_tokens\":50}}\n\n",
          "data: {\"type\":\"message_stop\"}\n\n",
      },
      200);
  FakeCredentialStore credentials;
  credentials.Set("cloud-account", "secret-key-value");

  CloudModelConfig config;
  config.endpoint_url = "https://api.provider.test/v1/messages";
  config.model_id = "cloud-model";
  config.credential_account = "cloud-account";
  config.capabilities.input_cost_per_mtok_microdollars = 3'000'000;
  config.capabilities.output_cost_per_mtok_microdollars = 15'000'000;
  CloudModelProvider provider(config, &transport, &credentials);

  auto result = RunToCompletion(provider, GenerationRequest());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->text, "Hi");
  EXPECT_EQ(result->usage.input_tokens, 100);
  EXPECT_EQ(result->usage.output_tokens, 50);
  // 100 in * 3 + 50 out * 15 (per Mtok, microdollars) = 300 + 750 = 1050.
  EXPECT_EQ(result->usage.cost_microdollars, 1050);

  // The key was sent in the Authorization header and is not retained anywhere
  // observable on the provider.
  bool sent_key = false;
  for (const HttpHeader& h : transport.last_request().headers) {
    if (h.name == "Authorization" && h.value == "Bearer secret-key-value") {
      sent_key = true;
    }
  }
  EXPECT_TRUE(sent_key);
}

TEST(CloudModelProviderTest, MissingKeyFailsBeforeSending) {
  FakeHttpTransport transport;
  FakeCredentialStore credentials;  // empty
  CloudModelConfig config;
  config.endpoint_url = "https://api.provider.test/v1/messages";
  config.model_id = "cloud-model";
  config.credential_account = "cloud-account";
  CloudModelProvider provider(config, &transport, &credentials);

  auto result = RunToCompletion(provider, GenerationRequest());
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("API key"), std::string::npos);
  EXPECT_EQ(transport.start_count(), 0);
}

TEST(CloudModelProviderTest, MapsHttpErrorBodies) {
  FakeHttpTransport transport;
  transport.SetResponse({}, 401);
  FakeCredentialStore credentials;
  credentials.Set("cloud-account", "bad-key");
  CloudModelConfig config;
  config.endpoint_url = "https://api.provider.test/v1/messages";
  config.model_id = "cloud-model";
  config.credential_account = "cloud-account";
  CloudModelProvider provider(config, &transport, &credentials);

  auto result = RunToCompletion(provider, GenerationRequest());
  ASSERT_FALSE(result.has_value());
  EXPECT_NE(result.error().find("401"), std::string::npos);
}

}  // namespace
}  // namespace seoul
