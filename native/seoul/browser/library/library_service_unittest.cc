// Project Seoul Library, Boards, and Live Collections.

#include "seoul/browser/library/library_service.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

class LibraryServiceTest : public testing::Test {
 protected:
  LibraryServiceTest()
      : now_(base::Time::FromSecondsSinceUnixEpoch(1000)),
        service_(base::BindRepeating(&LibraryServiceTest::Now,
                                     base::Unretained(this))) {}

  base::Time Now() const { return now_; }
  void Advance(base::TimeDelta delta) { now_ += delta; }

  LiveCollectionDefinition FixtureDefinition() {
    LiveCollectionDefinition definition;
    definition.name = "Reading feed";
    definition.refresh_capability =
        ToolId::FromString("connector.fixture.refresh_collection");
    definition.source_locator = "fixture-source-1";
    definition.refresh_interval_minutes = 15;
    return definition;
  }

  base::Time now_;
  LibraryService service_;
};

TEST_F(LibraryServiceTest, BoardEditsValidateAndRemainAtomic) {
  auto board = service_.CreateBoard("Research board");
  ASSERT_TRUE(board.has_value());

  BoardElement text;
  text.kind = BoardElementKind::kText;
  text.text = "A durable note";
  auto element = service_.AddBoardElement(*board, text);
  ASSERT_TRUE(element.has_value());
  ASSERT_EQ(service_.FindBoard(*board)->elements.size(), 1u);

  BoardElement invalid = service_.FindBoard(*board)->elements.front();
  invalid.width = 0.0;
  EXPECT_EQ(service_.UpdateBoardElement(*board, invalid).error(),
            LibraryError::kInvalidElement);
  EXPECT_EQ(service_.FindBoard(*board)->elements.front().width, 320.0);

  BoardElement link;
  link.kind = BoardElementKind::kLink;
  link.reference = "javascript:alert(1)";
  EXPECT_EQ(service_.AddBoardElement(*board, link).error(),
            LibraryError::kInvalidElement);
  EXPECT_EQ(service_.FindBoard(*board)->elements.size(), 1u);
}

TEST_F(LibraryServiceTest, LiveRefreshRejectsLateResponses) {
  auto id = service_.CreateLiveCollection(FixtureDefinition());
  ASSERT_TRUE(id.has_value());
  auto first = service_.BeginLiveRefresh(*id);
  auto second = service_.BeginLiveRefresh(*id);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());

  LiveCollectionItem old_item;
  old_item.stable_key = "old";
  old_item.title = "Old response";
  EXPECT_EQ(service_.CompleteLiveRefresh(*id, *first, {old_item}, std::nullopt)
                .error(),
            LibraryError::kStaleRefresh);

  LiveCollectionItem current;
  current.stable_key = "current";
  current.title = "Current response";
  ASSERT_TRUE(
      service_.CompleteLiveRefresh(*id, *second, {current}, std::nullopt)
          .has_value());
  ASSERT_EQ(service_.FindLiveCollection(*id)->items.size(), 1u);
  EXPECT_EQ(service_.FindLiveCollection(*id)->items.front().stable_key,
            "current");
}

TEST_F(LibraryServiceTest, ReconfigurationInvalidatesActiveRefresh) {
  LiveCollectionDefinition definition = FixtureDefinition();
  auto id = service_.CreateLiveCollection(definition);
  ASSERT_TRUE(id.has_value());
  auto generation = service_.BeginLiveRefresh(*id);
  ASSERT_TRUE(generation.has_value());

  definition.id = *id;
  definition.source_locator = "fixture-source-2";
  ASSERT_TRUE(service_.UpdateLiveCollection(definition).has_value());
  EXPECT_EQ(service_.FindLiveCollection(*id)->refresh_state,
            LiveRefreshState::kIdle);

  LiveCollectionItem stale_item;
  stale_item.stable_key = "stale";
  stale_item.title = "Old source response";
  EXPECT_EQ(service_
                .CompleteLiveRefresh(*id, *generation, {stale_item},
                                     std::nullopt)
                .error(),
            LibraryError::kStaleRefresh);
  EXPECT_TRUE(service_.FindLiveCollection(*id)->items.empty());
}

TEST_F(LibraryServiceTest, ActionableLiveItemRequiresSafeUrl) {
  auto id = service_.CreateLiveCollection(FixtureDefinition());
  ASSERT_TRUE(id.has_value());
  auto generation = service_.BeginLiveRefresh(*id);
  ASSERT_TRUE(generation.has_value());

  LiveCollectionItem item;
  item.stable_key = "action-without-target";
  item.title = "Unsafe action";
  item.actionable = true;
  EXPECT_EQ(service_
                .CompleteLiveRefresh(*id, *generation, {item}, std::nullopt)
                .error(),
            LibraryError::kInvalidLiveItem);
  EXPECT_TRUE(service_.FindLiveCollection(*id)->items.empty());
}

