// Project Seoul product runtime - realtime voice agent.
//
// Owns the realtime session contract for Seoul Voice. The standard API
// key stays in the injected CredentialStore; Canvas receives only the
// short-lived client secret returned by /v1/realtime/client_secrets.

#ifndef SEOUL_BROWSER_PRODUCT_REALTIME_VOICE_AGENT_H_
#define SEOUL_BROWSER_PRODUCT_REALTIME_VOICE_AGENT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/intelligence/credential_store.h"
#include "seoul/browser/intelligence/http_transport.h"

namespace seoul {

inline constexpr char kRealtimeVoiceCredentialAccount[] = "voice_realtime";
inline constexpr char kSeoulRealtimeVoiceApiModel[] = "gpt-realtime-2.1";
inline constexpr char kSeoulRealtimeVoiceProductTarget[] = "gpt-live-1";
inline constexpr char kSeoulRealtimeVoiceToolName[] = "seoul_browser_task";

struct RealtimeVoiceAgentSnapshot {
  bool configured = false;
  bool creating_session = false;
  std::string api_model = kSeoulRealtimeVoiceApiModel;
  std::string product_target = kSeoulRealtimeVoiceProductTarget;
  std::string route = "cloud";
  std::string last_error;
};

class RealtimeVoiceAgent {
 public:
  using CreateSessionResult = base::expected<base::DictValue, std::string>;
  using CreateSessionCallback = base::OnceCallback<void(CreateSessionResult)>;

  RealtimeVoiceAgent(HttpTransport* transport, CredentialStore* credentials);
  RealtimeVoiceAgent(const RealtimeVoiceAgent&) = delete;
  RealtimeVoiceAgent& operator=(const RealtimeVoiceAgent&) = delete;
  ~RealtimeVoiceAgent();

  void CreateSession(const std::string& safety_identifier,
                     CreateSessionCallback callback);
  void Cancel();

  RealtimeVoiceAgentSnapshot Snapshot() const;

  base::DictValue BuildSessionConfigForTesting() const;
  std::string InstructionsForTesting() const;

 private:
  base::DictValue BuildSessionConfig() const;
  base::ListValue BuildToolDefinitions() const;
  std::string BuildInstructions() const;
  void OnCreateSessionResponse(CreateSessionCallback callback,
                               std::unique_ptr<std::string> body,
                               int http_status,
                               const std::string& transport_error);

  raw_ptr<HttpTransport> transport_;
  raw_ptr<CredentialStore> credentials_;
  int active_handle_ = 0;
  bool creating_session_ = false;
  std::string last_error_;
  base::WeakPtrFactory<RealtimeVoiceAgent> weak_factory_{this};
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_REALTIME_VOICE_AGENT_H_
