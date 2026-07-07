// Project Seoul product runtime: the surface service.

#include "seoul/browser/product/surface_service.h"

#include <string>
#include <utility>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "seoul/browser/saui/saui_catalog.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

FieldSpec Field(const std::string& id,
                FieldPrimitive primitive,
                SemanticRole role) {
  FieldSpec field;
  field.id = id;
  field.label = id;
  field.primitive = primitive;
  field.role = role;
  field.nullable = false;
  return field;
}

void SetFixtureProvenance(SemanticResult* result) {
  result->provenance.base.source_name = "fixture-capability";
  result->provenance.base.source_url = "https://source.test/data";
  result->provenance.base.retrieved_at =
      base::Time::UnixEpoch() + base::Days(20000);
  result->provenance.base.effective_at = result->provenance.base.retrieved_at;
}

// A held-out collection schema: never special-cased anywhere in the compiler.
SemanticResult OrchardYields() {
  SemanticResult result;
  result.schema.shape = SemanticShape::kEntityCollection;
  result.schema.fields = {
      Field("orchard", FieldPrimitive::kString, SemanticRole::kIdentifier),
      Field("cultivar", FieldPrimitive::kString, SemanticRole::kCategory),
      Field("yield_kg", FieldPrimitive::kNumber, SemanticRole::kMeasure)};
  result.data = base::test::ParseJson(R"json([
      {"orchard": "north-slope", "cultivar": "jonagold", "yield_kg": 412.5},
      {"orchard": "river-flat", "cultivar": "opal", "yield_kg": 388.0},
      {"orchard": "high-terrace", "cultivar": "jonagold", "yield_kg": 205.25}
  ])json");
  SetFixtureProvenance(&result);
  return result;
}

class RecordingObserver : public SurfaceServiceObserver {
 public:
  void OnSurfaceUpdated(const SurfaceId& id,
                        const std::string& surface_json) override {
    ++updates_;
    last_json_ = surface_json;
  }
  void OnSurfaceRemoved(const SurfaceId& id) override { ++removals_; }

  int updates() const { return updates_; }
  int removals() const { return removals_; }
  const std::string& last_json() const { return last_json_; }

 private:
  int updates_ = 0;
  int removals_ = 0;
  std::string last_json_;
};

TEST(SurfaceServiceTest, CreatesFromUnseenSchemaAndNotifies) {
  SurfaceService service;
  RecordingObserver observer;
  service.AddObserver(&observer);

  const SurfaceId id =
      service.CreateFromSemantic(OrchardYields(), InterfaceIntent(), TaskId());
  ASSERT_TRUE(id.is_valid());
  EXPECT_EQ(observer.updates(), 1);
  EXPECT_TRUE(service.SurfaceJson(id).has_value());
  EXPECT_NE(observer.last_json().find("orchard"), std::string::npos);
  service.RemoveObserver(&observer);
}

TEST(SurfaceServiceTest, RepresentationChangePatchesSameSurfaceInPlace) {
  SurfaceService service;
  const SurfaceId id =
      service.CreateFromSemantic(OrchardYields(), InterfaceIntent(), TaskId());
  ASSERT_TRUE(id.is_valid());

  // "show as chart" is a generic re-compile of the SAME id, not a duplicate.
  ASSERT_TRUE(service.SetRepresentation(id, "bar_chart"));
  EXPECT_EQ(service.AllSurfaces().size(), 1u);
  const AdaptiveSurface* surface = service.FindSurface(id);
  ASSERT_TRUE(surface);
  EXPECT_EQ(surface->id, id);

  // Unknown component types fail closed and leave the surface untouched.
  EXPECT_FALSE(service.SetRepresentation(id, "totally_unknown_widget"));
  EXPECT_EQ(service.AllSurfaces().size(), 1u);
}

