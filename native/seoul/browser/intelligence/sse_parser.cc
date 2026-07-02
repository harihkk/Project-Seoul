// Project Seoul hybrid intelligence.

#include "seoul/browser/intelligence/sse_parser.h"

#include "base/strings/string_util.h"

namespace seoul {

SseParser::SseParser() = default;
SseParser::~SseParser() = default;

void SseParser::ConsumeLine(std::string_view line, std::vector<SseEvent>* out) {
  // Strip an optional trailing CR (CRLF streams).
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  if (line.empty()) {
    // Blank line: dispatch the accumulated event, if any.
    if (has_data_) {
      SseEvent event;
      event.data = data_accum_;
      event.done = data_accum_ == "[DONE]";
      out->push_back(std::move(event));
      data_accum_.clear();
      has_data_ = false;
    }
    return;
  }
  if (line[0] == ':') {
    return;  // comment line
  }
  std::string_view field = line;
  std::string_view value;
  const size_t colon = line.find(':');
  if (colon != std::string_view::npos) {
    field = line.substr(0, colon);
    value = line.substr(colon + 1);
    if (!value.empty() && value.front() == ' ') {
      value.remove_prefix(1);
    }
  }
  if (field != "data") {
    return;  // only the data field is meaningful to our providers
  }
  // A lone "[DONE]" is dispatched immediately (many APIs send it without a
  // following blank line).
  if (value == "[DONE]") {
    SseEvent event;
    event.data = "[DONE]";
    event.done = true;
    out->push_back(std::move(event));
    data_accum_.clear();
    has_data_ = false;
    return;
  }
  if (has_data_) {
    data_accum_.push_back('\n');
  }
  data_accum_.append(value);
  has_data_ = true;
}

std::vector<SseEvent> SseParser::Feed(std::string_view chunk) {
  std::vector<SseEvent> events;
  if (overflowed_) {
    return events;
  }
  if (buffer_.size() + chunk.size() > kMaxSseBufferBytes) {
    overflowed_ = true;
    buffer_.clear();
    return events;
  }
  buffer_.append(chunk);

  size_t start = 0;
  while (true) {
    const size_t newline = buffer_.find('\n', start);
    if (newline == std::string::npos) {
      break;
    }
    ConsumeLine(std::string_view(buffer_).substr(start, newline - start),
                &events);
    start = newline + 1;
  }
  buffer_.erase(0, start);
  return events;
}

std::vector<SseEvent> SseParser::Flush() {
  std::vector<SseEvent> events;
  if (overflowed_) {
    return events;
  }
  if (!buffer_.empty()) {
    ConsumeLine(buffer_, &events);
    buffer_.clear();
  }
  // Dispatch any trailing accumulated data with no final blank line.
  if (has_data_) {
    SseEvent event;
    event.data = data_accum_;
    event.done = data_accum_ == "[DONE]";
    events.push_back(std::move(event));
    data_accum_.clear();
    has_data_ = false;
  }
  return events;
}

}  // namespace seoul
