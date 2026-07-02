// Project Seoul hybrid intelligence.
// Unit tests for the incremental SSE parser: chunk-split reassembly, the
// [DONE] sentinel, CRLF handling, comments, and the overflow guard.

#include "seoul/browser/intelligence/sse_parser.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(SseParserTest, ReassemblesEventsSplitAcrossChunks) {
  SseParser parser;
  auto e1 = parser.Feed("data: hel");
  EXPECT_TRUE(e1.empty());  // no newline yet
  auto e2 = parser.Feed("lo\n\n");
  ASSERT_EQ(e2.size(), 1u);
  EXPECT_EQ(e2[0].data, "hello");
  EXPECT_FALSE(e2[0].done);
}

TEST(SseParserTest, HandlesCrlfAndComments) {
  SseParser parser;
  auto events = parser.Feed(": keep-alive\r\ndata: one\r\n\r\ndata: two\r\n\r\n");
  ASSERT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0].data, "one");
  EXPECT_EQ(events[1].data, "two");
}

TEST(SseParserTest, DoneSentinelIsReported) {
  SseParser parser;
  auto events = parser.Feed("data: [DONE]\n\n");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_TRUE(events[0].done);
  EXPECT_EQ(events[0].data, "[DONE]");
}

TEST(SseParserTest, MultiLineDataConcatenates) {
  SseParser parser;
  auto events = parser.Feed("data: a\ndata: b\n\n");
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events[0].data, "a\nb");
}

TEST(SseParserTest, FlushEmitsTrailingEventWithoutBlankLine) {
  SseParser parser;
  auto during = parser.Feed("data: tail");
  EXPECT_TRUE(during.empty());
  auto flushed = parser.Flush();
  ASSERT_EQ(flushed.size(), 1u);
  EXPECT_EQ(flushed[0].data, "tail");
}

TEST(SseParserTest, OverflowGuardStopsUnboundedGrowth) {
  SseParser parser;
  // A hostile stream with no newline must not grow the buffer without bound.
  const std::string huge(kMaxSseBufferBytes + 1, 'x');
  auto events = parser.Feed(huge);
  EXPECT_TRUE(events.empty());
  EXPECT_TRUE(parser.overflowed());
  // Once overflowed, further feeds are inert.
  EXPECT_TRUE(parser.Feed("data: x\n\n").empty());
}

}  // namespace
}  // namespace seoul
