// Project Seoul grounded data layer.

#include "seoul/browser/data/data_validation.h"

#include <cmath>

namespace seoul {

namespace {

bool IsFinite(double v) {
  return std::isfinite(v);
}

bool CurrencyLooksValid(const std::string& code) {
  if (code.size() != 3) {
    return false;
  }
  for (char c : code) {
    if (c < 'A' || c > 'Z') {
      return false;
    }
  }
  return true;
}

bool OptionalFractionValid(const std::optional<double>& v) {
  return !v.has_value() || (IsFinite(*v) && *v >= 0.0 && *v <= 1.0);
}

}  // namespace

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
    case DataError::kUnknownLocation:
      return "unknown_location";
    case DataError::kUnknownInstrument:
      return "unknown_instrument";
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
    case DataContractViolation::kMissingUnits:
      return "missing_units";
    case DataContractViolation::kInsufficientPoints:
      return "insufficient_points";
    case DataContractViolation::kNonMonotonicTimestamps:
      return "non_monotonic_timestamps";
    case DataContractViolation::kNonFiniteValue:
      return "non_finite_value";
    case DataContractViolation::kMissingCurrency:
      return "missing_currency";
    case DataContractViolation::kMissingMerchant:
      return "missing_merchant";
    case DataContractViolation::kMissingSourceUrl:
      return "missing_source_url";
    case DataContractViolation::kNegativePrice:
      return "negative_price";
    case DataContractViolation::kInconsistentCurrency:
      return "inconsistent_currency";
    case DataContractViolation::kMissingLocation:
      return "missing_location";
    case DataContractViolation::kOutOfRangeValue:
      return "out_of_range_value";
    case DataContractViolation::kEmptyResult:
      return "empty_result";
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
  if (!IsFinite(provenance.completeness) || provenance.completeness < 0.0 ||
      provenance.completeness > 1.0) {
    return DataContractViolation::kOutOfRangeValue;
  }
  return DataContractViolation::kEligible;
}

DataContractViolation ValidateWeatherReport(const WeatherReport& report) {
  if (report.location.display_name.empty()) {
    return DataContractViolation::kMissingLocation;
  }
  if (auto v = ValidateProvenance(report.provenance);
      v != DataContractViolation::kEligible) {
    return v;
  }
  if (!report.current.has_value() && report.hourly.empty() &&
      report.daily.empty()) {
    return DataContractViolation::kEmptyResult;
  }
  auto check_observation = [](const WeatherObservation& o) {
    if (o.effective_at.is_null()) {
      return DataContractViolation::kMissingTimestamps;
    }
    if (o.temperature_c.has_value() && !IsFinite(*o.temperature_c)) {
      return DataContractViolation::kNonFiniteValue;
    }
    if (!OptionalFractionValid(o.precipitation_probability) ||
        !OptionalFractionValid(o.humidity)) {
      return DataContractViolation::kOutOfRangeValue;
    }
    return DataContractViolation::kEligible;
  };
  if (report.current.has_value()) {
    if (auto v = check_observation(*report.current);
        v != DataContractViolation::kEligible) {
      return v;
    }
  }
  base::Time last_hour;
  for (const WeatherObservation& o : report.hourly) {
    if (auto v = check_observation(o); v != DataContractViolation::kEligible) {
      return v;
    }
    if (!last_hour.is_null() && o.effective_at <= last_hour) {
      return DataContractViolation::kNonMonotonicTimestamps;
    }
    last_hour = o.effective_at;
  }
  base::Time last_day;
  for (const DailyForecast& d : report.daily) {
    if (d.date.is_null()) {
      return DataContractViolation::kMissingTimestamps;
    }
    if (!last_day.is_null() && d.date <= last_day) {
      return DataContractViolation::kNonMonotonicTimestamps;
    }
    if (!OptionalFractionValid(d.precipitation_probability)) {
      return DataContractViolation::kOutOfRangeValue;
    }
    last_day = d.date;
  }
  return DataContractViolation::kEligible;
}

