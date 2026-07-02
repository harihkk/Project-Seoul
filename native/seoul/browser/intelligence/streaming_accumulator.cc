// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/streaming_accumulator.h"

#include <utility>

namespace seoul {

StreamingAccumulator::StreamingAccumulator(PayloadParser parser)
    : parser_(std::move(parser)) {}

StreamingAccumulator::~StreamingAccumulator() = default;

bool StreamingAccumulator::Feed(std::string_view chunk) {
  for (const SseEvent& event : sse_.Feed(chunk)) {
    if (event.done) {
      saw_stop_ = true;
      continue;
    }
    auto delta = parser_.Run(event.data);
    if (!delta.has_value()) {
      // Tolerate a malformed payload only before any content arrives (some
      // servers emit non-JSON keep-alive comments); once producing content, a
      // malformed payload is a hard error.
      if (!text_.empty()) {
        error_ = delta.error();
        return false;
      }
      continue;
    }
    text_.append(delta->text);
    if (delta->input_tokens > 0) input_tokens_ = delta->input_tokens;
    if (delta->output_tokens > 0) output_tokens_ = delta->output_tokens;
    if (delta->stop) saw_stop_ = true;
  }
  if (sse_.overflowed()) {
    error_ = "response stream exceeded the maximum buffered size";
    return false;
  }
  return true;
}

GenerationResult StreamingAccumulator::Finish() {
  for (const SseEvent& event : sse_.Flush()) {
    if (event.done) {
      saw_stop_ = true;
      continue;
    }
    auto delta = parser_.Run(event.data);
    if (delta.has_value()) {
      text_.append(delta->text);
      if (delta->output_tokens > 0) output_tokens_ = delta->output_tokens;
      if (delta->stop) saw_stop_ = true;
    }
  }
  GenerationResult result;
  result.text = text_;
  result.usage.input_tokens = input_tokens_;
  result.usage.output_tokens = output_tokens_;
  result.truncated = !saw_stop_;
  return result;
}

}  // namespace seoul