TEST_F(LibraryServiceTest, RefreshFailurePreservesLastGoodItems) {
  auto id = service_.CreateLiveCollection(FixtureDefinition());
  ASSERT_TRUE(id.has_value());
  auto generation = service_.BeginLiveRefresh(*id);
  ASSERT_TRUE(generation.has_value());
  LiveCollectionItem item;
  item.stable_key = "one";
  item.title = "Verified item";
  ASSERT_TRUE(
      service_.CompleteLiveRefresh(*id, *generation, {item}, std::nullopt)
          .has_value());

  auto failed_generation = service_.BeginLiveRefresh(*id);
  ASSERT_TRUE(failed_generation.has_value());
  ASSERT_TRUE(service_
                  .CompleteLiveRefresh(*id, *failed_generation, {},
                                       std::string("provider unavailable"))
                  .has_value());
  const LiveCollectionRecord* record = service_.FindLiveCollection(*id);
  ASSERT_TRUE(record);
  EXPECT_EQ(record->refresh_state, LiveRefreshState::kError);
  ASSERT_EQ(record->items.size(), 1u);
  EXPECT_EQ(record->items.front().stable_key, "one");
}

TEST_F(LibraryServiceTest, RefreshDueUsesLastSuccessfulRefresh) {
  auto id = service_.CreateLiveCollection(FixtureDefinition());
  ASSERT_TRUE(id.has_value());
  EXPECT_TRUE(service_.IsRefreshDue(*id, now_));
  auto generation = service_.BeginLiveRefresh(*id);
  ASSERT_TRUE(generation.has_value());
  ASSERT_TRUE(service_.CompleteLiveRefresh(*id, *generation, {}, std::nullopt)
                  .has_value());
  EXPECT_FALSE(service_.IsRefreshDue(*id, now_));
  Advance(base::Minutes(15));
  EXPECT_TRUE(service_.IsRefreshDue(*id, now_));
}

TEST_F(LibraryServiceTest, PersistenceRoundTripsAndSkipsCorruption) {
  auto board = service_.CreateBoard("Ideas");
  ASSERT_TRUE(board.has_value());
  BoardElement element;
  element.kind = BoardElementKind::kText;
  element.text = "Keep me";
  ASSERT_TRUE(service_.AddBoardElement(*board, element).has_value());

  auto collection = service_.CreateLiveCollection(FixtureDefinition());
  ASSERT_TRUE(collection.has_value());
  auto generation = service_.BeginLiveRefresh(*collection);
  ASSERT_TRUE(generation.has_value());
  LiveCollectionItem item;
  item.stable_key = "story-1";
  item.title = "Story";
  item.url = "https://example.test/story";
  ASSERT_TRUE(
      service_
          .CompleteLiveRefresh(*collection, *generation, {item}, std::nullopt)
          .has_value());

  base::DictValue state = service_.TakePersistedState();
  base::DictValue corrupt;
  corrupt.Set("id", "not-a-uuid");
  corrupt.Set("name", "Corrupt");
  state.FindList("boards")->Append(std::move(corrupt));

  // Duplicate element ids are corrupt input and must not create ambiguous
  // board mutations after restart.
  base::DictValue* persisted_board =
      state.FindList("boards")->front().GetIfDict();
  ASSERT_TRUE(persisted_board);
  base::ListValue* persisted_elements = persisted_board->FindList("elements");
  ASSERT_TRUE(persisted_elements);
  persisted_elements->Append(persisted_elements->front().Clone());

  LibraryService restored(base::BindRepeating(
      []() { return base::Time::FromSecondsSinceUnixEpoch(2000); }));
  restored.RestorePersistedState(state);
  EXPECT_EQ(restored.board_count(), 1u);
  EXPECT_EQ(restored.live_collection_count(), 1u);
  ASSERT_TRUE(restored.FindBoard(*board));
  EXPECT_EQ(restored.FindBoard(*board)->elements.size(), 1u);
  EXPECT_EQ(restored.FindBoard(*board)->elements.front().text, "Keep me");
  ASSERT_TRUE(restored.FindLiveCollection(*collection));
  EXPECT_EQ(restored.FindLiveCollection(*collection)->items.front().title,
            "Story");
}

TEST_F(LibraryServiceTest, MutationsNotifyPersistenceOwner) {
  int notifications = 0;
  LibraryService observed(
      base::BindRepeating([](const base::Time* now) { return *now; }, &now_),
      base::BindRepeating([](int* count) { ++*count; }, &notifications));

  auto board = observed.CreateBoard("Observed");
  ASSERT_TRUE(board.has_value());
  EXPECT_EQ(notifications, 1);

  EXPECT_EQ(observed.RenameBoard(*board, "Still observed").has_value(), true);
  EXPECT_EQ(notifications, 2);

  EXPECT_EQ(observed.RenameBoard(BoardId::GenerateNew(), "Missing").error(),
            LibraryError::kUnknownBoard);
  EXPECT_EQ(notifications, 2);
}

TEST_F(LibraryServiceTest, ArtifactStoresMetadataNotBytes) {
  LibraryArtifact artifact;
  artifact.kind = LibraryArtifactKind::kCapture;
  artifact.title = "Web capture";
  artifact.reference = "capture-handle-1";
  artifact.origin = "https://example.test";
  artifact.mime_type = "image/png";
  auto id = service_.AddArtifact(artifact);
  ASSERT_TRUE(id.has_value());
  const LibraryArtifact* stored = service_.FindArtifact(*id);
  ASSERT_TRUE(stored);
  EXPECT_EQ(stored->reference, "capture-handle-1");
}

}  // namespace
}  // namespace seoul
