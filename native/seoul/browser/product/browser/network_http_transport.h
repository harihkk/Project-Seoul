// Project Seoul product runtime - Chromium glue.
// The concrete HttpTransport over Chromium's network service: streaming
// responses (SSE included) through SimpleURLLoader's DownloadAsStream,
// per-request cancellation, timeouts, bounded response sizes, HTTP status
// mapping, and profile-inherited proxy/certificate behavior (no bypass of
// certificate errors, ever). An optional loopback-only mode rejects any
// non-loopback URL before a socket is opened; the local reasoning provider
// gets a transport constructed in that mode.

#ifndef SEOUL_BROWSER_PRODUCT_BROWSER_NETWORK_HTTP_TRANSPORT_H_
#define SEOUL_BROWSER_PRODUCT_BROWSER_NETWORK_HTTP_TRANSPORT_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "seoul/browser/intelligence/http_transport.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace seoul {

class NetworkHttpTransport : public HttpTransport {
 public:
  enum class Mode {
    kGeneral,       // any http(s) URL
    kLoopbackOnly,  // rejects anything that is not a loopback host
  };

  NetworkHttpTransport(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Mode mode);
  NetworkHttpTransport(const NetworkHttpTransport&) = delete;
  NetworkHttpTransport& operator=(const NetworkHttpTransport&) = delete;
  ~NetworkHttpTransport() override;

  // HttpTransport:
  int Start(const HttpRequest& request, HttpStreamCallbacks callbacks) override;
  void Cancel(int request_handle) override;

  size_t active_request_count_for_testing() const { return requests_.size(); }

 private:
  class StreamReader;

  void OnRequestDone(int handle, int http_status, std::string transport_error);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const Mode mode_;
  int next_handle_ = 1;
  std::map<int, std::unique_ptr<StreamReader>> requests_;
  base::WeakPtrFactory<NetworkHttpTransport> weak_factory_{this};
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_BROWSER_NETWORK_HTTP_TRANSPORT_H_
