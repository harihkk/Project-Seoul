// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/semantic_to_saui.h"

#include <algorithm>
#include <set>
#include <utility>

#include "seoul/browser/saui/saui_document.h"
#include "seoul/browser/saui/saui_limits.h"
#include "seoul/browser/semantic/semantic_inspection.h"

namespace seoul {

SauiDataConversion::SauiDataConversion() = default;
SauiDataConversion::SauiDataConversion(const SauiDataConversion&) = default;
SauiDataConversion::SauiDataConversion(SauiDataConversion&&) = default;
SauiDataConversion& SauiDataConversion::operator=(const SauiDataConversion&) =
    default;
SauiDataConversion& SauiDataConversion::operator=(SauiDataConversion&&) =
    default;
SauiDataConversion::~SauiDataConversion() = default;

std::map<std::string, std::string> BuildSauiKeyMap(
    const std::vector<std::string>& semantic_ids) {
  std::map<std::string, std::string> result;
  std::set<std::string> occupied;
  for (const std::string& id : semantic_ids) {
    if (IsValidPropKey(id)) {
      occupied.insert(id);
    }
  }
  std::vector<std::string> sorted = semantic_ids;
  std::sort(sorted.begin(), sorted.end());
  sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
  size_t generated_index = 0;
  for (const std::string& id : sorted) {
    if (IsValidPropKey(id)) {
      result[id] = id;
      continue;
    }
    std::string candidate;
    do {
      candidate = "field_" + std::to_string(generated_index++);
    } while (occupied.contains(candidate));
    occupied.insert(candidate);
    result[id] = std::move(candidate);
  }
  return result;
}

namespace {

std::map<std::string, std::string> FieldKeyMap(
    const std::vector<FieldSpec>& fields) {
  std::vector<std::string> ids;
  ids.reserve(fields.size());
  for (const FieldSpec& field : fields) {
    ids.push_back(field.id);
  }
  return BuildSauiKeyMap(ids);
}

DataProvenance ProvenanceFor(const SemanticResult& result) {
  return result.provenance.base;
}

bool FieldHidden(const std::vector<std::string>& hidden,
                 const std::string& field_id) {
  return std::find(hidden.begin(), hidden.end(), field_id) != hidden.end();
}

// Copies a row dict keeping only visible declared fields, in field order.
base::ListValue RowCells(const std::vector<FieldSpec>& fields,
                           const std::vector<std::string>& hidden,
                           const base::DictValue& row) {
  base::ListValue cells;
  for (const FieldSpec& field : fields) {
    if (FieldHidden(hidden, field.id)) {
      continue;
    }
    const base::Value* value = row.Find(field.id);
    cells.Append(value ? value->Clone() : base::Value());
  }
  return cells;
}

DataEntry TableFromRows(const SemanticResult& result,
                        const std::vector<FieldSpec>& fields,
                        const base::ListValue& rows,
                        const std::vector<std::string>& hidden,
                        const std::vector<std::string>& keep_ids,
                        size_t* truncated_rows) {
  DataEntry entry;
  entry.kind = DataEntryKind::kTable;
  const std::map<std::string, std::string> keys = FieldKeyMap(fields);
  for (const FieldSpec& field : fields) {
    if (FieldHidden(hidden, field.id)) {
      continue;
    }
    std::string label = field.label.empty() ? field.id : field.label;
    if (!field.unit.empty()) {
      label += " (" + field.unit + ")";
    }
    entry.table.columns.push_back({keys.at(field.id), label});
  }
  const FieldSpec* identifier = nullptr;
  for (const FieldSpec& field : fields) {
    if (field.role == SemanticRole::kIdentifier) {
      identifier = &field;
      break;
    }
  }
  std::set<std::string> keep(keep_ids.begin(), keep_ids.end());
  for (const base::Value& row_value : rows) {
    const base::DictValue* row = row_value.GetIfDict();
    if (!row) {
      continue;
    }
    if (!keep.empty() && identifier) {
      const std::string* id_value = row->FindString(identifier->id);
      if (!id_value || keep.find(*id_value) == keep.end()) {
        continue;
      }
    }
    if (entry.table.rows.size() >= kMaxTableRows) {
      ++(*truncated_rows);
      continue;
    }
    entry.table.rows.push_back(RowCells(fields, hidden, *row));
  }
  const DataProvenance provenance = ProvenanceFor(result);
  if (!provenance.source_name.empty()) {
    entry.has_provenance = true;
    entry.provenance = provenance;
  }
  return entry;
}

DataEntry RecordFromDict(const SemanticResult& result,
                         const std::vector<FieldSpec>& fields,
                         const std::vector<std::string>& hidden,
                         const base::DictValue& dict) {
  DataEntry entry;
  entry.kind = DataEntryKind::kRecord;
  const std::map<std::string, std::string> keys = FieldKeyMap(fields);
  for (const FieldSpec& field : fields) {
    if (FieldHidden(hidden, field.id)) {
      continue;
    }
    if (const base::Value* value = dict.Find(field.id)) {
      entry.record.Set(keys.at(field.id), value->Clone());
    }
  }
  const DataProvenance provenance = ProvenanceFor(result);
  if (!provenance.source_name.empty()) {
    entry.has_provenance = true;
    entry.provenance = provenance;
  }
  return entry;
}

void EmitSeriesEntries(const SemanticResult& result,
                       const base::ListValue& rows,
                       std::map<std::string, DataEntry>* entries) {
  const FieldSpec* timestamp =
      FindFieldByRole(result.schema, SemanticRole::kTimestamp);
  if (!timestamp) {
    return;
  }
  size_t emitted = 0;
  const std::map<std::string, std::string> keys =
      FieldKeyMap(result.schema.fields);
  for (const FieldSpec* measure : MeasureFields(result.schema)) {
    if (measure->primitive != FieldPrimitive::kNumber &&
        measure->primitive != FieldPrimitive::kInteger) {
      continue;
    }
    if (++emitted > 8) {
      break;  // bounded; the full data remains in the "rows" table
    }
    DataEntry entry;
    entry.kind = DataEntryKind::kSeries;
    entry.series.y_unit = measure->unit;
    entry.series.x_unit = "time";
    for (const base::Value& row_value : rows) {
      const base::DictValue* row = row_value.GetIfDict();
      if (!row) {
        continue;
      }
      std::optional<double> t = row->FindDouble(timestamp->id);
      std::optional<double> y = row->FindDouble(measure->id);
      if (!t.has_value() || !y.has_value()) {
        continue;  // nullable gaps are omitted, never invented
      }
      if (entry.series.points.size() >= kMaxSeriesPoints) {
        break;
      }
      SeriesPoint point;
      point.has_time = true;
      point.time = base::Time::UnixEpoch() + base::Milliseconds(*t);
      point.y = *y;
      entry.series.points.push_back(point);
    }
    const DataProvenance provenance = ProvenanceFor(result);
    if (!provenance.source_name.empty()) {
      entry.has_provenance = true;
      entry.provenance = provenance;
    }
    (*entries)["series_" + keys.at(measure->id)] = std::move(entry);
  }
}

void ConvertInto(const SemanticResult& result,
                 const std::string& prefix,
                 const std::vector<std::string>& hidden,
                 const std::vector<std::string>& keep_ids,
                 SauiDataConversion* out) {
  const SemanticSchema& schema = result.schema;
  auto name = [&prefix](const char* base_name) {
    return prefix.empty() ? std::string(base_name)
                          : prefix + "_" + base_name;
  };
  switch (schema.shape) {
    case SemanticShape::kScalar: {
      DataEntry entry;
      entry.kind = DataEntryKind::kScalar;
      entry.scalar = result.data.Clone();
      const DataProvenance provenance = ProvenanceFor(result);
      if (!provenance.source_name.empty()) {
        entry.has_provenance = true;
        entry.provenance = provenance;
      }
      out->entries[name("value")] = std::move(entry);
      return;
    }
    case SemanticShape::kRecord:
    case SemanticShape::kDocument:
    case SemanticShape::kArtifact:
    case SemanticShape::kDiff: {
      const base::DictValue* dict = result.data.GetIfDict();
      if (dict) {
        out->entries[name("record")] =
            RecordFromDict(result, schema.fields, hidden, *dict);
      }
      return;
    }
    case SemanticShape::kGraph: {
      const base::DictValue* dict = result.data.GetIfDict();
      if (!dict) {
        return;
      }
      if (const base::ListValue* nodes = dict->FindList("nodes")) {
        out->entries[name("nodes")] =
            TableFromRows(result, schema.fields, *nodes, hidden, keep_ids,
                          &out->truncated_rows);
      }
      if (const base::ListValue* edges = dict->FindList("edges")) {
        out->entries[name("edges")] =
            TableFromRows(result, schema.edge_fields, *edges, hidden,
                          keep_ids, &out->truncated_rows);
      }
      return;
    }
    case SemanticShape::kComposite: {
      const base::DictValue* dict = result.data.GetIfDict();
      if (!dict) {
        return;
      }
      for (size_t i = 0; i < schema.parts.size(); ++i) {
        const base::Value* part_data = dict->Find(schema.part_names[i]);
        if (!part_data) {
          continue;
        }
        SemanticResult part;
        part.schema = schema.parts[i];
        part.data = part_data->Clone();
        part.provenance = result.provenance;
        // Ordinal path segments are bounded and collision-free even when
        // semantic part names use their full 64-character allowance. Human
        // names remain in titles; wire identifiers are structural only.
        const std::string segment = "p" + std::to_string(i);
        const std::string part_prefix =
            prefix.empty() ? segment : prefix + "_" + segment;
        ConvertInto(part, part_prefix, hidden, keep_ids, out);
      }
      return;
    }
    default: {
      // Every list shape becomes a table entry; temporal numeric measures
      // additionally become series entries for chart bindings.
      const base::ListValue* rows = result.data.GetIfList();
      if (!rows) {
        return;
      }
      out->entries[name("rows")] =
          TableFromRows(result, schema.fields, *rows, hidden, keep_ids,
                        &out->truncated_rows);
      if (prefix.empty()) {
        EmitSeriesEntries(result, *rows, &out->entries);
      }
      return;
    }
  }
}

}  // namespace

SauiDataConversion ConvertSemanticResult(
    const SemanticResult& result,
    const std::vector<std::string>& hidden_field_ids,
    const std::vector<std::string>& keep_entity_ids) {
  SauiDataConversion conversion;
  ConvertInto(result, std::string(), hidden_field_ids, keep_entity_ids,
              &conversion);
  return conversion;
}

}  // namespace seoul
