// Project Seoul grounded data layer.
// Shopping provider seam. Implementations adapt a configured shopping source,
// a structured search result, or a verified merchant-page extraction to the
// neutral offer contract.

#ifndef SEOUL_BROWSER_DATA_SHOPPING_PROVIDER_H_
#define SEOUL_BROWSER_DATA_SHOPPING_PROVIDER_H_

#include <string>

#include "base/functional/callback.h"
#include "seoul/browser/data/data_errors.h"
#include "seoul/browser/data/product_types.h"

namespace seoul {

struct ProductSearchRequest {
  std::string query;
  int max_offers = 20;  // bounded by the provider as well
};

class ShoppingProvider {
 public:
  virtual ~ShoppingProvider() = default;

  virtual std::string provider_name() const = 0;
  virtual bool requires_network() const = 0;

  using SearchCallback =
      base::OnceCallback<void(DataResult<ProductSearchResult> result)>;
  virtual void Search(const ProductSearchRequest& request,
                      SearchCallback callback) = 0;

  using HistoryCallback =
      base::OnceCallback<void(DataResult<ProductPriceHistory> result)>;
  // Providers without real history respond with availability
  // kHistoryUnavailable; callers render "history unavailable".
  virtual void FetchPriceHistory(const ProductOffer& offer,
                                 HistoryCallback callback) = 0;

  virtual void Cancel() = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_DATA_SHOPPING_PROVIDER_H_
