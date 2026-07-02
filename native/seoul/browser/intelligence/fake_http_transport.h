// Project Seoul hybrid intelligence.
// Deterministic test doubles: a scripted HttpTransport that replays canned SSE
// chunks and a completion status, and an in-memory CredentialStore. Test
// support only; never linked into production.

#ifndef SEOUL_BROWSER_INTELLIGENCE_FAKE_HTTP_TRANSPORT_H_
#define SEOUL_BROWSER_INTELLIGENCE_FAKE_HTTP_TRANSPORT_H_

#include <map>
#include <string>
#include <vector>

#include "seoul/browser/intelligence/credential_store.h"
#include "seoul/browser/intelligence/http_transport.h"

namespace seoul {

class FakeHttpTransport : public HttpTransport {
 public:
  FakeHttpTransport();
  ~FakeHttpTransport() override;

  // Script the next response: chunks are delivered in order, then on_complete
  // fires with `status` and `transport_error`.
  void SetResponse(std::vector<std::string> chunks,
                   int status,
                   std::string transport_error = std::string());

  int Start(const HttpRequest& request, HttpStreamCallbacks callbacks) override;
  void Cancel(int request_handle) override;

  const HttpRequest& last_request() const { return last_request_; }
  int start_count() const { return start_count_; }
  int cancel_count() const { return cancel_count_; }

 private:
  std::vector<std::string> chunks_;
  int status_ = 200;
  std::string transport_error_;
  HttpRequest last_request_;
  int start_count_ = 0;
  int cancel_count_ = 0;
  int next_handle_ = 1;
};

class FakeCredentialStore : public CredentialStore {
 public:
  FakeCredentialStore();
  ~FakeCredentialStore() override;

  std::optional<std::string> Get(const std::string& account_key) override;
  bool Set(const std::string& account_key, const std::string& secret) override;
  bool Delete(const std::string& account_key) override;

 private:
  std::map<std::string, std::string> secrets_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_FAKE_HTTP_TRANSPORT_H_
