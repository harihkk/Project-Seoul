// Project Seoul grounded data layer.
// Generic provenance validation. Domain-specific result contracts do not
// exist in this module: every capability result is expressed through the
// generic semantic fabric (seoul/browser/semantic/), and the only invariant
// owned here is that data carrying a provenance envelope is attributed and
// timed. Shape- and role-specific validation lives in semantic_validation.h.

#ifndef SEOUL_BROWSER_DATA_DATA_VALIDATION_H_
#define SEOUL_BROWSER_DATA_DATA_VALIDATION_H_

#include "seoul/browser/data/provenance.h"

namespace seoul {

// Why provenance failed validation. kEligible means valid.
enum class DataContractViolation {
  kEligible,
  kMissingSource,
  kMissingTimestamps,
  kOutOfRangeValue,
};

const char* DataContractViolationToString(DataContractViolation violation);

// Provenance is well-formed: named source and both timestamps present,
// completeness within [0, 1].
DataContractViolation ValidateProvenance(const DataProvenance& provenance);

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_DATA_VALIDATION_H_
