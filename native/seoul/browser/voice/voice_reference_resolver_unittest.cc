// Project Seoul voice operating layer.
// Unit tests for spoken-reference resolution against stable visible-item ids.

#include "seoul/browser/voice/voice_reference_resolver.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

std::vector<VisibleReferent> SearchResults() {
  return {
      {"r1", "Chromium documentation", "result", 1, false, false},
      {"r2", "GN reference", "result", 2, false, false},
      {"r3", "Ninja manual", "result", 3, false, false},
      {"chart1", "red line", "chart", 1, false, false},
  };
}

TEST(VoiceReferenceResolverTest, DeicticUsesSelectionThenFocus) {
  std::vector<VisibleReferent> referents = SearchResults();
  referents[1].selected = true;
  auto resolved = ResolveVoiceReference("open this", referents);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "r2");

  referents[1].selected = false;
  referents[2].focused = true;
  resolved = ResolveVoiceReference("that one please", referents);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "r3");
}

TEST(VoiceReferenceResolverTest, DeicticWithNoAnchorIsAmbiguous) {
  EXPECT_EQ(ResolveVoiceReference("open this", SearchResults()).error(),
            VoiceError::kAmbiguousReference);
}

TEST(VoiceReferenceResolverTest, OrdinalScopedByKind) {
  auto resolved =
      ResolveVoiceReference("open the second result", SearchResults());
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "r2");

  resolved = ResolveVoiceReference("the first chart", SearchResults());
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "chart1");
}

TEST(VoiceReferenceResolverTest, LastSelectsHighestOrdinal) {
  auto resolved =
      ResolveVoiceReference("open the last result", SearchResults());
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "r3");
}

TEST(VoiceReferenceResolverTest, OrdinalBeyondRangeIsUnknown) {
  EXPECT_EQ(ResolveVoiceReference("the ninth result", SearchResults()).error(),
            VoiceError::kUnknownReference);
}

TEST(VoiceReferenceResolverTest, LabelMatchIsCaseInsensitive) {
  auto resolved = ResolveVoiceReference("remove the RED LINE", SearchResults());
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "chart1");
}

TEST(VoiceReferenceResolverTest, AmbiguousLabelIsReported) {
  std::vector<VisibleReferent> referents = {
      {"d1", "Saturday", "card", 1, false, false},
      {"d2", "Saturday evening", "card", 2, false, false},
  };
  // "saturday" appears in both labels; exact match breaks the tie.
  auto resolved = ResolveVoiceReference("saturday", referents);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved.value(), "d1");

  EXPECT_EQ(ResolveVoiceReference("satur", referents).error(),
            VoiceError::kAmbiguousReference);
}

TEST(VoiceReferenceResolverTest, UnknownAndBoundsAreExplicit) {
  EXPECT_EQ(
      ResolveVoiceReference("the purple elephant", SearchResults()).error(),
      VoiceError::kUnknownReference);
  EXPECT_EQ(ResolveVoiceReference("anything", {}).error(),
            VoiceError::kUnknownReference);

  std::vector<VisibleReferent> too_many(kMaxVisibleReferents + 1);
  EXPECT_EQ(ResolveVoiceReference("x", too_many).error(),
            VoiceError::kTooManyReferents);

  const std::string long_phrase(kMaxReferencePhraseLength + 1, 'a');
  EXPECT_EQ(ResolveVoiceReference(long_phrase, SearchResults()).error(),
            VoiceError::kTranscriptTooLong);
}

}  // namespace
}  // namespace seoul
