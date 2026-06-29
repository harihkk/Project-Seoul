// Project Seoul native lifecycle bridge.
// Authored for later compilation on a capable host. Not run on the dev machine.

#include "seoul/browser/lifecycle/lifecycle_identity.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(LifecycleIdentityTest, LiveKeyEquality) {
  EXPECT_TRUE(LiveTabKey::FromSessionId(5) == LiveTabKey::FromSessionId(5));
  EXPECT_FALSE(LiveTabKey::FromSessionId(5) == LiveTabKey::FromSessionId(6));
  EXPECT_TRUE(LiveWindowKey::FromSessionId(1) ==
              LiveWindowKey::FromSessionId(1));
}

TEST(LifecycleIdentityTest, StringFormsArePrefixedAndNumeric) {
  EXPECT_EQ("t-7", LiveTabKey::FromSessionId(7).value());
  EXPECT_EQ("w-3", LiveWindowKey::FromSessionId(3).value());
  EXPECT_EQ("tref-7", PersistedTabRef::FromSessionId(7).value());
  EXPECT_EQ("wref-3", PersistedWindowRef::FromSessionId(3).value());
}

TEST(LifecycleIdentityTest, PersistedRefParseRoundTrip) {
  PersistedTabRef ref = PersistedTabRef::Parse("tref-42");
  EXPECT_TRUE(ref.is_valid());
  EXPECT_EQ(42, ref.session_id());
  EXPECT_EQ("tref-42", ref.value());

  PersistedWindowRef wref = PersistedWindowRef::Parse("wref-9");
  EXPECT_TRUE(wref.is_valid());
  EXPECT_EQ(9, wref.session_id());
}

TEST(LifecycleIdentityTest, InvalidParseYieldsInvalid) {
  EXPECT_FALSE(PersistedTabRef::Parse("garbage").is_valid());
  EXPECT_FALSE(PersistedTabRef::Parse("").is_valid());
  // Wrong prefix is rejected (a window ref string is not a tab ref).
  EXPECT_FALSE(PersistedTabRef::Parse("wref-1").is_valid());
  // A zero-valued id is treated as invalid.
  EXPECT_FALSE(PersistedTabRef::Parse("tref-0").is_valid());
}

TEST(LifecycleIdentityTest, DefaultIsInvalidAndEmpty) {
  EXPECT_FALSE(LiveTabKey().is_valid());
  EXPECT_TRUE(LiveTabKey().value().empty());
  EXPECT_FALSE(LiveWindowKey().is_valid());
}

TEST(LifecycleIdentityTest, NoPointerOrIndexInSerialization) {
  // The serialized form is exactly prefix + session id, never a pointer/index.
  const std::string v = LiveTabKey::FromSessionId(123).value();
  EXPECT_EQ("t-123", v);
  EXPECT_NE(std::string::npos, v.find("t-"));
}

TEST(LifecycleIdentityTest, DistinctSessionIdsDoNotCollide) {
  // A reused tab index never reuses a session-id-based identity.
  EXPECT_FALSE(LiveTabKey::FromSessionId(100) ==
               LiveTabKey::FromSessionId(101));
  EXPECT_TRUE(LiveWindowKey::FromSessionId(1) <
              LiveWindowKey::FromSessionId(2));
}

}  // namespace
}  // namespace seoul
