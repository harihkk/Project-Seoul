// Project Seoul grounded data layer.
// Provider-neutral product offer contract. Every price is tied to a merchant,
// currency, timestamp, and source URL. Variants are distinct offers and are
// never merged; price history exists only when the provider supplies real
// history (kHistoryUnavailable otherwise, never synthesized).

#ifndef SEOUL_BROWSER_DATA_PRODUCT_TYPES_H_
#define SEOUL_BROWSER_DATA_PRODUCT_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "seoul/browser/data/market_types.h"
#include "seoul/browser/data/provenance.h"

namespace seoul {

enum class StockState {
  kUnknown,
  kInStock,
  kLimited,
  kOutOfStock,
  kPreorder,
};

struct RatingSummary {
  double value = 0.0;  // in [0, scale]
  double scale = 5.0;  // rating scale maximum
  int64_t count = 0;   // number of ratings behind the value
  std::string source;  // where the rating comes from; displayed with it

  friend bool operator==(const RatingSummary&, const RatingSummary&) = default;
};

struct ProductOffer {
  std::string title;
  std::string merchant;    // selling merchant; never empty on success
  std::string source_url;  // offer page; never empty on success
  std::string variant;     // color/size/model descriptor; may be empty
  DecimalAmount price;
  std::optional<DecimalAmount> shipping;  // unset when unknown, not zero
  StockState stock = StockState::kUnknown;
  std::optional<base::Time> delivery_estimate;
  std::optional<RatingSummary> rating;
  DataProvenance provenance;

  friend bool operator==(const ProductOffer&, const ProductOffer&) = default;
};

enum class PriceHistoryAvailability {
  kHistoryUnavailable,  // provider supplies no history; none is invented
  kProviderSupplied,
};

struct PriceHistoryPoint {
  base::Time time;
  DecimalAmount price;

  friend bool operator==(const PriceHistoryPoint&,
                         const PriceHistoryPoint&) = default;
};

struct ProductPriceHistory {
  PriceHistoryAvailability availability =
      PriceHistoryAvailability::kHistoryUnavailable;
  std::vector<PriceHistoryPoint> points;  // empty unless kProviderSupplied
  DataProvenance provenance;

  friend bool operator==(const ProductPriceHistory&,
                         const ProductPriceHistory&) = default;
};

struct ProductSearchResult {
  std::string query;
  std::vector<ProductOffer> offers;
  DataProvenance provenance;

  friend bool operator==(const ProductSearchResult&,
                         const ProductSearchResult&) = default;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_PRODUCT_TYPES_H_
