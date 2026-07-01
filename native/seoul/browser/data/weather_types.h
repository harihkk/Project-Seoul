// Project Seoul grounded data layer.
// Provider-neutral weather contract. A configured WeatherProvider fills these
// records from a real structured source. Without a provider the weather tool
// reports kNoProvider; no forecast is ever fabricated or approximated.

#ifndef SEOUL_BROWSER_DATA_WEATHER_TYPES_H_
#define SEOUL_BROWSER_DATA_WEATHER_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "seoul/browser/data/provenance.h"

namespace seoul {

// Normalized condition classes. Providers map their own condition codes onto
// these; kUnknown is legitimate and rendered as such.
enum class WeatherCondition {
  kUnknown,
  kClear,
  kPartlyCloudy,
  kCloudy,
  kFog,
  kDrizzle,
  kRain,
  kSnow,
  kSleet,
  kThunderstorm,
  kWindy,
};

enum class TemperatureUnit { kCelsius, kFahrenheit };

struct WeatherLocation {
  std::string display_name;  // resolved place name; never empty on success
  double latitude = 0.0;
  double longitude = 0.0;

  friend bool operator==(const WeatherLocation&,
                         const WeatherLocation&) = default;
};

struct WeatherObservation {
  base::Time effective_at;
  WeatherCondition condition = WeatherCondition::kUnknown;
  std::optional<double> temperature_c;
  std::optional<double> feels_like_c;
  std::optional<double> precipitation_probability;  // [0.0, 1.0]
  std::optional<double> precipitation_mm;
  std::optional<double> wind_speed_kph;
  std::optional<double> wind_direction_degrees;
  std::optional<double> humidity;  // [0.0, 1.0]

  friend bool operator==(const WeatherObservation&,
                         const WeatherObservation&) = default;
};

struct DailyForecast {
  base::Time date;
  WeatherCondition condition = WeatherCondition::kUnknown;
  std::optional<double> high_c;
  std::optional<double> low_c;
  std::optional<double> precipitation_probability;
  std::optional<base::Time> sunrise;
  std::optional<base::Time> sunset;

  friend bool operator==(const DailyForecast&, const DailyForecast&) = default;
};

struct WeatherAlert {
  std::string title;
  std::string severity;  // provider severity label, displayed verbatim
  std::string source;
  base::Time effective_from;
  base::Time effective_until;

  friend bool operator==(const WeatherAlert&, const WeatherAlert&) = default;
};

// One complete provider response. `hourly` and `daily` may be empty when the
// provider does not supply them; missing fields stay unset and the surface
// renders explicit gaps instead of substitutes.
struct WeatherReport {
  WeatherLocation location;
  std::optional<WeatherObservation> current;
  std::vector<WeatherObservation> hourly;
  std::vector<DailyForecast> daily;
  std::vector<WeatherAlert> alerts;
  DataProvenance provenance;

  friend bool operator==(const WeatherReport&, const WeatherReport&) = default;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_WEATHER_TYPES_H_