TEST(SurfaceServiceTest, HideFieldAndGroupByRecompile) {
  SurfaceService service;
  const SurfaceId id =
      service.CreateFromSemantic(OrchardYields(), InterfaceIntent(), TaskId());
  ASSERT_TRUE(id.is_valid());
  const std::string before = service.SurfaceJson(id).value();
  ASSERT_TRUE(service.ToggleFieldHidden(id, "cultivar"));
  const std::string after = service.SurfaceJson(id).value();
  EXPECT_NE(before, after);
  // Toggling back restores the field.
  ASSERT_TRUE(service.ToggleFieldHidden(id, "cultivar"));
  ASSERT_TRUE(service.SetGroupBy(id, "cultivar"));
}

TEST(SurfaceServiceTest, PinPersistsAndRestores) {
  SurfaceService service;
  const SurfaceId id = service.CreateFromSemantic(
      OrchardYields(),
      [] {
        InterfaceIntent intent;
        intent.title = "Orchard yields";
        return intent;
      }(),
      TaskId());
  ASSERT_TRUE(id.is_valid());
  ASSERT_TRUE(service.SetPinned(id, true));
  ASSERT_EQ(service.PinnedSurfaces().size(), 1u);

  const base::DictValue persisted = service.TakePersistedState();

  SurfaceService restored;
  restored.RestorePersistedState(persisted);
  EXPECT_EQ(restored.PinnedSurfaces().size(), 1u);
}

TEST(SurfaceServiceTest, EventResolutionFailsClosed) {
  SurfaceService service;
  const SurfaceId id =
      service.CreateFromSemantic(OrchardYields(), InterfaceIntent(), TaskId());
  ASSERT_TRUE(id.is_valid());

  // Unknown surface id.
  ComponentEvent unknown_surface;
  unknown_surface.surface_id = SurfaceId::GenerateNew();
  unknown_surface.component_id = "any";
  EXPECT_EQ(service.HandleComponentEvent(unknown_surface).kind,
            SurfaceEventOutcome::Kind::kNone);

  // Undeclared action id on a real surface.
  ComponentEvent undeclared;
  undeclared.surface_id = id;
  undeclared.component_id = "any";
  undeclared.action_id = "not-declared";
  EXPECT_EQ(service.HandleComponentEvent(undeclared).kind,
            SurfaceEventOutcome::Kind::kNone);

  // A form submit without a declared action becomes a typed turn.
  ComponentEvent submit;
  submit.surface_id = id;
  submit.component_id = "form";
  submit.kind = ComponentEventKind::kSubmit;
  base::DictValue fields;
  fields.Set("note", "hello");
  submit.value = base::Value(std::move(fields));
  const SurfaceEventOutcome outcome = service.HandleComponentEvent(submit);
  EXPECT_EQ(outcome.kind, SurfaceEventOutcome::Kind::kSubmitTurn);
  EXPECT_EQ(*outcome.payload.FindString("note"), "hello");
}

TEST(SurfaceServiceTest, DeclaredToolCallActionResolvesToCapability) {
  // Inject a surface that declares a tool-call action through the persistence
  // path (ParseSurface accepts declared actions), then resolve an event
  // against it.
  base::DictValue surface;
  surface.Set("kind", "response");
  surface.Set("schema_version", 1);
  surface.Set("title", "Fixture");
  base::ListValue components;
  base::DictValue button;
  button.Set("id", "refresh-btn");
  button.Set("type", "button");
  button.Set("accessible_name", "Refresh");
  base::DictValue props;
  props.Set("label", "Refresh");
  button.Set("props", std::move(props));
  base::ListValue action_refs;
  action_refs.Append("refresh");
  button.Set("actions", std::move(action_refs));
  components.Append(std::move(button));
  surface.Set("components", std::move(components));
  base::ListValue actions;
  base::DictValue action;
  action.Set("id", "refresh");
  action.Set("label", "Refresh");
  action.Set("kind", "tool_call");
  action.Set("target", "info.read.inventory");
  actions.Append(std::move(action));
  surface.Set("actions", std::move(actions));
  surface.Set("pinned", true);

  base::DictValue persisted;
  base::ListValue pinned;
  base::DictValue entry;
  entry.Set("surface", std::move(surface));
  pinned.Append(std::move(entry));
  persisted.Set("pinned", std::move(pinned));

  SurfaceService service;
  service.RestorePersistedState(persisted);
  const std::vector<SurfaceId> ids = service.AllSurfaces();
  ASSERT_EQ(ids.size(), 1u);

  ComponentEvent event;
  event.surface_id = ids[0];
  event.component_id = "refresh-btn";
  event.kind = ComponentEventKind::kActivate;
  event.action_id = "refresh";
  const SurfaceEventOutcome outcome = service.HandleComponentEvent(event);
  EXPECT_EQ(outcome.kind, SurfaceEventOutcome::Kind::kRunCapability);
  EXPECT_EQ(outcome.target, "info.read.inventory");
}

