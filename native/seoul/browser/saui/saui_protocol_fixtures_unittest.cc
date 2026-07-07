// Project Seoul Adaptive UI (SAUI).
// Conformance tests over the shared protocol fixture corpus
// (protocol/fixtures/, mirrored to src/seoul/protocol/ by materialize.sh).
// The TypeScript protocol tests consume the same files, so the surface and
// patch wire formats cannot drift between languages without a failure here
// or there.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "seoul/browser/saui/saui_document.h"
#include "seoul/browser/saui/saui_patch.h"
#include "seoul/browser/saui/saui_validator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::FilePath FixtureDir() {
  base::FilePath src_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));
  return src_root.AppendASCII("seoul/protocol/fixtures");
}

base::Value ReadFixture(const std::string& relative) {
  std::string contents;
  const base::FilePath path = FixtureDir().AppendASCII(relative);
  CHECK(base::ReadFileToString(path, &contents))
      << "missing fixture " << path;
  std::optional<base::Value> parsed = base::JSONReader::Read(contents, base::JSON_PARSE_RFC);
  CHECK(parsed.has_value()) << "unparseable fixture " << relative;
  return std::move(parsed.value());
}

AdaptiveSurface LoadDashboard() {
  auto surface = ParseSurface(ReadFixture("surface/dashboard.json"));
  CHECK(surface.has_value());
  return std::move(surface.value());
}

TEST(SauiProtocolFixturesTest, SurfaceFixturesParseValidateAndRoundTrip) {
  for (const char* name : {"surface/minimal.json", "surface/dashboard.json"}) {
    SCOPED_TRACE(name);
    auto surface = ParseSurface(ReadFixture(name));
    ASSERT_TRUE(surface.has_value());
    EXPECT_TRUE(ValidateSurface(surface.value()).has_value());

    auto reparsed =
        ParseSurface(base::Value(SurfaceToValue(surface.value())));
    ASSERT_TRUE(reparsed.has_value());
    EXPECT_EQ(surface.value(), reparsed.value());
  }
}

TEST(SauiProtocolFixturesTest, PatchFixturesParseAndApplyToDashboard) {
  AdaptiveSurface surface = LoadDashboard();

  // Ordered so every fixture applies cleanly: props/title, data upsert,
  // stream append, then the structural rewrite.
  for (const char* name :
       {"patch/set-props.json", "patch/data-update.json",
        "patch/stream-append.json", "patch/structure.json"}) {
    SCOPED_TRACE(name);
    auto patch = ParseSurfacePatch(ReadFixture(name));
    ASSERT_TRUE(patch.has_value());
    auto applied = ApplySurfacePatch(surface, patch.value());
    ASSERT_TRUE(applied.has_value());
  }
  // The structural fixture appended the badge, replaced the table in place,
  // and removed the headline metric: three children remain.
  EXPECT_EQ(surface.components[0].children.size(), 3u);
  EXPECT_EQ(surface.title, "Pipeline health (updated)");
  EXPECT_EQ(surface.data["latency"].series.points.size(), 6u);
}

TEST(SauiProtocolFixturesTest, SetBindingsFixtureRebindsAfterEntryExists) {
  AdaptiveSurface surface = LoadDashboard();
  const AdaptiveSurface before = surface;

  auto rebind = ParseSurfacePatch(ReadFixture("patch/set-bindings.json"));
  ASSERT_TRUE(rebind.has_value());

  // The rebind targets an entry the surface does not carry yet: it must fail
  // atomically first, then succeed once the entry exists.
  EXPECT_FALSE(ApplySurfacePatch(surface, rebind.value()).has_value());
  EXPECT_EQ(surface, before);

  SurfacePatch add_entry;
  add_entry.surface_id = surface.id;
  SurfacePatchOp op;
  op.kind = PatchOpKind::kUpsertDataEntry;
  op.entry_name = "latency_backfill";
  op.entry = surface.data["latency"];  // same kind and provenance
  add_entry.ops.push_back(std::move(op));
  ASSERT_TRUE(ApplySurfacePatch(surface, add_entry).has_value());

  auto applied = ApplySurfacePatch(surface, rebind.value());
  ASSERT_TRUE(applied.has_value());
  EXPECT_EQ(surface.components[0].children[1].bindings.at("data"),
            "latency_backfill");
}

TEST(SauiProtocolFixturesTest, InvalidFixturesAreRejected) {
  EXPECT_FALSE(
      ParseSurface(ReadFixture("invalid/surface-unknown-component.json"))
          .has_value());
  EXPECT_FALSE(
      ParseSurface(ReadFixture("invalid/surface-event-handler-prop.json"))
          .has_value());
  EXPECT_FALSE(
      ParseSurfacePatch(ReadFixture("invalid/patch-unknown-op.json"))
          .has_value());
  EXPECT_FALSE(
      ParseSurfacePatch(ReadFixture("invalid/patch-empty-ops.json"))
          .has_value());
}

TEST(SauiProtocolFixturesTest, SurfaceWithoutVersionIsRejected) {
  EXPECT_FALSE(
      ParseSurface(ReadFixture("compat/surface-missing-version.json"))
          .has_value());
}

}  // namespace
}  // namespace seoul
