// Project Seoul general-purpose operator: tool layer.
// Conformance tests for the canonical capability-descriptor wire codec over
// the shared protocol fixture corpus (see protocol/README.md).

#include "seoul/browser/tools/tool_descriptor_wire.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "seoul/browser/tools/tool_schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::FilePath FixtureDir() {
  base::FilePath src_root;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root));
  return src_root.AppendASCII("seoul/protocol/fixtures");
}

base::Value ReadJson(const base::FilePath& path) {
  std::string contents;
  CHECK(base::ReadFileToString(path, &contents)) << "missing " << path;
  std::optional<base::Value> parsed = base::JSONReader::Read(contents, base::JSON_PARSE_RFC);
  CHECK(parsed.has_value()) << "unparseable " << path;
  return std::move(parsed.value());
}

TEST(ToolDescriptorWireTest, EveryCapabilityFixtureParsesAndRoundTrips) {
  base::FileEnumerator files(FixtureDir().AppendASCII("capability"),
                             /*recursive=*/false,
                             base::FileEnumerator::FILES,
                             FILE_PATH_LITERAL("*.json"));
  size_t seen = 0;
  for (base::FilePath path = files.Next(); !path.empty();
       path = files.Next()) {
    SCOPED_TRACE(path.BaseName().value());
    seen++;
    auto descriptor = ParseToolDescriptor(ReadJson(path));
    ASSERT_TRUE(descriptor.has_value()) << descriptor.error();
    EXPECT_TRUE(IsWellFormedSchema(descriptor->input_schema));
    EXPECT_TRUE(IsWellFormedSchema(descriptor->output_schema));
    // Fixture capabilities must declare themselves: synthetic provider and
    // an observation contract that names the absence of real observation.
    EXPECT_EQ(descriptor->provider, "fixture");

    auto reparsed = ParseToolDescriptor(
        base::Value(ToolDescriptorToValue(descriptor.value())));
    ASSERT_TRUE(reparsed.has_value()) << reparsed.error();
    EXPECT_EQ(ToolDescriptorToValue(descriptor.value()),
              ToolDescriptorToValue(reparsed.value()));
  }
  // The Design Lab catalog registers 20 fixture capabilities plus the
  // schema-exercise descriptor.
  EXPECT_EQ(seen, 21u);
}

TEST(ToolDescriptorWireTest, SchemaExerciseCoversEveryFieldKind) {
  auto descriptor = ParseToolDescriptor(
      ReadJson(FixtureDir().AppendASCII("capability/schema-exercise.json")));
  ASSERT_TRUE(descriptor.has_value()) << descriptor.error();
  const ToolSchema& input = descriptor->input_schema;
  ASSERT_EQ(input.fields.size(), 8u);
  bool saw[8] = {};
  for (const SchemaField& field : input.fields) {
    if (field.name == "query") {
      saw[0] = field.kind == SchemaFieldKind::kString && field.required;
    } else if (field.name == "depth") {
      saw[1] = field.kind == SchemaFieldKind::kInteger && field.has_range;
    } else if (field.name == "threshold") {
      saw[2] = field.kind == SchemaFieldKind::kNumber && field.has_range;
    } else if (field.name == "strict") {
      saw[3] = field.kind == SchemaFieldKind::kBoolean;
    } else if (field.name == "mode") {
      saw[4] = field.kind == SchemaFieldKind::kEnum &&
               field.enum_values.size() == 2;
    } else if (field.name == "source") {
      saw[5] = field.kind == SchemaFieldKind::kUrl;
    } else if (field.name == "stations") {
      saw[6] = field.kind == SchemaFieldKind::kList &&
               field.children.size() == 1;
    } else if (field.name == "window") {
      saw[7] = field.kind == SchemaFieldKind::kObject &&
               field.children.size() == 2;
    }
  }
  for (size_t i = 0; i < 8; i++) {
    EXPECT_TRUE(saw[i]) << "field kind case " << i << " not covered";
  }
}

TEST(ToolDescriptorWireTest, FutureProtocolVersionIsRejected) {
  auto descriptor = ParseToolDescriptor(ReadJson(
      FixtureDir().AppendASCII("compat/descriptor-future-version.json")));
  ASSERT_FALSE(descriptor.has_value());
  EXPECT_NE(descriptor.error().find("schema_version"), std::string::npos);
}

TEST(ToolDescriptorWireTest, EnumWireNamesRoundTrip) {
  for (auto sensitivity :
       {DataSensitivity::kNone, DataSensitivity::kOrganization,
        DataSensitivity::kPageContent, DataSensitivity::kPersonal,
        DataSensitivity::kCredentialAdjacent}) {
    DataSensitivity out;
    EXPECT_TRUE(
        DataSensitivityFromWire(DataSensitivityToWire(sensitivity), &out));
    EXPECT_EQ(out, sensitivity);
  }
  for (auto risk : {RiskCategory::kReadOnly, RiskCategory::kReversibleMutation,
                    RiskCategory::kIrreversibleMutation,
                    RiskCategory::kExternalSideEffect}) {
    RiskCategory out;
    EXPECT_TRUE(RiskCategoryFromWire(RiskCategoryToWire(risk), &out));
    EXPECT_EQ(out, risk);
  }
  for (auto approval :
       {ApprovalPolicy::kNeverRequired, ApprovalPolicy::kFirstUsePerScope,
        ApprovalPolicy::kAlwaysRequired}) {
    ApprovalPolicy out;
    EXPECT_TRUE(ApprovalPolicyFromWire(ApprovalPolicyToWire(approval), &out));
    EXPECT_EQ(out, approval);
  }
  for (auto freshness :
       {FreshnessSemantics::kRealTime, FreshnessSemantics::kNearRealTime,
        FreshnessSemantics::kCached, FreshnessSemantics::kStatic}) {
    FreshnessSemantics out;
    EXPECT_TRUE(
        FreshnessSemanticsFromWire(FreshnessSemanticsToWire(freshness), &out));
    EXPECT_EQ(out, freshness);
  }
}

}  // namespace
}  // namespace seoul