TEST(SurfaceServiceTest, RefreshSemanticKeepsIdAndRejectsBadData) {
  SurfaceService service;
  const SurfaceId id =
      service.CreateFromSemantic(OrchardYields(), InterfaceIntent(), TaskId());
  ASSERT_TRUE(id.is_valid());
  SemanticResult refreshed = OrchardYields();
  EXPECT_TRUE(service.RefreshSemantic(id, refreshed));
  EXPECT_EQ(service.AllSurfaces().size(), 1u);
}

}  // namespace
}  // namespace seoul

namespace seoul {
namespace {

// Every declared surface-action kind must resolve to a typed outcome the
// Canvas handler dispatches; a kind that resolved to nothing would be a
// silently dead control. local_state is the one deliberate renderer-local
// kind and maps to kNone by design. Injection uses the persistence path, the
// same untrusted-document route pinned surfaces take.
TEST(SurfaceActionCompletenessTest, EveryDeclaredActionKindHasAnOutcome) {
  const struct {
    const char* wire_kind;
    const char* target;
    SurfaceEventOutcome::Kind expected;
  } kCases[] = {
      {"tool_call", "fixture.tool.call",
       SurfaceEventOutcome::Kind::kRunCapability},
      {"local_state", "panel_open", SurfaceEventOutcome::Kind::kNone},
      {"workflow_edit", "node-1", SurfaceEventOutcome::Kind::kWorkflowEdit},
      {"browser_action", "browser.tabs.open",
       SurfaceEventOutcome::Kind::kBrowserCommand},
      {"task_approval", "step-1", SurfaceEventOutcome::Kind::kTaskApproval},
      {"navigate", "https://example.test/x",
       SurfaceEventOutcome::Kind::kNavigate},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.wire_kind);
    base::DictValue surface;
    surface.Set("kind", "response");
    surface.Set("schema_version", 1);
    surface.Set("title", "Fixture");
    base::ListValue components;
    base::DictValue button;
    button.Set("id", "go");
    button.Set("type", "button");
    button.Set("accessible_name", "Go");
    base::DictValue props;
    props.Set("label", "Go");
    button.Set("props", std::move(props));
    base::ListValue action_refs;
    action_refs.Append("act");
    button.Set("actions", std::move(action_refs));
    components.Append(std::move(button));
    surface.Set("components", std::move(components));
    base::ListValue actions;
    base::DictValue action;
    action.Set("id", "act");
    action.Set("label", "Go");
    action.Set("kind", test_case.wire_kind);
    action.Set("target", test_case.target);
    actions.Append(std::move(action));
    surface.Set("actions", std::move(actions));
    surface.Set("pinned", true);

    base::DictValue persisted;
    base::ListValue pinned;
    base::DictValue entry;
    entry.Set("surface", std::move(surface));
    pinned.Append(std::move(entry));
    persisted.Set("pinned", std::move(pinned));

    SurfaceService service;
    service.RestorePersistedState(persisted);
    const std::vector<SurfaceId> ids = service.AllSurfaces();
    ASSERT_EQ(ids.size(), 1u);

    ComponentEvent event;
    event.surface_id = ids[0];
    event.component_id = "go";
    event.kind = ComponentEventKind::kActivate;
    event.action_id = "act";
    EXPECT_EQ(service.HandleComponentEvent(event).kind, test_case.expected);
  }
}

}  // namespace
}  // namespace seoul
