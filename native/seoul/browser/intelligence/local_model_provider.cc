// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/local_model_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "seoul/browser/intelligence/provider_protocol.h"

namespace seoul {

LocalModelConfig::LocalModelConfig() = default;
LocalModelConfig::LocalModelConfig(const LocalModelConfig&) = default;
LocalModelConfig::LocalModelConfig(LocalModelConfig&&) = default;
LocalModelConfig& LocalModelConfig::operator=(const LocalModelConfig&) =
    default;
LocalModelConfig& LocalModelConfig::operator=(LocalModelConfig&&) = default;
LocalModelConfig::~LocalModelConfig() = default;

LocalModelProvider::LocalModelProvider(LocalModelConfig config,
                                       HttpTransport* transport)
    : config_(std::move(config)), transport_(transport) {
  config_.capabilities.locality = ModelLocality::kLocal;
}

LocalModelProvider::~LocalModelProvider() = default;

std::string LocalModelProvider::provider_id() const {
  return "local:" + config_.model_id;
}

ModelCapabilities LocalModelProvider::capabilities() const {
  return config_.capabilities;
}

void LocalModelProvider::Generate(const GenerationRequest& request,
                                  GenerateCallback callback) {
  // Hard local-only guard: refuse before any bytes leave if the endpoint is
  // not loopback. This is what makes "local" trustworthy.
  if (!IsLocalOnlyEndpoint(config_.endpoint_url)) {
    std::move(callback).Run(
        base::unexpected("local provider endpoint is not a loopback address"));
    return;
  }
  if (!transport_) {
    std::move(callback).Run(base::unexpected("no transport configured"));
    return;
  }

  pending_ = std::move(callback);
  accumulator_ = std::make_unique<StreamingAccumulator>(
      base::BindRepeating(&chat_completions::ParseStreamPayload));

  HttpRequest http;
  http.method = "POST";
  http.url = config_.endpoint_url;
  http.expect_event_stream = true;
  http.headers.push_back({"Content-Type", "application/json"});
  http.body = chat_completions::BuildRequestBody(config_.model_id, request,
                                                 /*stream=*/true);

  HttpStreamCallbacks callbacks;
  callbacks.on_chunk = base::BindRepeating(
      [](StreamingAccumulator* acc, std::string_view chunk) {
        acc->Feed(chunk);
      },
      accumulator_.get());
  callbacks.on_complete = base::BindOnce(
      [](LocalModelProvider* self, int http_status,
         const std::string& transport_error) {
        if (!self->pending_) return;
        if (!transport_error.empty()) {
          std::move(self->pending_).Run(base::unexpected(transport_error));
          return;
        }
        if (http_status < 200 || http_status >= 300) {
          std::move(self->pending_)
              .Run(base::unexpected("local endpoint returned HTTP " +
                                    std::to_string(http_status)));
          return;
        }
        if (self->accumulator_->has_error()) {
          std::move(self->pending_)
              .Run(base::unexpected(self->accumulator_->error()));
          return;
        }
        std::move(self->pending_).Run(self->accumulator_->Finish());
      },
      base::Unretained(this));

  active_handle_ = transport_->Start(http, std::move(callbacks));
}

void LocalModelProvider::Cancel() {
  if (transport_ && active_handle_ != 0) {
    transport_->Cancel(active_handle_);
  }
  pending_.Reset();
}

int64_t LocalModelProvider::EstimateCostMicrodollars(int, int) const {
  return 0;  // local inference has no per-token cost
}

}  // namespace seoul
