// Project Seoul grounded data layer.
// Precise error results for data providers and validation. When no provider is
// configured the result is an explicit kNoProvider error surfaced to the user
// as "unavailable"; Seoul never substitutes fabricated or placeholder values.

#ifndef SEOUL_BROWSER_DATA_DATA_ERRORS_H_
#define SEOUL_BROWSER_DATA_DATA_ERRORS_H_

#include "base/types/expected.h"

namespace seoul {

enum class DataError {
  kNoProvider,           // no provider configured for this data family
  kProviderUnavailable,  // provider configured but unreachable/disabled
  kProviderError,        // provider returned an error
  kMalformedResult,      // provider result failed contract validation
  kMissingProvenance,    // result lacked source or timing attribution
  kUnknownLocation,      // geocoding/location resolution failed
  kUnknownInstrument,    // symbol/instrument resolution failed
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
