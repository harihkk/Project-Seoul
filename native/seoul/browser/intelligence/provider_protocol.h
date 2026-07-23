// Project Seoul hybrid intelligence.
// Pure request-building and response-parsing for the two provider shapes Seoul
// ships: a local chat-completions-style endpoint and an official cloud
// messages-style endpoint. Kept separate from I/O so both are unit-testable
// without a network. Also holds the local-only endpoint guard, which the local
// provider enforces so on-device reasoning cannot be pointed at a remote host.

#ifndef SEOUL_BROWSER_INTELLIGENCE_PROVIDER_PROTOCOL_H_
#define SEOUL_BROWSER_INTELLIGENCE_PROVIDER_PROTOCOL_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/intelligence/model_provider.h"

namespace seoul {

// True only for a loopback / on-device endpoint: http(s) to localhost,
// 127.0.0.0/8, or [::1]. The local provider rejects anything else so a
// "local" model can never be a remote call in disguise.
bool IsLocalOnlyEndpoint(const std::string& url);

// One streamed delta parsed from a provider event payload.
struct ProviderDelta {
  ProviderDelta();
  ProviderDelta(const ProviderDelta&);
  ProviderDelta(ProviderDelta&&);
  ProviderDelta& operator=(const ProviderDelta&);
  ProviderDelta& operator=(ProviderDelta&&);
  ~ProviderDelta();

  std::string text;       // incremental output text (may be empty)
  bool stop = false;      // the provider signaled end of the message
  int input_tokens = 0;   // usage, when the payload carries it
  int output_tokens = 0;
};

// Chat-completions-style (local) request/response.
namespace chat_completions {

// Builds the JSON request body for a local chat-completions endpoint.
std::string BuildRequestBody(const std::string& model,
                             const GenerationRequest& request,
                             bool stream);

// Parses one SSE data payload (a chat.completion.chunk JSON object) into a
// delta. Returns nullopt for a payload that carries no delta (for example a
// role-only opening chunk).
base::expected<ProviderDelta, std::string> ParseStreamPayload(
    const std::string& payload);

}  // namespace chat_completions

// Messages-style (official cloud) request/response.
namespace messages {

std::string BuildRequestBody(const std::string& model,
                             const GenerationRequest& request,
                             bool stream);

// Parses one messages-style SSE event payload (content_block_delta,
// message_delta with usage, message_stop) into a delta.
base::expected<ProviderDelta, std::string> ParseStreamPayload(
    const std::string& payload);

// Maps a non-2xx cloud response body to a human-readable error, never echoing
// secrets or full headers.
std::string MapErrorResponse(int http_status, const std::string& body);

}  // namespace messages

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_PROVIDER_PROTOCOL_H_
