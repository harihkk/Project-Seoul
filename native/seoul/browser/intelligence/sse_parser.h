// Project Seoul hybrid intelligence.
// A pure incremental Server-Sent-Events parser. Providers stream raw response
// bytes into Feed() and receive complete "data:" payloads as events; the
// sentinel payload "[DONE]" is reported as done. No I/O, no allocation beyond
// the pending line buffer, bounded so a hostile stream cannot grow it without
// limit.

#ifndef SEOUL_BROWSER_INTELLIGENCE_SSE_PARSER_H_
#define SEOUL_BROWSER_INTELLIGENCE_SSE_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

namespace seoul {

inline constexpr size_t kMaxSseBufferBytes = 1024 * 1024;  // 1 MiB pending cap

struct SseEvent {
  std::string data;  // the concatenated data payload of one event
  bool done = false;  // the payload was the "[DONE]" sentinel
};

class SseParser {
 public:
  SseParser();
  ~SseParser();

  // Feeds a chunk and returns any complete events it produced. An event is
  // emitted on a blank line (SSE dispatch) or when a lone "data: [DONE]" line
  // arrives. `overflowed()` becomes true if the pending buffer exceeds the cap;
  // the caller should abort the stream.
  std::vector<SseEvent> Feed(std::string_view chunk);

  // Flushes a trailing event if the stream ended without a final blank line.
  std::vector<SseEvent> Flush();

  bool overflowed() const { return overflowed_; }

 private:
  void ConsumeLine(std::string_view line, std::vector<SseEvent>* out);

  std::string buffer_;       // bytes not yet split into lines
  std::string data_accum_;   // data lines for the current event
  bool has_data_ = false;
  bool overflowed_ = false;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_INTELLIGENCE_SSE_PARSER_H_
