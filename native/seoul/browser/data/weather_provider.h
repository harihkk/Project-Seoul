// Project Seoul grounded data layer.
// Weather provider seam. Implementations adapt one structured source (a
// configured weather API or a browser-search extraction path) to the neutral
// WeatherReport contract. Core logic never depends on a specific vendor.

#ifndef SEOUL_BROWSER_DATA_WEATHER_PROVIDER_H_
#define SEOUL_BROWSER_DATA_WEATHER_PROVIDER_H_

#include <string>

#include "base/functional/callback.h"
#include "seoul/browser/data/data_errors.h"
#include "seoul/browser/data/weather_types.h"

namespace seoul {

struct WeatherRequest {
  // Free-text place query ("seoul", "portland oregon") or empty when
  // coordinates are given.
  std::string place_query;
  bool has_coordinates = false;
  double latitude = 0.0;
  double longitude = 0.0;
  bool include_hourly = true;
  bool include_daily = true;
};

class WeatherProvider {
 public:
  virtual ~WeatherProvider() = default;

  virtual std::string provider_name() const = 0;
  // True when the provider performs network requests (routing and budget
  // enforcement need this; a cached-only provider returns false).
  virtual bool requires_network() const = 0;

  using FetchCallback =
      base::OnceCallback<void(DataResult<WeatherReport> result)>;
  virtual void Fetch(const WeatherRequest& request, FetchCallback callback) = 0;
  virtual void Cancel() = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_WEATHER_PROVIDER_H_
