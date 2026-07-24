// Project Seoul hybrid intelligence.
// HTTP transport seam for model providers. Concrete implementations wrap
// Chromium's network stack (a later capable-host glue target); the provider
// logic, request building, streaming accumulation, and error mapping are pure
// and depend only on this seam, so they are fully testable with a fake.

#ifndef SEOUL_BROWSER_INTELLIGENCE_HTTP_TRANSPORT_H_
#define SEOUL_BROWSER_INTELLIGENCE_HTTP_TRANSPORT_H_

#include <cstddef>
#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace seoul {

struct HttpHeader {
  HttpHeader();
  // Two-field constructor so existing brace-initialized call sites
  // (`headers.push_back({name, value})`) keep working now that the struct is
  // no longer an aggregate.
  HttpHeader(std::string name, std::string value);
  HttpHeader(const HttpHeader&);
  HttpHeader(HttpHeader&&);
  HttpHeader& operator=(const HttpHeader&);
  HttpHeader& operator=(HttpHeader&&);
  ~HttpHeader();

  std::string name;
  std::string value;
};

struct HttpRequest {
  HttpRequest();
  HttpRequest(const HttpRequest&);
  HttpRequest(HttpRequest&&);
  HttpRequest& operator=(const HttpRequest&);
  HttpRequest& operator=(HttpRequest&&);
  ~HttpRequest();

  std::string method = "POST";
  std::string url;
  std::vector<HttpHeader> headers;
  std::string body;  // JSON request body
  bool expect_event_stream = false;  // server-sent events response
  // Callers can select a tighter response limit for small control-plane
  // requests. The concrete transport still enforces its absolute ceiling.
  size_t max_response_bytes = 32 * 1024 * 1024;
};

// Streaming callbacks. `on_chunk` delivers raw response bytes as they arrive
// (the provider feeds them to an SseParser); `on_complete` fires once with the
// final HTTP status and any transport error. A transport never interprets the
// body; it moves bytes.
// Move-only: base::OnceCallback is move-only, so the copy operations stay
// implicitly deleted.
struct HttpStreamCallbacks {
  HttpStreamCallbacks();
  HttpStreamCallbacks(HttpStreamCallbacks&&);
  HttpStreamCallbacks& operator=(HttpStreamCallbacks&&);
  ~HttpStreamCallbacks();

  base::RepeatingCallback<void(std::string_view chunk)> on_chunk;
  base::OnceCallback<void(int http_status, const std::string& transport_error)>
      on_complete;
};

class HttpTransport {
 public:
  virtual ~HttpTransport() = default;
  // Starts a request. The implementation must reject a request whose URL is
  // not permitted for its role (the local provider passes a loopback-only
  // transport). Returns an opaque request handle usable with Cancel.
  virtual int Start(const HttpRequest& request,
                    HttpStreamCallbacks callbacks) = 0;
  virtual void Cancel(int request_handle) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_HTTP_TRANSPORT_H_