DataContractViolation ValidateMarketQuote(const MarketQuote& quote) {
  if (quote.instrument.symbol.empty() || quote.instrument.exchange.empty()) {
    return DataContractViolation::kMissingSource;
  }
  if (!CurrencyLooksValid(quote.last_price.currency_code)) {
    return DataContractViolation::kMissingCurrency;
  }
  if (!quote.instrument.currency_code.empty() &&
      quote.instrument.currency_code != quote.last_price.currency_code) {
    return DataContractViolation::kInconsistentCurrency;
  }
  if (quote.last_price.amount_minor < 0) {
    return DataContractViolation::kNegativePrice;
  }
  if (!IsFinite(quote.session_change_percent)) {
    return DataContractViolation::kNonFiniteValue;
  }
  if (quote.as_of.is_null()) {
    return DataContractViolation::kMissingTimestamps;
  }
  return ValidateProvenance(quote.provenance);
}

DataContractViolation ValidatePriceSeriesForChart(const PriceSeries& series) {
  if (auto v = ValidateProvenance(series.provenance);
      v != DataContractViolation::kEligible) {
    return v;
  }
  if (series.bars.size() < 2) {
    return DataContractViolation::kInsufficientPoints;
  }
  const std::string& currency = series.bars.front().close.currency_code;
  if (!CurrencyLooksValid(currency)) {
    return DataContractViolation::kMissingCurrency;
  }
  base::Time last;
  for (const SeriesBar& bar : series.bars) {
    if (bar.time.is_null()) {
      return DataContractViolation::kMissingTimestamps;
    }
    if (!last.is_null() && bar.time <= last) {
      return DataContractViolation::kNonMonotonicTimestamps;
    }
    last = bar.time;
    for (const DecimalAmount* amount :
         {&bar.open, &bar.high, &bar.low, &bar.close}) {
      if (amount->currency_code != currency) {
        return DataContractViolation::kInconsistentCurrency;
      }
      if (amount->amount_minor < 0) {
        return DataContractViolation::kNegativePrice;
      }
    }
  }
  return DataContractViolation::kEligible;
}

DataContractViolation ValidateProductOffer(const ProductOffer& offer) {
  if (offer.merchant.empty()) {
    return DataContractViolation::kMissingMerchant;
  }
  if (offer.source_url.empty()) {
    return DataContractViolation::kMissingSourceUrl;
  }
  if (!CurrencyLooksValid(offer.price.currency_code)) {
    return DataContractViolation::kMissingCurrency;
  }
  if (offer.price.amount_minor < 0) {
    return DataContractViolation::kNegativePrice;
  }
  if (offer.shipping.has_value() &&
      offer.shipping->currency_code != offer.price.currency_code) {
    return DataContractViolation::kInconsistentCurrency;
  }
  if (offer.rating.has_value()) {
    const RatingSummary& r = *offer.rating;
    if (!IsFinite(r.value) || !IsFinite(r.scale) || r.scale <= 0.0 ||
        r.value < 0.0 || r.value > r.scale || r.count < 0) {
      return DataContractViolation::kOutOfRangeValue;
    }
  }
  return ValidateProvenance(offer.provenance);
}

DataContractViolation ValidateProductPriceHistory(
    const ProductPriceHistory& history) {
  if (history.availability == PriceHistoryAvailability::kHistoryUnavailable) {
    // Unavailable history must be empty; an empty result is the honest state.
    return history.points.empty() ? DataContractViolation::kEligible
                                  : DataContractViolation::kOutOfRangeValue;
  }
  if (auto v = ValidateProvenance(history.provenance);
      v != DataContractViolation::kEligible) {
    return v;
  }
  if (history.points.size() < 2) {
    return DataContractViolation::kInsufficientPoints;
  }
  const std::string& currency = history.points.front().price.currency_code;
  if (!CurrencyLooksValid(currency)) {
    return DataContractViolation::kMissingCurrency;
  }
  base::Time last;
  for (const PriceHistoryPoint& point : history.points) {
    if (point.time.is_null()) {
      return DataContractViolation::kMissingTimestamps;
    }
    if (!last.is_null() && point.time <= last) {
      return DataContractViolation::kNonMonotonicTimestamps;
    }
    if (point.price.currency_code != currency) {
      return DataContractViolation::kInconsistentCurrency;
    }
    if (point.price.amount_minor < 0) {
      return DataContractViolation::kNegativePrice;
    }
    last = point.time;
  }
  return DataContractViolation::kEligible;
}

}  // namespace seoul
