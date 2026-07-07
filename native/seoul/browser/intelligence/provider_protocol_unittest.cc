// Project Seoul hybrid intelligence.
// Unit tests for the pure provider request/response protocol and the local-only
// endpoint guard. Fixture payloads are deterministic samples of the two wire
// shapes; no network is involved.

#include "seoul/browser/intelligence/provider_protocol.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(LocalEndpointGuardTest, AcceptsLoopbackRejectsRemote) {
  EXPECT_TRUE(IsLocalOnlyEndpoint("http://localhost:11434/v1/chat/completions"));
  EXPECT_TRUE(IsLocalOnlyEndpoint("http://127.0.0.1:8080/v1"));
  EXPECT_TRUE(IsLocalOnlyEndpoint("http://127.5.5.5/v1"));
  EXPECT_TRUE(IsLocalOnlyEndpoint("http://[::1]:1234/v1"));
  EXPECT_FALSE(IsLocalOnlyEndpoint("https://api.remote.test/v1"));
  EXPECT_FALSE(IsLocalOnlyEndpoint("http://10.0.0.5/v1"));
  EXPECT_FALSE(IsLocalOnlyEndpoint("ftp://localhost/x"));
  EXPECT_FALSE(IsLocalOnlyEndpoint("not-a-url"));
}

TEST(ChatCompletionsTest, BuildsRequestWithSystemAndStructuredOutput) {
  GenerationRequest request;
  request.system_prompt = "be terse";
  request.user_prompt = "hello";
  request.max_output_tokens = 256;
  base::DictValue schema;
  schema.Set("type", "object");
  request.response_schema = std::move(schema);

  const std::string body =
      chat_completions::BuildRequestBody("local-3b", request, /*stream=*/true);
  std::optional<base::Value> parsed = base::JSONReader::Read(body, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed && parsed->is_dict());
  const base::DictValue& dict = parsed->GetDict();
  EXPECT_EQ(*dict.FindString("model"), "local-3b");
  EXPECT_TRUE(dict.FindBool("stream").value_or(false));
  const base::ListValue* messages = dict.FindList("messages");
  ASSERT_TRUE(messages);
  ASSERT_EQ(messages->size(), 2u);  // system + user
  EXPECT_EQ(*(*messages)[0].GetDict().FindString("role"), "system");
  EXPECT_EQ(*(*messages)[1].GetDict().FindString("role"), "user");
  EXPECT_TRUE(dict.FindDict("response_format"));
}

TEST(ChatCompletionsTest, ParsesStreamDeltaAndFinish) {
  auto delta = chat_completions::ParseStreamPayload(
      R"json({"choices":[{"delta":{"content":"Hel"},"finish_reason":null}]})json");
  ASSERT_TRUE(delta.has_value());
  EXPECT_EQ(delta->text, "Hel");
  EXPECT_FALSE(delta->stop);

  auto stop = chat_completions::ParseStreamPayload(
      R"json({"choices":[{"delta":{},"finish_reason":"stop"}],
              "usage":{"prompt_tokens":10,"completion_tokens":3}})json");
  ASSERT_TRUE(stop.has_value());
  EXPECT_TRUE(stop->stop);
  EXPECT_EQ(stop->input_tokens, 10);
  EXPECT_EQ(stop->output_tokens, 3);

  EXPECT_FALSE(chat_completions::ParseStreamPayload("not json").has_value());
}

TEST(MessagesTest, BuildsRequestWithTopLevelSystem) {
  GenerationRequest request;
  request.system_prompt = "be terse";
  request.user_prompt = "hello";
  request.max_output_tokens = 512;
  const std::string body =
      messages::BuildRequestBody("cloud-model", request, /*stream=*/true);
  std::optional<base::Value> parsed = base::JSONReader::Read(body, base::JSON_PARSE_RFC);
  ASSERT_TRUE(parsed && parsed->is_dict());
  const base::DictValue& dict = parsed->GetDict();
  EXPECT_EQ(*dict.FindString("system"), "be terse");
  const base::ListValue* messages_list = dict.FindList("messages");
  ASSERT_TRUE(messages_list);
  ASSERT_EQ(messages_list->size(), 1u);  // only the user turn
  EXPECT_EQ(*(*messages_list)[0].GetDict().FindString("role"), "user");
}

TEST(MessagesTest, ParsesEventStreamShapes) {
  auto start = messages::ParseStreamPayload(
      R"json({"type":"message_start","message":{"usage":{"input_tokens":9}}})json");
  ASSERT_TRUE(start.has_value());
  EXPECT_EQ(start->input_tokens, 9);

  auto block = messages::ParseStreamPayload(
      R"json({"type":"content_block_delta","delta":{"text":"lo"}})json");
  ASSERT_TRUE(block.has_value());
  EXPECT_EQ(block->text, "lo");

  auto usage = messages::ParseStreamPayload(
      R"json({"type":"message_delta","usage":{"output_tokens":5}})json");
  ASSERT_TRUE(usage.has_value());
  EXPECT_EQ(usage->output_tokens, 5);

  auto stop = messages::ParseStreamPayload(R"json({"type":"message_stop"})json");
  ASSERT_TRUE(stop.has_value());
  EXPECT_TRUE(stop->stop);
}

TEST(MessagesTest, MapsErrorWithoutLeakingSecrets) {
  const std::string mapped = messages::MapErrorResponse(
      401,
      R"json({"error":{"type":"authentication_error","message":"invalid key"}})json");
  EXPECT_NE(mapped.find("authentication_error"), std::string::npos);
  EXPECT_NE(mapped.find("invalid key"), std::string::npos);

  const std::string fallback = messages::MapErrorResponse(503, "gateway down");
  EXPECT_NE(fallback.find("503"), std::string::npos);
}

}  // namespace
}  // namespace seoul
