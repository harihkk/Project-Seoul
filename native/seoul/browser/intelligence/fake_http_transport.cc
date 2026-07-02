// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/fake_http_transport.h"

#include <utility>

namespace seoul {

FakeHttpTransport::FakeHttpTransport() = default;
FakeHttpTransport::~FakeHttpTransport() = default;

void FakeHttpTransport::SetResponse(std::vector<std::string> chunks,
                                    int status,
                                    std::string transport_error) {
  chunks_ = std::move(chunks);
  status_ = status;
  transport_error_ = std::move(transport_error);
}

int FakeHttpTransport::Start(const HttpRequest& request,
                             HttpStreamCallbacks callbacks) {
  ++start_count_;
  last_request_ = request;
  const int handle = next_handle_++;
  // Synchronous scripted delivery: chunks in order, then completion. Providers
  // treat delivery as asynchronous, so a synchronous fake is a valid drive.
  for (const std::string& chunk : chunks_) {
    if (callbacks.on_chunk) {
      callbacks.on_chunk.Run(chunk);
    }
  }
  if (callbacks.on_complete) {
    std::move(callbacks.on_complete).Run(status_, transport_error_);
  }
  return handle;
}

void FakeHttpTransport::Cancel(int request_handle) {
  ++cancel_count_;
}

FakeCredentialStore::FakeCredentialStore() = default;
FakeCredentialStore::~FakeCredentialStore() = default;

std::optional<std::string> FakeCredentialStore::Get(
    const std::string& account_key) {
  auto it = secrets_.find(account_key);
  if (it == secrets_.end()) return std::nullopt;
  return it->second;
}

bool FakeCredentialStore::Set(const std::string& account_key,
                              const std::string& secret) {
  secrets_[account_key] = secret;
  return true;
}

bool FakeCredentialStore::Delete(const std::string& account_key) {
  return secrets_.erase(account_key) > 0;
}

}  // namespace seoul
