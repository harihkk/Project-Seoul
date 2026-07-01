// Project Seoul grounded data layer.
// Contract validation for provider results and chart eligibility. A visual is
// only eligible when the underlying data is real, attributed, and sufficient;
// "one unverified number" never becomes a chart.

#ifndef SEOUL_BROWSER_DATA_DATA_VALIDATION_H_
#define SEOUL_BROWSER_DATA_DATA_VALIDATION_H_

#include <string>

#include "seoul/browser/data/market_types.h"
#include "seoul/browser/data/product_types.h"
#include "seoul/browser/data/provenance.h"
#include "seoul/browser/data/weather_types.h"

namespace seoul {

// Why data failed validation or chart eligibility. kEligible means valid.
enum class DataContractViolation {
  kEligible,
  kMissingSource,
  kMissingTimestamps,
  kMissingUnits,
  kInsufficientPoints,  // fewer than two points cannot be a chart
  kNonMonotonicTimestamps,
  kNonFiniteValue,
  kMissingCurrency,
  kMissingMerchant,
  kMissingSourceUrl,
  kNegativePrice,
  kInconsistentCurrency,
  kMissingLocation,
  kOutOfRangeValue,
  kEmptyResult,
};

const char* DataContractViolationToString(DataContractViolation violation);

// Provenance is well-formed: named source and both timestamps present.
DataContractViolation ValidateProvenance(const DataProvenance& provenance);

// Weather: location resolved, provenance valid, all probabilities/humidity in
// [0, 1], and at least one of current/hourly/daily present.
DataContractViolation ValidateWeatherReport(const WeatherReport& report);

// Quote: instrument identity complete, price finite/consistent currency,
// as_of set, provenance valid.
DataContractViolation ValidateMarketQuote(const MarketQuote& quote);

// Series chart eligibility: >= 2 bars, strictly increasing times, one
// currency, provenance valid. This is the gate the Canvas uses before it will
// render a price chart.
DataContractViolation ValidatePriceSeriesForChart(const PriceSeries& series);

// Offer: merchant, source URL, ISO-shaped currency, non-negative price,
// provenance valid.
DataContractViolation ValidateProductOffer(const ProductOffer& offer);

// History: provider-supplied only, >= 2 points when supplied, increasing
// times, consistent currency.
DataContractViolation ValidateProductPriceHistory(
    const ProductPriceHistory& history);

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_DATA_VALIDATION_H_
