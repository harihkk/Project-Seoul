// Project Seoul connected tools.
// Generic production capability seams: search, bounded web extraction, and
// the browser capability catalog over the native command layer. Each seam
// returns generic semantic results; none of them knows a business domain.
// Concrete providers (a configured search engine adapter, the page
// extraction engine, the command dispatcher glue) implement these interfaces
// in the runtime layer.

#ifndef SEOUL_BROWSER_CONNECTORS_GENERIC_CAPABILITIES_H_
#define SEOUL_BROWSER_CONNECTORS_GENERIC_CAPABILITIES_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "seoul/browser/data/data_errors.h"
#include "seoul/browser/semantic/semantic_types.h"
#include "seoul/browser/tools/tool_types.h"

namespace seoul {

// Generic cited search. The result is a kCitations/kEntityCollection
// semantic result with provenance; ranking and sources belong to the
// configured provider.
class GenericSearchProvider {
 public:
  virtual ~GenericSearchProvider() = default;

  virtual std::string provider_name() const = 0;

  using SearchCallback =
      base::OnceCallback<void(base::expected<SemanticResult, DataError>)>;
  virtual void Search(const std::string& query,
                      int max_results,
                      SearchCallback callback) = 0;
  virtual void Cancel() = 0;
};

// Bounded structured extraction from a user-visible page. The caller supplies
// the semantic schema it wants filled; the extractor returns rows conforming
// to it (or a precise error), never fabricating fields it cannot find.
class WebExtractionProvider {
 public:
  virtual ~WebExtractionProvider() = default;

  using ExtractCallback =
      base::OnceCallback<void(base::expected<SemanticResult, DataError>)>;
  // `tab_key` is the stable live-tab key from the lifecycle layer.
  virtual void ExtractStructured(const std::string& tab_key,
                                 const SemanticSchema& wanted,
                                 ExtractCallback callback) = 0;
  virtual void Cancel() = 0;
};

// Capability descriptors for the generic information seams (info.search.web,
// page.extract.structured). Provider "seoul".
std::vector<ToolDescriptor> BuildInformationCapabilities();

// The browser capability catalog: typed operations over the native command
// layer (open/activate/close/move tabs, switch workspaces, activate scenes,
// observe page text, splits, archive). Ids live under browser.* and page.*;
// dispatch is implemented by the runtime over ModelCommandFacade /
// CommandExecutor. The catalog carries honest risk metadata so plan
// validation gates mutations correctly.
std::vector<ToolDescriptor> BuildBrowserCapabilities();

}  // namespace seoul

#endif  // SEOUL_BROWSER_CONNECTORS_GENERIC_CAPABILITIES_H_
