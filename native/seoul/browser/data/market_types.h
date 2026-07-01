// Project Seoul grounded data layer.
// Provider-neutral market data contract. Quotes and series carry explicit
// exchange, currency, timing, and delay state; a delayed quote is never
// presented as live and model-generated prices are structurally impossible
// (only a registered MarketDataProvider can produce these records).

#ifndef SEOUL_BROWSER_DATA_MARKET_TYPES_H_
#define SEOUL_BROWSER_DATA_MARKET_TYPES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "seoul/browser/data/provenance.h"

namespace seoul {

enum class MarketStatus {
  kUnknown,
  kOpen,
  kClosed,
  kPreMarket,
  kPostMarket,
  kHalted,
};

enum class SeriesInterval {
  kMinute,
  kFiveMinutes,
  kHour,
  kDay,
  kWeek,
  kMonth,
};

// Corporate-action adjustment state of a historical series.
enum class SeriesAdjustment {
  kUnadjusted,
  kSplitAdjusted,
  kSplitAndDividendAdjusted,
};

// Exact decimal money/price value: `amount_minor / 10^exponent` in
// `currency_code`. Stored as integers so no float rounding enters price data.
struct DecimalAmount {
  int64_t amount_minor = 0;
  int32_t exponent = 2;       // digits after the decimal point
  std::string currency_code;  // ISO 4217 alpha code, for example "USD"

  friend bool operator==(const DecimalAmount&, const DecimalAmount&) = default;
};

struct InstrumentIdentity {
  std::string symbol;    // provider-resolved ticker, never a guess
  std::string exchange;  // listing exchange code
  std::string name;      // display name
  std::string currency_code;

  friend bool operator==(const InstrumentIdentity&,
                         const InstrumentIdentity&) = default;
};

struct MarketQuote {
  InstrumentIdentity instrument;
  DecimalAmount last_price;
  DecimalAmount session_change;  // signed change vs previous close
  double session_change_percent = 0.0;
  MarketStatus market_status = MarketStatus::kUnknown;
  base::Time as_of;       // quote time from the provider
  bool delayed = true;    // defaults to delayed; providers must opt in to live
  base::TimeDelta delay;  // provider-declared delay when `delayed`
  DataProvenance provenance;

  friend bool operator==(const MarketQuote&, const MarketQuote&) = default;
};

struct SeriesBar {
  base::Time time;
  DecimalAmount open;
  DecimalAmount high;
  DecimalAmount low;
  DecimalAmount close;
  int64_t volume = 0;

  friend bool operator==(const SeriesBar&, const SeriesBar&) = default;
};

struct PriceSeries {
  InstrumentIdentity instrument;
  SeriesInterval interval = SeriesInterval::kDay;
  SeriesAdjustment adjustment = SeriesAdjustment::kUnadjusted;
  std::vector<SeriesBar> bars;  // strictly increasing by time
  DataProvenance provenance;

  friend bool operator==(const PriceSeries&, const PriceSeries&) = default;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_MARKET_TYPES_H_
