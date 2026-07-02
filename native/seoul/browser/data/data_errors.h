// Project Seoul grounded data layer.
// Precise, domain-neutral error results for capability data acquisition. When
// no capability can supply requested data the result is an explicit
// kNoProvider error surfaced to the user as "unavailable"; Seoul never
// substitutes fabricated or placeholder values.

#ifndef SEOUL_BROWSER_DATA_DATA_ERRORS_H_
#define SEOUL_BROWSER_DATA_DATA_ERRORS_H_

#include "base/types/expected.h"

namespace seoul {

enum class DataError {
  kNoProvider,           // no capability supplies this data family
  kProviderUnavailable,  // capability registered but unreachable/disabled
  kProviderError,        // provider returned an error
  kMalformedResult,      // result failed contract validation
  kMissingProvenance,    // result lacked source or timing attribution
  kUnresolvedEntity,     // an input entity could not be resolved
  kEmptyResult,          // provider returned no data for the request
  kStaleBeyondPolicy,    // cached data older than the configured maximum
  kBudgetExceeded,       // request rejected by the task/network budget
  kCancelled,
};

const char* DataErrorToString(DataError error);

template <typename T>
using DataResult = base::expected<T, DataError>;

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_DATA_ERRORS_H_
