// Project Seoul context threads.
// Unit tests for approved-only context, sensitive rejection, and cloud scope
// minimization.

#include "seoul/browser/context/context_thread.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::Time Now() {
  return base::Time::UnixEpoch() + base::Days(20000);
}

ContextItem TabItem() {
  ContextItem item;
  item.kind = ContextItemKind::kTabReference;
  item.title = "GN reference";
  item.reference = "tab-key-123";
  item.origin = "https://gn.googlesource.com";
  return item;
}

ContextItem Excerpt(const std::string& text) {
  ContextItem item;
  item.kind = ContextItemKind::kExcerpt;
  item.title = "Selected passage";
  item.text = text;
  return item;
}

TEST(ContextThreadTest, AddsApprovedItemsAndAssignsIds) {
  ContextThread thread("thread-1", "Research");
  auto id = thread.AddItem(TabItem(), Now());
  ASSERT_TRUE(id.has_value());
  EXPECT_FALSE(id->empty());
  ASSERT_EQ(thread.items().size(), 1u);
  EXPECT_EQ(thread.items()[0].kind, ContextItemKind::kTabReference);
  EXPECT_EQ(thread.items()[0].id, id.value());
}

TEST(ContextThreadTest, RejectsSensitiveItems) {
  ContextThread thread("thread-1", "Research");
  ContextItem sensitive = TabItem();
  sensitive.flagged_sensitive = true;  // e.g. a tab on a password manager
  EXPECT_EQ(thread.AddItem(sensitive, Now()).error(),
            ContextError::kSensitiveItemRejected);
  EXPECT_TRUE(thread.items().empty());
}

TEST(ContextThreadTest, ValidatesKindSpecificRequirements) {
  ContextThread thread("thread-1", "Research");
  ContextItem no_reference;
  no_reference.kind = ContextItemKind::kCitation;  // needs a reference
  EXPECT_EQ(thread.AddItem(no_reference, Now()).error(),
            ContextError::kInvalidItem);

  ContextItem empty_note;
  empty_note.kind = ContextItemKind::kNote;
  EXPECT_EQ(thread.AddItem(empty_note, Now()).error(),
            ContextError::kInvalidItem);

  ContextItem long_excerpt =
      Excerpt(std::string(kMaxContextExcerptLength + 1, 'x'));
  EXPECT_EQ(thread.AddItem(long_excerpt, Now()).error(),
            ContextError::kExcerptTooLong);
}

TEST(ContextThreadTest, RemoveClearAndArchive) {
  ContextThread thread("thread-1", "Research");
  auto id = thread.AddItem(TabItem(), Now());
  ASSERT_TRUE(id.has_value());
  EXPECT_EQ(thread.RemoveItem("nope").error(), ContextError::kUnknownItem);
  ASSERT_TRUE(thread.RemoveItem(id.value()).has_value());
  EXPECT_TRUE(thread.items().empty());

  ASSERT_TRUE(thread.AddItem(TabItem(), Now()).has_value());
  thread.Archive();
  EXPECT_TRUE(thread.archived());
  thread.Clear();
  EXPECT_TRUE(thread.items().empty());
}

TEST(ContextThreadTest, MinimizeForCloudStripsBodiesWhenRequested) {
  ContextThread thread("thread-1", "Research");
  ASSERT_TRUE(thread.AddItem(TabItem(), Now()).has_value());
  ASSERT_TRUE(
      thread.AddItem(Excerpt("a long selected passage"), Now()).has_value());

  CloudContextScope with_bodies =
      MinimizeForCloud(thread, /*include_bodies=*/true, /*max_bytes=*/100000);
  EXPECT_EQ(with_bodies.items.size(), 2u);
  bool excerpt_kept = false;
  for (const ContextItem& item : with_bodies.items) {
    if (item.kind == ContextItemKind::kExcerpt) {
      excerpt_kept = !item.text.empty();
    }
  }
  EXPECT_TRUE(excerpt_kept);

  CloudContextScope minimal =
      MinimizeForCloud(thread, /*include_bodies=*/false, /*max_bytes=*/100000);
  EXPECT_EQ(minimal.items.size(), 2u);
  for (const ContextItem& item : minimal.items) {
    if (item.kind == ContextItemKind::kExcerpt) {
      EXPECT_TRUE(item.text.empty());  // body stripped; reference kept
    }
  }
}

TEST(ContextThreadTest, MinimizeForCloudRespectsByteBudget) {
  ContextThread thread("thread-1", "Research");
  for (int i = 0; i < 5; ++i) {
    ContextItem item = TabItem();
    item.reference = "tab-" + std::to_string(i);
    ASSERT_TRUE(thread.AddItem(item, Now()).has_value());
  }
  CloudContextScope tiny =
      MinimizeForCloud(thread, /*include_bodies=*/true, /*max_bytes=*/40);
  EXPECT_LT(tiny.items.size(), 5u);  // budget dropped the rest, not truncated
  EXPECT_LE(tiny.approximate_bytes, 40u);
}

TEST(ContextThreadTest, ArchivedThreadIsNeverSent) {
  ContextThread thread("thread-1", "Research");
  ASSERT_TRUE(thread.AddItem(TabItem(), Now()).has_value());
  thread.Archive();
  CloudContextScope scope =
      MinimizeForCloud(thread, /*include_bodies=*/true, /*max_bytes=*/100000);
  EXPECT_TRUE(scope.items.empty());
}

}  // namespace
}  // namespace seoul
