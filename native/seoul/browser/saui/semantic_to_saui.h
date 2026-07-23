// Project Seoul Adaptive UI (SAUI).
// Conversion from the generic semantic result model to SAUI data entries.
// This is a pure structural mapping (shapes to scalar/record/series/table
// entries); it carries provenance through and never inspects domain meaning.

#ifndef SEOUL_BROWSER_SAUI_SEMANTIC_TO_SAUI_H_
#define SEOUL_BROWSER_SAUI_SEMANTIC_TO_SAUI_H_

#include <map>
#include <string>
#include <vector>

#include "seoul/browser/saui/saui_types.h"
#include "seoul/browser/semantic/semantic_types.h"

namespace seoul {

// Result of converting one semantic result. Entry names are deterministic:
// "value" (scalar), "record" (dict shapes), "rows" (list shapes), "nodes" and
// "edges" (graph), "series_<field_id>" per temporal numeric measure, and
// part-prefixed names for composites ("<part>_rows", ...).
struct SauiDataConversion {
  SauiDataConversion();
  SauiDataConversion(const SauiDataConversion&);
  SauiDataConversion(SauiDataConversion&&);
  SauiDataConversion& operator=(const SauiDataConversion&);
  SauiDataConversion& operator=(SauiDataConversion&&);
  ~SauiDataConversion();

  std::map<std::string, DataEntry> entries;
  // Rows dropped because the semantic result exceeded the SAUI table bound;
  // surfaced so truncation is never silent.
  size_t truncated_rows = 0;
};

// Builds a collision-free mapping from semantic identifiers into SAUI prop
// keys. Already-safe keys remain unchanged. Longer or reserved identifiers
// receive deterministic field_N keys assigned by lexical order, never hashes;
// every mapping is therefore bounded, reproducible, and collision-free for the
// complete schema.
std::map<std::string, std::string> BuildSauiKeyMap(
    const std::vector<std::string>& semantic_ids);

// Converts a validated semantic result. `hidden_field_ids` drops those fields
// from emitted tables/records (user-directed "hide that column");
// `keep_entity_ids`, when non-empty, keeps only rows whose identifier-role
// value is in the set ("compare only these items").
SauiDataConversion ConvertSemanticResult(
    const SemanticResult& result,
    const std::vector<std::string>& hidden_field_ids,
    const std::vector<std::string>& keep_entity_ids);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SEMANTIC_TO_SAUI_H_
