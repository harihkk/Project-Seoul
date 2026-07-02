// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/provider_protocol.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace seoul {

bool IsLocalOnlyEndpoint(const std::string& url) {
  GURL parsed{url};
  if (!parsed.is_valid() || !parsed.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  const std::string host = parsed.host();
  if (host == "localhost" || host == "127.0.0.1" || host == "[::1]" ||
      host == "::1") {
    return true;
  }
  // 127.0.0.0/8 loopback range.
  if (base::StartsWith(host, "127.")) {
    return true;
  }
  return false;
}

namespace {

// Builds the messages array shared by both request shapes: a system turn (when
// present) is handled by the caller; here we emit a single user turn.
base::Value::List UserMessages(const std::string& user_prompt) {
  base::Value::List messages;
  base::Value::Dict user;
  user.Set("role", "user");
  user.Set("content", user_prompt);
  messages.Append(std::move(user));
  return messages;
}

}  // namespace

namespace chat_completions {

std::string BuildRequestBody(const std::string& model,
                             const GenerationRequest& request,
                             bool stream) {
  base::Value::Dict body;
  body.Set("model", model);
  body.Set("stream", stream);
  body.Set("temperature", request.temperature);
  body.Set("max_tokens", request.max_output_tokens);
  base::Value::List messages;
  if (!request.system_prompt.empty()) {
    base::Value::Dict system;
    system.Set("role", "system");
    system.Set("content", request.system_prompt);
    messages.Append(std::move(system));
  }
  base::Value::List user = UserMessages(request.user_prompt);
  for (base::Value& turn : user) {
    messages.Append(std::move(turn));
  }
  body.Set("messages", std::move(messages));
  // Structured output: a JSON-schema response format when a schema was given.
  if (!request.response_schema.empty()) {
    base::Value::Dict format;
    format.Set("type", "json_schema");
    base::Value::Dict schema;
    schema.Set("name", "seoul_structured");
    schema.Set("schema", request.response_schema.Clone());
    format.Set("json_schema", std::move(schema));
    body.Set("response_format", std::move(format));
  }
  std::string out;
  base::JSONWriter::Write(body, &out);
  return out;
}

base::expected<ProviderDelta, std::string> ParseStreamPayload(
    const std::string& payload) {
  std::optional<base::Value> parsed = base::JSONReader::Read(payload);
  if (!parsed || !parsed->is_dict()) {
    return base::unexpected("malformed chat.completion chunk");
  }
  const base::Value::Dict& dict = parsed->GetDict();
  ProviderDelta delta;
  const base::Value::List* choices = dict.FindList("choices");
  if (choices && !choices->empty()) {
    const base::Value::Dict* choice = choices->front().GetIfDict();
    if (choice) {
      if (const base::Value::Dict* d = choice->FindDict("delta")) {
        if (const std::string* content = d->FindString("content")) {
          delta.text = *content;
        }
      }
      if (const std::string* finish = choice->FindString("finish_reason")) {
        delta.stop = !finish->empty();
      }
    }
  }
  if (const base::Value::Dict* usage = dict.FindDict("usage")) {
    delta.input_tokens = usage->FindInt("prompt_tokens").value_or(0);
    delta.output_tokens = usage->FindInt("completion_tokens").value_or(0);
  }
  return delta;
}

}  // namespace chat_completions

namespace messages {

std::string BuildRequestBody(const std::string& model,
                             const GenerationRequest& request,
                             bool stream) {
  base::Value::Dict body;
  body.Set("model", model);
  body.Set("stream", stream);
  body.Set("max_tokens", request.max_output_tokens);
  body.Set("temperature", request.temperature);
  if (!request.system_prompt.empty()) {
    // Messages-style keeps the system prompt as a top-level field.
    body.Set("system", request.system_prompt);
  }
  body.Set("messages", UserMessages(request.user_prompt));
  std::string out;
  base::JSONWriter::Write(body, &out);
  return out;
}

base::expected<ProviderDelta, std::string> ParseStreamPayload(
    const std::string& payload) {
  std::optional<base::Value> parsed = base::JSONReader::Read(payload);
  if (!parsed || !parsed->is_dict()) {
    return base::unexpected("malformed messages event");
  }
  const base::Value::Dict& dict = parsed->GetDict();
  ProviderDelta delta;
  const std::string* type = dict.FindString("type");
  if (!type) {
    return base::unexpected("messages event missing type");
  }
  if (*type == "content_block_delta") {
    if (const base::Value::Dict* d = dict.FindDict("delta")) {
      if (const std::string* text = d->FindString("text")) {
        delta.text = *text;
      }
    }
  } else if (*type == "message_delta") {
    if (const base::Value::Dict* usage = dict.FindDict("usage")) {
      delta.output_tokens = usage->FindInt("output_tokens").value_or(0);
    }
  } else if (*type == "message_start") {
    if (const base::Value::Dict* message = dict.FindDict("message")) {
      if (const base::Value::Dict* usage = message->FindDict("usage")) {
        delta.input_tokens = usage->FindInt("input_tokens").value_or(0);
      }
    }
  } else if (*type == "message_stop") {
    delta.stop = true;
  }
  return delta;
}

std::string MapErrorResponse(int http_status, const std::string& body) {
  // Surface the provider's error type/message when present, never the raw
  // headers or any credential. Bounded to a short summary.
  std::optional<base::Value> parsed = base::JSONReader::Read(body);
  if (parsed && parsed->is_dict()) {
    if (const base::Value::Dict* error = parsed->GetDict().FindDict("error")) {
      const std::string* type = error->FindString("type");
      const std::string* message = error->FindString("message");
      std::string summary;
      if (type) {
        summary += *type;
      }
      if (message) {
        if (!summary.empty()) {
          summary += ": ";
        }
        summary += message->substr(0, 200);
      }
      if (!summary.empty()) {
        return summary;
      }
    }
  }
  return "provider returned HTTP " + base::NumberToString(http_status);
}

}  // namespace messages

}  // namespace seoul
