// Project Seoul product runtime - Chromium glue.

#include "seoul/browser/product/browser/network_http_transport.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace seoul {

namespace {

// Streaming bodies are consumed incrementally; this bounds the total bytes a
// single response may deliver so a runaway stream cannot grow memory.
constexpr size_t kAbsoluteMaxResponseBytes = 32 * 1024 * 1024;
constexpr size_t kMaxRequestBodyBytes = 32 * 1024 * 1024;

// Requests without server activity fail after this long. Streaming responses
// reset the clock on every chunk via SimpleURLLoader's own timeout handling.
constexpr base::TimeDelta kRequestTimeout = base::Seconds(120);

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("seoul_product_runtime", R"(
      semantics {
        sender: "Seoul product runtime"
        description:
          "Requests made on the user's behalf by Seoul: reasoning provider "
          "calls (local loopback runtime or the user's configured cloud "
          "provider) and user-configured connector capabilities (OpenAPI/"
          "MCP endpoints)."
        trigger: "An explicit user request, task step, or health check."
        data:
          "The user's goal text, capability arguments, and provider request "
          "bodies. Never credentials in URLs; secrets travel only in "
          "authorization headers read from the OS keychain."
        destination: OTHER
      }
      policy {
        cookies_allowed: NO
        setting: "Providers and connectors are configured in Seoul Studio."
        policy_exception_justification:
          "User-configured endpoints; disabled entirely when none are set."
      })");

}  // namespace

// One in-progress request: owns the loader and streams chunks to the callback.
class NetworkHttpTransport::StreamReader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  StreamReader(std::unique_ptr<network::SimpleURLLoader> loader,
               size_t max_response_bytes,
               HttpStreamCallbacks callbacks,
               base::OnceCallback<void(int, std::string)> done)
      : loader_(std::move(loader)),
        max_response_bytes_(max_response_bytes),
        callbacks_(std::move(callbacks)),
        done_(std::move(done)) {}

  ~StreamReader() override = default;

  void Start(network::SharedURLLoaderFactory* factory) {
    loader_->DownloadAsStream(factory, this);
  }

  // network::SimpleURLLoaderStreamConsumer:
  void OnDataReceived(std::string_view piece,
                      base::OnceClosure resume) override {
    received_ += piece.size();
    if (received_ > max_response_bytes_) {
      // Abandon the oversized stream; OnComplete reports the failure.
      loader_.reset();
      if (done_) {
        std::move(done_).Run(0, "response exceeded the size bound");
      }
      return;
    }
    if (callbacks_.on_chunk) {
      callbacks_.on_chunk.Run(piece);
    }
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    int status = 0;
    if (loader_ && loader_->ResponseInfo() &&
        loader_->ResponseInfo()->headers) {
      status = loader_->ResponseInfo()->headers->response_code();
    }
    std::string error;
    if (!success && status == 0) {
      error = loader_ ? net::ErrorToString(loader_->NetError())
                      : std::string("request aborted");
    }
    if (done_) {
      std::move(done_).Run(status, std::move(error));
    }
  }

  void OnRetry(base::OnceClosure start_retry) override {
    // Retries are configured per request policy at the SimpleURLLoader level;
    // when unset this is never called. Resume immediately when it is.
    std::move(start_retry).Run();
  }

  HttpStreamCallbacks& callbacks() { return callbacks_; }

 private:
  std::unique_ptr<network::SimpleURLLoader> loader_;
  const size_t max_response_bytes_;
  HttpStreamCallbacks callbacks_;
  base::OnceCallback<void(int, std::string)> done_;
  size_t received_ = 0;
};

NetworkHttpTransport::NetworkHttpTransport(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Mode mode)
    : url_loader_factory_(std::move(url_loader_factory)), mode_(mode) {}

NetworkHttpTransport::~NetworkHttpTransport() = default;

int NetworkHttpTransport::Start(const HttpRequest& request,
                                HttpStreamCallbacks callbacks) {
  const GURL url(request.url);
  const bool scheme_ok = url.is_valid() && url.SchemeIsHTTPOrHTTPS();
  const bool loopback_ok =
      mode_ != Mode::kLoopbackOnly || (url.is_valid() && net::IsLocalhost(url));
  const bool method_ok = net::HttpUtil::IsValidHeaderName(request.method);
  const bool body_ok = request.body.size() <= kMaxRequestBodyBytes;
  bool headers_ok = true;
  for (const HttpHeader& header : request.headers) {
    if (!net::HttpUtil::IsValidHeaderName(header.name) ||
        !net::HttpUtil::IsValidHeaderValue(header.value)) {
      headers_ok = false;
      break;
    }
  }
  if (!scheme_ok || !loopback_ok || !method_ok || !headers_ok || !body_ok ||
      !url_loader_factory_) {
    // Reject before any socket exists; the completion still fires exactly
    // once so callers have one code path.
    if (callbacks.on_complete) {
      std::string error = "invalid request";
      if (!loopback_ok) {
        error = "endpoint is not a loopback address";
      } else if (!scheme_ok) {
        error = "invalid request URL";
      } else if (!method_ok) {
        error = "invalid request method";
      } else if (!headers_ok) {
        error = "invalid request header";
      } else if (!body_ok) {
        error = "request body exceeded the size bound";
      } else if (!url_loader_factory_) {
        error = "network service is unavailable";
      }
      std::move(callbacks.on_complete).Run(0, std::move(error));
    }
    return 0;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = request.method;
  // Provider/connector calls are credentialless: no cookies, ever.
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  for (const HttpHeader& header : request.headers) {
    resource_request->headers.SetHeader(header.name, header.value);
  }
  if (request.expect_event_stream) {
    resource_request->headers.SetHeader("Accept", "text/event-stream");
  }

  std::unique_ptr<network::SimpleURLLoader> loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kTrafficAnnotation);
  if (!request.body.empty()) {
    loader->AttachStringForUpload(request.body, "application/json");
  }
  loader->SetTimeoutDuration(kRequestTimeout);
  // Error bodies carry provider error JSON; deliver them instead of dropping.
  loader->SetAllowHttpErrorResults(true);

  const int handle = next_handle_++;
  const size_t max_response_bytes =
      request.max_response_bytes == 0
          ? kAbsoluteMaxResponseBytes
          : std::min(request.max_response_bytes, kAbsoluteMaxResponseBytes);
  auto reader = std::make_unique<StreamReader>(
      std::move(loader), max_response_bytes, std::move(callbacks),
      base::BindOnce(&NetworkHttpTransport::OnRequestDone,
                     weak_factory_.GetWeakPtr(), handle));
  StreamReader* raw = reader.get();
  requests_[handle] = std::move(reader);
  raw->Start(url_loader_factory_.get());
  return handle;
}

void NetworkHttpTransport::Cancel(int request_handle) {
  // Dropping the reader destroys the loader, which aborts the request; no
  // completion callback fires after an explicit cancel.
  requests_.erase(request_handle);
}

void NetworkHttpTransport::OnRequestDone(int handle,
                                         int http_status,
                                         std::string transport_error) {
  auto it = requests_.find(handle);
  if (it == requests_.end()) {
    return;
  }
  HttpStreamCallbacks callbacks = std::move(it->second->callbacks());
  requests_.erase(it);
  if (callbacks.on_complete) {
    std::move(callbacks.on_complete)
        .Run(http_status, std::move(transport_error));
  }
}

}  // namespace seoul
