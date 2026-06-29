// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_URL_POLICY_H_
#define SEOUL_BROWSER_COMMANDS_URL_POLICY_H_

#include "seoul/browser/commands/command_errors.h"
#include "url/gurl.h"

namespace seoul {

class UrlPolicy {
 public:
  static CommandStatusResult ValidateNavigationUrl(const GURL& url);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_URL_POLICY_H_
