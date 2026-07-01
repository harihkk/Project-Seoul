// Project Seoul grounded data layer.
// Unit tests for provider-result contract validation and chart eligibility.
// Fixture values below are deterministic test data only; production data comes
// exclusively from configured providers.

#include "seoul/browser/data/data_validation.h"

#include "base/time/time.h"
#include "seoul/browser/data/market_types.h"
#include "seoul/browser/data/product_types.h"
#include "seoul/browser/data/weather_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

base::Time TestTime(int minutes) {
  return base::Time::UnixEpoch() + base::Days(20000) + base::Minutes(minutes);
}

DataProvenance ValidProvenance() {
  DataProvenance p;
  p.source_name = "fixture-provider";
  p.source_url = "https://provider.test/data";
  p.retrieved_at = TestTime(10);
  p.effective_at = TestTime(9);
  p.timezone = "UTC";
  p.freshness = FreshnessState::kDelayed;
  return p;
}

DecimalAmount Usd(int64_t minor) {
  DecimalAmount a;
  a.amount_minor = minor;
  a.exponent = 2;
  a.currency_code = "USD";
  return a;
}

TEST(DataProvenanceTest, RequiresSourceAndTimestamps) {
  DataProvenance p = ValidProvenance();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kEligible);

  p.source_name.clear();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kMissingSource);

  p = ValidProvenance();
  p.effective_at = base::Time();
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kMissingTimestamps);

  p = ValidProvenance();
  p.completeness = 1.5;
  EXPECT_EQ(ValidateProvenance(p), DataContractViolation::kOutOfRangeValue);
}

TEST(WeatherValidationTest, EmptyReportIsNotRenderable) {
  WeatherReport report;
  report.location.display_name = "Test City";
  report.provenance = ValidProvenance();
  EXPECT_EQ(ValidateWeatherReport(report), DataContractViolation::kEmptyResult);
}

TEST(WeatherValidationTest, ValidReportPasses) {
  WeatherReport report;
  report.location.display_name = "Test City";
  report.provenance = ValidProvenance();
  WeatherObservation now;
  now.effective_at = TestTime(0);
  now.temperature_c = 21.5;
  now.precipitation_probability = 0.2;
  report.current = now;
  for (int i = 1; i <= 3; ++i) {
    WeatherObservation h = now;
    h.effective_at = TestTime(i * 60);
    report.hourly.push_back(h);
  }
  EXPECT_EQ(ValidateWeatherReport(report), DataContractViolation::kEligible);
}

TEST(WeatherValidationTest, RejectsOutOfOrderHourlyAndBadProbability) {
  WeatherReport report;
  report.location.display_name = "Test City";
  report.provenance = ValidProvenance();
  WeatherObservation a;
  a.effective_at = TestTime(120);
  WeatherObservation b;
  b.effective_at = TestTime(60);  // goes backwards
  report.hourly = {a, b};
  EXPECT_EQ(ValidateWeatherReport(report),
            DataContractViolation::kNonMonotonicTimestamps);

  report.hourly = {a};
  report.hourly[0].precipitation_probability = 1.7;
  EXPECT_EQ(ValidateWeatherReport(report),
            DataContractViolation::kOutOfRangeValue);
}

TEST(MarketValidationTest, QuoteRequiresIdentityCurrencyAndTime) {
  MarketQuote quote;
  quote.instrument.symbol = "TSTA";
  quote.instrument.exchange = "XTST";
  quote.instrument.currency_code = "USD";
  quote.last_price = Usd(1234500);
  quote.session_change = Usd(-2500);
  quote.session_change_percent = -0.2;
  quote.as_of = TestTime(5);
  quote.provenance = ValidProvenance();
  EXPECT_EQ(ValidateMarketQuote(quote), DataContractViolation::kEligible);

  MarketQuote bad = quote;
  bad.instrument.exchange.clear();
  EXPECT_EQ(ValidateMarketQuote(bad), DataContractViolation::kMissingSource);

  bad = quote;
  bad.last_price.currency_code = "usd";
  EXPECT_EQ(ValidateMarketQuote(bad), DataContractViolation::kMissingCurrency);

  bad = quote;
  bad.instrument.currency_code = "EUR";
  EXPECT_EQ(ValidateMarketQuote(bad),
            DataContractViolation::kInconsistentCurrency);

  bad = quote;
  bad.as_of = base::Time();
  EXPECT_EQ(ValidateMarketQuote(bad),
            DataContractViolation::kMissingTimestamps);
}

