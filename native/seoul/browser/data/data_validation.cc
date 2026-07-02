// Project Seoul grounded data layer.

#include "seoul/browser/data/data_validation.h"

#include <cmath>

namespace seoul {

const char* FreshnessStateToString(FreshnessState state) {
  switch (state) {
    case FreshnessState::kRealTime:
      return "real_time";
    case FreshnessState::kDelayed:
      return "delayed";
    case FreshnessState::kCached:
      return "cached";
    case FreshnessState::kStale:
      return "stale";
  }
  return "cached";
}

const char* DataErrorToString(DataError error) {
  switch (error) {
    case DataError::kNoProvider:
      return "no_provider";
    case DataError::kProviderUnavailable:
      return "provider_unavailable";
    case DataError::kProviderError:
      return "provider_error";
    case DataError::kMalformedResult:
      return "malformed_result";
    case DataError::kMissingProvenance:
      return "missing_provenance";
    case DataError::kUnresolvedEntity:
      return "unresolved_entity";
    case DataError::kEmptyResult:
      return "empty_result";
    case DataError::kStaleBeyondPolicy:
      return "stale_beyond_policy";
    case DataError::kBudgetExceeded:
      return "budget_exceeded";
    case DataError::kCancelled:
      return "cancelled";
  }
  return "provider_error";
}

const char* DataContractViolationToString(DataContractViolation violation) {
  switch (violation) {
    case DataContractViolation::kEligible:
      return "eligible";
    case DataContractViolation::kMissingSource:
      return "missing_source";
    case DataContractViolation::kMissingTimestamps:
      return "missing_timestamps";
    case DataContractViolation::kOutOfRangeValue:
      return "out_of_range_value";
  }
  return "out_of_range_value";
}

DataContractViolation ValidateProvenance(const DataProvenance& provenance) {
  if (provenance.source_name.empty()) {
    return DataContractViolation::kMissingSource;
  }
  if (provenance.retrieved_at.is_null() || provenance.effective_at.is_null()) {
    return DataContractViolation::kMissingTimestamps;
  }
  if (!std::isfinite(provenance.completeness) ||
      provenance.completeness < 0.0 || provenance.completeness > 1.0) {
    return DataContractViolation::kOutOfRangeValue;
  }
  return DataContractViolation::kEligible;
}

}  // namespace seoul
