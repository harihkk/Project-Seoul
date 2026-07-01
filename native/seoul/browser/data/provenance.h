// Project Seoul grounded data layer.
// Provenance envelope attached to every data result that can back a visual.
// A surface may only render data-backed components when the data carries a
// provenance record; fabricated or unattributed values are rejected at
// validation time (see data_validation.h and saui_validator.h).

#ifndef SEOUL_BROWSER_DATA_PROVENANCE_H_
#define SEOUL_BROWSER_DATA_PROVENANCE_H_

#include <string>

#include "base/time/time.h"

namespace seoul {

// How current the data is relative to its source.
enum class FreshnessState {
  kRealTime,  // streamed or fetched live from the source
  kDelayed,   // source-imposed delay (for example delayed market quotes)
  kCached,    // served from a local cache within its validity window
  kStale,     // past its validity window; display only with a stale indicator
};

const char* FreshnessStateToString(FreshnessState state);

// Attribution and timing for one data result. `retrieved_at` is when Seoul
// obtained the data; `effective_at` is the moment the data describes (a quote
// time, a forecast issue time). Both are required for chart eligibility.
struct DataProvenance {
  std::string source_name;  // human-readable provider name; never empty
  std::string source_url;   // link to the source when the provider permits
  base::Time retrieved_at;
  base::Time effective_at;
  std::string
      timezone;       // IANA zone name for display (for example "Asia/Seoul")
  std::string units;  // display units for scalar/series data; may be empty
  FreshnessState freshness = FreshnessState::kCached;
  // Fraction of the requested fields the provider actually returned, in
  // [0.0, 1.0]. 1.0 means complete.
  double completeness = 1.0;

  friend bool operator==(const DataProvenance&,
                         const DataProvenance&) = default;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_PROVENANCE_H_