TEST(MarketValidationTest, SingleBarIsNeverAChart) {
  PriceSeries series;
  series.provenance = ValidProvenance();
  SeriesBar bar;
  bar.time = TestTime(0);
  bar.open = bar.high = bar.low = bar.close = Usd(100000);
  series.bars = {bar};
  EXPECT_EQ(ValidatePriceSeriesForChart(series),
            DataContractViolation::kInsufficientPoints);
}

TEST(MarketValidationTest, SeriesRejectsTimeAndCurrencyDefects) {
  PriceSeries series;
  series.provenance = ValidProvenance();
  SeriesBar a;
  a.time = TestTime(0);
  a.open = a.high = a.low = a.close = Usd(100000);
  SeriesBar b = a;
  b.time = TestTime(1);
  series.bars = {a, b};
  EXPECT_EQ(ValidatePriceSeriesForChart(series),
            DataContractViolation::kEligible);

  PriceSeries dup = series;
  dup.bars[1].time = dup.bars[0].time;  // duplicate timestamp
  EXPECT_EQ(ValidatePriceSeriesForChart(dup),
            DataContractViolation::kNonMonotonicTimestamps);

  PriceSeries mixed = series;
  mixed.bars[1].close.currency_code = "EUR";
  EXPECT_EQ(ValidatePriceSeriesForChart(mixed),
            DataContractViolation::kInconsistentCurrency);

  PriceSeries missing = series;
  missing.provenance.source_name.clear();
  EXPECT_EQ(ValidatePriceSeriesForChart(missing),
            DataContractViolation::kMissingSource);
}

TEST(ProductValidationTest, OfferRequiresMerchantUrlAndCurrency) {
  ProductOffer offer;
  offer.title = "Fixture Product";
  offer.merchant = "Fixture Store";
  offer.source_url = "https://store.test/item";
  offer.price = Usd(49900);
  offer.provenance = ValidProvenance();
  EXPECT_EQ(ValidateProductOffer(offer), DataContractViolation::kEligible);

  ProductOffer bad = offer;
  bad.merchant.clear();
  EXPECT_EQ(ValidateProductOffer(bad), DataContractViolation::kMissingMerchant);

  bad = offer;
  bad.source_url.clear();
  EXPECT_EQ(ValidateProductOffer(bad),
            DataContractViolation::kMissingSourceUrl);

  bad = offer;
  bad.price.amount_minor = -1;
  EXPECT_EQ(ValidateProductOffer(bad), DataContractViolation::kNegativePrice);

  bad = offer;
  bad.shipping = DecimalAmount();
  bad.shipping->currency_code = "EUR";
  EXPECT_EQ(ValidateProductOffer(bad),
            DataContractViolation::kInconsistentCurrency);

  bad = offer;
  RatingSummary rating;
  rating.value = 6.0;
  rating.scale = 5.0;
  rating.count = 10;
  bad.rating = rating;
  EXPECT_EQ(ValidateProductOffer(bad), DataContractViolation::kOutOfRangeValue);
}

TEST(ProductValidationTest, HistoryIsHonestAboutUnavailability) {
  ProductPriceHistory history;
  EXPECT_EQ(ValidateProductPriceHistory(history),
            DataContractViolation::kEligible);

  // Points on an "unavailable" history are a contract violation: history is
  // never synthesized.
  PriceHistoryPoint point;
  point.time = TestTime(0);
  point.price = Usd(100);
  history.points.push_back(point);
  EXPECT_EQ(ValidateProductPriceHistory(history),
            DataContractViolation::kOutOfRangeValue);

  history.availability = PriceHistoryAvailability::kProviderSupplied;
  history.provenance = ValidProvenance();
  EXPECT_EQ(ValidateProductPriceHistory(history),
            DataContractViolation::kInsufficientPoints);

  PriceHistoryPoint second;
  second.time = TestTime(60);
  second.price = Usd(90);
  history.points.push_back(second);
  EXPECT_EQ(ValidateProductPriceHistory(history),
            DataContractViolation::kEligible);
}

}  // namespace
}  // namespace seoul
