// Project Seoul hybrid intelligence.
// Out-of-line special members for the transport seam's value types.

#include "seoul/browser/intelligence/http_transport.h"

#include <utility>

namespace seoul {

HttpHeader::HttpHeader() = default;
HttpHeader::HttpHeader(std::string name, std::string value)
    : name(std::move(name)), value(std::move(value)) {}
HttpHeader::HttpHeader(const HttpHeader&) = default;
HttpHeader::HttpHeader(HttpHeader&&) = default;
HttpHeader& HttpHeader::operator=(const HttpHeader&) = default;
HttpHeader& HttpHeader::operator=(HttpHeader&&) = default;
HttpHeader::~HttpHeader() = default;

HttpRequest::HttpRequest() = default;
HttpRequest::HttpRequest(const HttpRequest&) = default;
HttpRequest::HttpRequest(HttpRequest&&) = default;
HttpRequest& HttpRequest::operator=(const HttpRequest&) = default;
HttpRequest& HttpRequest::operator=(HttpRequest&&) = default;
HttpRequest::~HttpRequest() = default;

// HttpStreamCallbacks carries a base::OnceCallback: move-only, so only the
// move operations are declared and copies remain implicitly deleted.
HttpStreamCallbacks::HttpStreamCallbacks() = default;
HttpStreamCallbacks::HttpStreamCallbacks(HttpStreamCallbacks&&) = default;
HttpStreamCallbacks& HttpStreamCallbacks::operator=(HttpStreamCallbacks&&) =
    default;
HttpStreamCallbacks::~HttpStreamCallbacks() = default;

}  // namespace seoul
