// Project Seoul grounded data layer.
// Market data provider seam. Implementations adapt one official data source to
// the neutral quote/series contract. Market surfaces carry an informational
// notice; nothing in this layer produces advice or model-generated prices.

#ifndef SEOUL_BROWSER_DATA_MARKET_DATA_PROVIDER_H_
#define SEOUL_BROWSER_DATA_MARKET_DATA_PROVIDER_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "seoul/browser/data/data_errors.h"
#include "seoul/browser/data/market_types.h"

namespace seoul {

struct InstrumentQuery {
  std::string text;  // symbol or free-text name to resolve
};

struct SeriesRequest {
  InstrumentIdentity instrument;
  SeriesInterval interval = SeriesInterval::kDay;
  base::Time from;
  base::Time to;
};

class MarketDataProvider {
 public:
  virtual ~MarketDataProvider() = default;

  virtual std::string provider_name() const = 0;
  virtual bool requires_network() const = 0;

  using ResolveCallback = base::OnceCallback<void(
      DataResult<std::vector<InstrumentIdentity>> result)>;
  virtual void ResolveInstrument(const InstrumentQuery& query,
                                 ResolveCallback callback) = 0;

  using QuoteCallback =
      base::OnceCallback<void(DataResult<MarketQuote> result)>;
  virtual void FetchQuote(const InstrumentIdentity& instrument,
                          QuoteCallback callback) = 0;

  using SeriesCallback =
      base::OnceCallback<void(DataResult<PriceSeries> result)>;
  virtual void FetchSeries(const SeriesRequest& request,
                           SeriesCallback callback) = 0;

  virtual void Cancel() = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_MARKET_DATA_PROVIDER_H_
