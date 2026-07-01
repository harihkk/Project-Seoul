// Project Seoul Adaptive UI (SAUI).
// Semantic validation for parsed surfaces: unique component and action ids,
// resolvable bindings with matching kinds, catalog-required properties,
// accessible names, and the chart honesty rules (title, labeled axes, units,
// attributed and timed data, indicated axis truncation, bounded pie slices).
// A surface that fails validation is never rendered.

#ifndef SEOUL_BROWSER_SAUI_SAUI_VALIDATOR_H_
#define SEOUL_BROWSER_SAUI_SAUI_VALIDATOR_H_

#include "seoul/browser/saui/saui_errors.h"
#include "seoul/browser/saui/saui_types.h"

namespace seoul {

// Validates the complete surface. Returns the first violation found; the
// walk order is deterministic (components in tree order, then actions).
SauiStatusResult ValidateSurface(const AdaptiveSurface& surface);

// True when `entry` can back a chart: a series with at least two points, or a
// table with at least two rows, carrying provenance with a named source and
// both timestamps. One unverified number is never chart-eligible.
bool EntryChartEligible(const DataEntry& entry);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_VALIDATOR_H_
