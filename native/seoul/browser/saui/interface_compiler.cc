// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/interface_compiler.h"

#include <utility>

#include "seoul/browser/saui/saui_catalog.h"
#include "seoul/browser/saui/saui_limits.h"
#include "seoul/browser/saui/saui_validator.h"
#include "seoul/browser/saui/semantic_to_saui.h"
#include "seoul/browser/semantic/semantic_inspection.h"
#include "seoul/browser/semantic/semantic_validation.h"

namespace seoul {

InterfaceIntent::InterfaceIntent() = default;
InterfaceIntent::InterfaceIntent(const InterfaceIntent&) = default;
InterfaceIntent::InterfaceIntent(InterfaceIntent&&) = default;
InterfaceIntent& InterfaceIntent::operator=(const InterfaceIntent&) = default;
InterfaceIntent& InterfaceIntent::operator=(InterfaceIntent&&) = default;
InterfaceIntent::~InterfaceIntent() = default;

CompiledInterface::CompiledInterface() = default;
CompiledInterface::CompiledInterface(const CompiledInterface&) = default;
CompiledInterface::CompiledInterface(CompiledInterface&&) = default;
CompiledInterface& CompiledInterface::operator=(const CompiledInterface&) =
    default;
CompiledInterface& CompiledInterface::operator=(CompiledInterface&&) = default;
CompiledInterface::~CompiledInterface() = default;

namespace {

// Bounded number of composite/document sections materialized as components.
constexpr size_t kMaxCompiledSections = 32;
constexpr size_t kMaxCompiledActions = 16;
constexpr size_t kMaxCompiledSources = 64;

std::string FieldLabel(const FieldSpec& field) {
  return field.label.empty() ? field.id : field.label;
}

std::string NonEmptyUnit(const FieldSpec& field) {
  return field.unit.empty() ? std::string("value") : field.unit;
}

ComponentNode MakeComponent(const std::string& id,
                            ComponentType type,
                            const std::string& accessible_name) {
  ComponentNode node;
  node.id = id;
  node.type = type;
  node.accessible_name = accessible_name;
  return node;
}

ComponentNode BoundComponent(const std::string& id,
                             ComponentType type,
                             const std::string& entry_name,
                             const std::string& accessible_name) {
  ComponentNode node = MakeComponent(id, type, accessible_name);
  node.bindings["data"] = entry_name;
  return node;
}

void PrefixComponentTree(ComponentNode* node, const std::string& prefix) {
  node->id = prefix + "-" + node->id;
  for (auto& [slot, entry_name] : node->bindings) {
    entry_name = prefix + "_" + entry_name;
  }
  for (ComponentNode& child : node->children) {
    PrefixComponentTree(&child, prefix);
  }
}

void SetChartProps(ComponentNode* node,
                   const std::string& title,
                   const std::string& x_label,
                   const std::string& y_label,
                   const std::string& units) {
  node->props.Set("title", title);
  node->props.Set("x_label", x_label);
  node->props.Set("y_label", y_label);
  node->props.Set("units", units);
}

// Charts additionally require attributed, timed data (the SAUI validator
// enforces this on the surface; the compiler falls back instead of failing).
bool ChartProvenanceComplete(const SemanticResult& result) {
  const DataProvenance& provenance = result.provenance.base;
  return !provenance.source_name.empty() &&
         !provenance.retrieved_at.is_null() &&
         !provenance.effective_at.is_null();
}

// The requested representation is honored only when the data can honestly
// support it.
bool RequestCompatible(ComponentType requested,
                       const SemanticResult& result) {
  const SemanticSchema& schema = result.schema;
  const ComponentTypeInfo& info = GetComponentTypeInfo(requested);
  if (info.chart) {
    if (ChartWouldMislead(result) || !ChartProvenanceComplete(result)) {
      return false;
    }
    if (requested == ComponentType::kCandlestickChart) {
      return OhlcEligible(schema);
    }
    if (requested == ComponentType::kLineChart ||
        requested == ComponentType::kAreaChart ||
        requested == ComponentType::kSparkline) {
      return HasTemporalAxis(schema);
    }
    // Bar/pie/scatter/histogram/heat map need at least a measure, checked by
    // ChartWouldMislead above, plus rows to aggregate.
    return RowCount(result) >= 2;
  }
  switch (requested) {
    case ComponentType::kMap:
    case ComponentType::kGeoLayer:
      return GeoEligible(schema);
    case ComponentType::kTree:
      return schema.shape == SemanticShape::kHierarchy ||
             schema.shape == SemanticShape::kCodeStructure;
    case ComponentType::kNetworkGraph:
      return schema.shape == SemanticShape::kGraph;
    case ComponentType::kTimeline:
      return schema.shape == SemanticShape::kIntervalSeries ||
             schema.shape == SemanticShape::kEventStream ||
             HasTemporalAxis(schema);
    case ComponentType::kTable:
    case ComponentType::kSortableTable:
    case ComponentType::kComparisonMatrix:
    case ComponentType::kList:
      return RowCount(result) > 0 ||
             schema.shape == SemanticShape::kRecord;
    case ComponentType::kSchemaForm:
      return true;  // "make this editable" always has a form rendering
    default:
      return false;  // not a representation users can force
  }
}

}  // namespace

CompileResult CompileInterface(const SemanticResult& result,
                               const InterfaceIntent& intent,
                               const SurfaceId& existing_surface) {
  if (auto valid = ValidateSemanticResult(result); !valid.has_value()) {
    return base::unexpected(SauiError::kInvalidDataEntry);
  }

  CompiledInterface compiled;
  AdaptiveSurface& surface = compiled.surface;
  surface.id = existing_surface.is_valid() ? existing_surface
                                           : SurfaceId::GenerateNew();
  surface.kind = intent.pin ? SurfaceKind::kDashboard : SurfaceKind::kResponse;
  surface.pinned = intent.pin;
  surface.title = intent.title;
  if (surface.kind != SurfaceKind::kResponse && surface.title.empty()) {
    surface.title = result.provenance.base.source_name.empty()
                        ? "Pinned result"
                        : result.provenance.base.source_name;
  }
  surface.provenance.generator = "deterministic";
  for (const Citation& citation : result.provenance.citations) {
    if (surface.provenance.source_urls.size() >= kMaxCompiledSources) {
      break;
    }
    surface.provenance.source_urls.push_back(citation.url);
  }

  if (!intent.hidden_field_ids.empty()) {
    compiled.reasons.push_back(CompilerReason::kFieldsHidden);
  }
  if (!intent.compare_entity_ids.empty()) {
    compiled.reasons.push_back(CompilerReason::kFilteredToComparedEntities);
  }

  SauiDataConversion conversion = ConvertSemanticResult(
      result, intent.hidden_field_ids, intent.compare_entity_ids);
  surface.data = std::move(conversion.entries);
  if (conversion.truncated_rows > 0) {
    compiled.reasons.push_back(CompilerReason::kRowsTruncated);
  }

  const SemanticSchema& schema = result.schema;
  ComponentNode root = MakeComponent("root", ComponentType::kStack, "");
  auto add = [&root](ComponentNode node) {
    root.children.push_back(std::move(node));
  };

  // Empty list results render an explicit empty state, never a blank panel.
  const bool list_like = surface.data.find("rows") != surface.data.end();
  if (list_like && RowCount(result) == 0) {
    ComponentNode empty =
        MakeComponent("empty", ComponentType::kEmptyState, "");
    empty.props.Set("text", "No results.");
    add(std::move(empty));
    compiled.reasons.push_back(CompilerReason::kEmptyResult);
    surface.components.push_back(std::move(root));
    if (auto valid = ValidateSurface(surface); !valid.has_value()) {
      return base::unexpected(valid.error());
    }
    return compiled;
  }

  // 1. Explicit user representation request, when honest.
  std::optional<ComponentType> chosen;
  if (intent.requested_representation.has_value()) {
    if (RequestCompatible(*intent.requested_representation, result)) {
      chosen = intent.requested_representation;
      compiled.reasons.push_back(
          CompilerReason::kUserRequestedRepresentation);
    } else {
      compiled.reasons.push_back(
          CompilerReason::kUserRequestRejectedMisleading);
    }
  }
  if (intent.wants_editable && !chosen.has_value() &&
      schema.shape != SemanticShape::kFormSchema) {
    // "Make this editable": present the row fields as a schema form bound to
    // the record/rows entry.
    chosen = ComponentType::kSchemaForm;
  }

  // 2. Automatic shape-and-role selection.
  const std::vector<const FieldSpec*> measures = MeasureFields(schema);
  const FieldSpec* timestamp =
      FindFieldByRole(schema, SemanticRole::kTimestamp);
  const FieldSpec* name_field = FindFieldByRole(schema, SemanticRole::kName);

  auto compile_primary = [&](ComponentType type) -> void {
    switch (type) {
      case ComponentType::kMetric: {
        ComponentNode metric = BoundComponent(
            "primary", ComponentType::kMetric, "value",
            FieldLabel(schema.fields[0]));
        metric.props.Set("label", FieldLabel(schema.fields[0]));
        add(std::move(metric));
        return;
      }
      case ComponentType::kEntityCard:
      case ComponentType::kKeyValueCard:
      case ComponentType::kDocument:
      case ComponentType::kFile: {
        add(BoundComponent("primary", type, "record",
                           surface.title.empty() ? "Result"
                                                 : surface.title));
        return;
      }
      case ComponentType::kSchemaForm: {
        const std::string entry =
            surface.data.find("rows") != surface.data.end() ? "rows"
                                                            : "record";
        add(BoundComponent("primary", ComponentType::kSchemaForm, entry,
                           "Provide the missing details"));
        return;
      }
      case ComponentType::kLineChart:
      case ComponentType::kAreaChart:
      case ComponentType::kBarChart:
      case ComponentType::kStackedBarChart:
      case ComponentType::kScatterChart:
      case ComponentType::kPieChart:
      case ComponentType::kHistogram:
      case ComponentType::kHeatMap:
      case ComponentType::kRangeChart:
      case ComponentType::kCandlestickChart: {
        const FieldSpec* measure = measures.empty() ? nullptr : measures[0];
        ComponentNode chart = BoundComponent(
            "primary", type, "rows",
            measure ? FieldLabel(*measure) + " over "
                          + (timestamp ? FieldLabel(*timestamp) : "items")
                    : "Chart");
        SetChartProps(&chart,
                      !intent.title.empty()
                          ? intent.title
                          : (measure ? FieldLabel(*measure) : "Result"),
                      timestamp ? FieldLabel(*timestamp) : "Item",
                      measure ? FieldLabel(*measure) : "Value",
                      measure ? NonEmptyUnit(*measure) : "value");
        if (type == ComponentType::kBarChart ||
            type == ComponentType::kStackedBarChart) {
          chart.props.Set("baseline_zero", true);
        }
        if (intent.percentages) {
          chart.props.Set("percentages", true);
        }
        chart.update_policy = UpdatePolicy::kLive;
        add(std::move(chart));
        return;
      }
      case ComponentType::kNetworkGraph: {
        ComponentNode graph = BoundComponent(
            "primary", ComponentType::kNetworkGraph, "nodes",
            "Relationship graph");
        graph.props.Set("title", intent.title.empty() ? "Relationships"
                                                      : intent.title);
        graph.bindings["edges"] = "edges";
        add(std::move(graph));
        return;
      }
      case ComponentType::kMap: {
        ComponentNode map_component = BoundComponent(
            "primary", ComponentType::kMap, "rows", "Map of results");
        map_component.props.Set(
            "title", intent.title.empty() ? "Locations" : intent.title);
        add(std::move(map_component));
        if (RowCount(result) <= 50) {
          add(BoundComponent("marker-list", ComponentType::kMapMarkerList,
                             "rows", "Mapped items"));
        }
        return;
      }
      case ComponentType::kTree: {
        add(BoundComponent("primary", ComponentType::kTree, "rows",
                           "Hierarchy"));
        return;
      }
      case ComponentType::kTimeline: {
        add(BoundComponent("primary", ComponentType::kTimeline, "rows",
                           "Timeline"));
        return;
      }
      case ComponentType::kActivityLog: {
        add(BoundComponent("primary", ComponentType::kActivityLog, "rows",
                           "Activity"));
        return;
      }
      case ComponentType::kComparisonMatrix: {
        add(BoundComponent("primary", ComponentType::kComparisonMatrix,
                           "rows", "Comparison"));
        return;
      }
      case ComponentType::kMedia: {
        add(BoundComponent("primary", ComponentType::kMedia, "rows",
                           "Media"));
        return;
      }
      case ComponentType::kDiffView: {
        ComponentNode diff =
            MakeComponent("primary", ComponentType::kDiffView, "");
        const base::DictValue* dict = result.data.GetIfDict();
        const FieldSpec* body =
            FindFieldByRole(schema, SemanticRole::kBody);
        const std::string* text =
            dict && body ? dict->FindString(body->id) : nullptr;
        diff.props.Set("diff", text ? *text : std::string(" "));
        add(std::move(diff));
        return;
      }
      case ComponentType::kFileTree: {
        add(BoundComponent("primary", ComponentType::kFileTree, "rows",
                           "Code structure"));
        return;
      }
      case ComponentType::kSortableTable:
      case ComponentType::kTable:
      case ComponentType::kList:
      default: {
        ComponentNode table = BoundComponent(
            "primary",
            type == ComponentType::kList ? ComponentType::kList
                                         : ComponentType::kSortableTable,
            "rows", "Result table");
        if (!intent.group_by_field_id.empty()) {
          std::vector<std::string> field_ids;
          field_ids.reserve(schema.fields.size());
          for (const FieldSpec& field : schema.fields) {
            field_ids.push_back(field.id);
          }
          const auto key_map = BuildSauiKeyMap(field_ids);
          const auto key = key_map.find(intent.group_by_field_id);
          if (key != key_map.end()) {
            table.props.Set("group_by", key->second);
          }
          // A group_by that names a field with no mapped key is dropped rather
          // than emitted raw: the renderer groups only by a declared column
          // key, so an unmapped id could never match one.
        }
        add(std::move(table));
        return;
      }
    }
  };

  if (chosen.has_value()) {
    compile_primary(*chosen);
  } else {
    switch (schema.shape) {
      case SemanticShape::kScalar:
        compile_primary(ComponentType::kMetric);
        compiled.reasons.push_back(CompilerReason::kShapeScalarMetric);
        break;
      case SemanticShape::kRecord:
        if (name_field ||
            FindFieldByRole(schema, SemanticRole::kIdentifier)) {
          compile_primary(ComponentType::kEntityCard);
          compiled.reasons.push_back(CompilerReason::kRecordEntityCard);
        } else {
          compile_primary(ComponentType::kKeyValueCard);
          compiled.reasons.push_back(CompilerReason::kRecordKeyValue);
        }
        break;
      case SemanticShape::kTimeSeries:
      case SemanticShape::kSeries:
        if (ChartWouldMislead(result) || !ChartProvenanceComplete(result)) {
          compile_primary(ComponentType::kSortableTable);
          compiled.reasons.push_back(CompilerReason::kChartWouldMislead);
          compiled.reasons.push_back(CompilerReason::kFallbackGenericTable);
        } else if (OhlcEligible(schema)) {
          compile_primary(ComponentType::kCandlestickChart);
          compiled.reasons.push_back(CompilerReason::kOhlcCandlestick);
        } else {
          compile_primary(ComponentType::kLineChart);
          compiled.reasons.push_back(
              CompilerReason::kTemporalMeasureLineChart);
        }
        break;
      case SemanticShape::kIntervalSeries:
        compile_primary(ComponentType::kTimeline);
        compiled.reasons.push_back(CompilerReason::kIntervalsTimeline);
        break;
      case SemanticShape::kEventStream:
        compile_primary(ComponentType::kTimeline);
        compiled.reasons.push_back(CompilerReason::kEventsTimeline);
        break;
      case SemanticShape::kStatusStream:
        compile_primary(ComponentType::kActivityLog);
        compiled.reasons.push_back(CompilerReason::kStatusActivityLog);
        break;
      case SemanticShape::kHierarchy:
        compile_primary(ComponentType::kTree);
        compiled.reasons.push_back(CompilerReason::kHierarchyTree);
        break;
      case SemanticShape::kGraph:
        compile_primary(ComponentType::kNetworkGraph);
        compiled.reasons.push_back(CompilerReason::kGraphNetwork);
        break;
      case SemanticShape::kGeoFeatures:
      case SemanticShape::kRoute:
        compile_primary(ComponentType::kMap);
        compiled.reasons.push_back(CompilerReason::kGeoMap);
        break;
      case SemanticShape::kEntityCollection:
      case SemanticShape::kTable:
      case SemanticShape::kCube: {
        const size_t rows = RowCount(result);
        if (ComparableEntities(result) && rows >= 2 && rows <= 6 &&
            !measures.empty()) {
          compile_primary(ComponentType::kComparisonMatrix);
          compiled.reasons.push_back(
              CompilerReason::kComparableEntitiesMatrix);
        } else {
          compile_primary(ComponentType::kSortableTable);
          compiled.reasons.push_back(CompilerReason::kCollectionTable);
        }
        break;
      }
      case SemanticShape::kDocument:
        compile_primary(ComponentType::kDocument);
        compiled.reasons.push_back(CompilerReason::kDocumentView);
        break;
      case SemanticShape::kDocumentSections: {
        // Materialize bounded collapsible sections from the rows.
        const base::ListValue* rows = result.data.GetIfList();
        const FieldSpec* heading =
            FindFieldByRole(schema, SemanticRole::kName);
        const FieldSpec* body = FindFieldByRole(schema, SemanticRole::kBody);
        size_t index = 0;
        for (const base::Value& row_value : *rows) {
          if (index >= kMaxCompiledSections) {
            compiled.reasons.push_back(CompilerReason::kRowsTruncated);
            break;
          }
          const base::DictValue* row = row_value.GetIfDict();
          if (!row) {
            continue;
          }
          const std::string* title =
              heading ? row->FindString(heading->id) : nullptr;
          const std::string* text = body ? row->FindString(body->id)
                                         : nullptr;
          ComponentNode section = MakeComponent(
              "section-" + std::to_string(index),
              ComponentType::kCollapsibleSection, "");
          section.props.Set("title", title ? *title
                                           : "Section " +
                                                 std::to_string(index + 1));
          ComponentNode paragraph = MakeComponent(
              "section-text-" + std::to_string(index), ComponentType::kText,
              "");
          paragraph.props.Set("text", text ? *text : std::string(" "));
          section.children.push_back(std::move(paragraph));
          add(std::move(section));
          ++index;
        }
        compiled.reasons.push_back(CompilerReason::kDocumentSections);
        break;
      }
      case SemanticShape::kCitations: {
        // Build the structured sources prop from the rows.
        const base::ListValue* rows = result.data.GetIfList();
        const FieldSpec* url = FindFieldByRole(schema, SemanticRole::kUrl);
        const FieldSpec* title_field =
            FindFieldByRole(schema, SemanticRole::kName);
        ComponentNode sources =
            MakeComponent("primary", ComponentType::kSourceList, "");
        base::ListValue source_items;
        for (const base::Value& row_value : *rows) {
          if (source_items.size() >= kMaxCompiledSources) {
            break;
          }
          const base::DictValue* row = row_value.GetIfDict();
          if (!row || !url) {
            continue;
          }
          const std::string* href = row->FindString(url->id);
          if (!href) {
            continue;
          }
          base::DictValue item;
          item.Set("href", *href);
          const std::string* text =
              title_field ? row->FindString(title_field->id) : nullptr;
          item.Set("title", text ? *text : *href);
          source_items.Append(std::move(item));
        }
        sources.props.Set("sources", std::move(source_items));
        add(std::move(sources));
        compiled.reasons.push_back(CompilerReason::kCitationsSourceList);
        break;
      }
      case SemanticShape::kMedia:
        compile_primary(ComponentType::kMedia);
        compiled.reasons.push_back(CompilerReason::kMediaGallery);
        break;
      case SemanticShape::kArtifact:
        compile_primary(ComponentType::kFile);
        compiled.reasons.push_back(CompilerReason::kArtifactFile);
        break;
      case SemanticShape::kFormSchema:
        compile_primary(ComponentType::kSchemaForm);
        compiled.reasons.push_back(CompilerReason::kFormFromMissingInputs);
        break;
      case SemanticShape::kActionSet: {
        const base::ListValue* rows = result.data.GetIfList();
        const FieldSpec* identifier =
            FindFieldByRole(schema, SemanticRole::kIdentifier);
        const FieldSpec* label = FindFieldByRole(schema, SemanticRole::kName);
        ComponentNode actions_row =
            MakeComponent("actions", ComponentType::kRow, "");
        size_t index = 0;
        for (const base::Value& row_value : *rows) {
          if (index >= kMaxCompiledActions) {
            break;
          }
          const base::DictValue* row = row_value.GetIfDict();
          if (!row || !identifier) {
            continue;
          }
          const std::string* action_id = row->FindString(identifier->id);
          if (!action_id) {
            continue;
          }
          const std::string* text =
              label ? row->FindString(label->id) : nullptr;
          SurfaceAction action;
          action.id = *action_id;
          action.label = text ? *text : *action_id;
          action.kind = SurfaceActionKind::kToolCall;
          action.target = *action_id;
          surface.actions.push_back(std::move(action));
          ComponentNode button = MakeComponent(
              "action-" + std::to_string(index), ComponentType::kButton,
              text ? *text : *action_id);
          button.props.Set("label", text ? *text : *action_id);
          button.action_ids.push_back(*action_id);
          actions_row.children.push_back(std::move(button));
          ++index;
        }
        add(std::move(actions_row));
        compiled.reasons.push_back(CompilerReason::kActionButtons);
        break;
      }
      case SemanticShape::kDiff:
        compile_primary(ComponentType::kDiffView);
        compiled.reasons.push_back(CompilerReason::kDiffView);
        break;
      case SemanticShape::kCodeStructure:
        compile_primary(ComponentType::kFileTree);
        compiled.reasons.push_back(CompilerReason::kCodeStructureTree);
        break;
      case SemanticShape::kComposite: {
        // Each part renders through the same rules and stacks in order.
        const base::DictValue* dict = result.data.GetIfDict();
        for (size_t i = 0; i < schema.parts.size() &&
                           i < kMaxCompiledSections;
             ++i) {
          const base::Value* part_data =
              dict ? dict->Find(schema.part_names[i]) : nullptr;
          if (!part_data) {
            continue;
          }
          SemanticResult part;
          part.schema = schema.parts[i];
          part.data = part_data->Clone();
          part.provenance = result.provenance;
          InterfaceIntent part_intent;
          part_intent.title = schema.part_names[i];
          auto part_compiled = CompileInterface(part, part_intent);
          if (!part_compiled.has_value()) {
            continue;
          }
          // Re-prefix the part's components and data into this surface.
          const std::string part_key = "p" + std::to_string(i);
          for (auto& [entry_name, entry] :
               part_compiled->surface.data) {
            surface.data[part_key + "_" + entry_name] =
                std::move(entry);
          }
          ComponentNode part_root = std::move(
              part_compiled->surface.components[0]);
          // Rewrite ids/bindings with the part prefix to stay unique.
          part_root.id = "part-" + part_key;
          for (ComponentNode& child : part_root.children) {
            PrefixComponentTree(&child, part_key);
          }
          add(std::move(part_root));
        }
        compiled.reasons.push_back(CompilerReason::kCompositeSections);
        break;
      }
    }
  }

  // Unknown or degenerate outcomes fall back rather than fail: if nothing was
  // composed, render the data as a generic table or record card.
  if (root.children.empty()) {
    if (surface.data.find("rows") != surface.data.end()) {
      add(BoundComponent("primary", ComponentType::kSortableTable, "rows",
                         "Result table"));
      compiled.reasons.push_back(CompilerReason::kFallbackGenericTable);
    } else if (surface.data.find("record") != surface.data.end()) {
      add(BoundComponent("primary", ComponentType::kKeyValueCard, "record",
                         "Result"));
      compiled.reasons.push_back(CompilerReason::kFallbackRecordCard);
    } else {
      ComponentNode empty =
          MakeComponent("empty", ComponentType::kEmptyState, "");
      empty.props.Set("text", "No presentable data.");
      add(std::move(empty));
      compiled.reasons.push_back(CompilerReason::kEmptyResult);
    }
  }

  surface.components.push_back(std::move(root));
  if (auto valid = ValidateSurface(surface); !valid.has_value()) {
    return base::unexpected(valid.error());
  }
  return compiled;
}

const char* CompilerReasonToString(CompilerReason reason) {
  switch (reason) {
    case CompilerReason::kShapeScalarMetric:
      return "shape_scalar_metric";
    case CompilerReason::kRecordEntityCard:
      return "record_entity_card";
    case CompilerReason::kRecordKeyValue:
      return "record_key_value";
    case CompilerReason::kComparableEntitiesMatrix:
      return "comparable_entities_matrix";
    case CompilerReason::kCollectionTable:
      return "collection_table";
    case CompilerReason::kTemporalMeasureLineChart:
      return "temporal_measure_line_chart";
    case CompilerReason::kOhlcCandlestick:
      return "ohlc_candlestick";
    case CompilerReason::kGeoMap:
      return "geo_map";
    case CompilerReason::kHierarchyTree:
      return "hierarchy_tree";
    case CompilerReason::kGraphNetwork:
      return "graph_network";
    case CompilerReason::kIntervalsTimeline:
      return "intervals_timeline";
    case CompilerReason::kEventsTimeline:
      return "events_timeline";
    case CompilerReason::kStatusActivityLog:
      return "status_activity_log";
    case CompilerReason::kFormFromMissingInputs:
      return "form_from_missing_inputs";
    case CompilerReason::kDocumentView:
      return "document_view";
    case CompilerReason::kDocumentSections:
      return "document_sections";
    case CompilerReason::kCitationsSourceList:
      return "citations_source_list";
    case CompilerReason::kMediaGallery:
      return "media_gallery";
    case CompilerReason::kArtifactFile:
      return "artifact_file";
    case CompilerReason::kActionButtons:
      return "action_buttons";
    case CompilerReason::kDiffView:
      return "diff_view";
    case CompilerReason::kCodeStructureTree:
      return "code_structure_tree";
    case CompilerReason::kCompositeSections:
      return "composite_sections";
    case CompilerReason::kEmptyResult:
      return "empty_result";
    case CompilerReason::kFallbackGenericTable:
      return "fallback_generic_table";
    case CompilerReason::kFallbackRecordCard:
      return "fallback_record_card";
    case CompilerReason::kUserRequestedRepresentation:
      return "user_requested_representation";
    case CompilerReason::kUserRequestRejectedMisleading:
      return "user_request_rejected_misleading";
    case CompilerReason::kChartWouldMislead:
      return "chart_would_mislead";
    case CompilerReason::kRowsTruncated:
      return "rows_truncated";
    case CompilerReason::kFieldsHidden:
      return "fields_hidden";
    case CompilerReason::kFilteredToComparedEntities:
      return "filtered_to_compared_entities";
  }
  return "fallback_generic_table";
}

}  // namespace seoul
