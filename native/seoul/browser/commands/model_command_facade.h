// Project Seoul outbound browser command layer.

#ifndef SEOUL_BROWSER_COMMANDS_MODEL_COMMAND_FACADE_H_
#define SEOUL_BROWSER_COMMANDS_MODEL_COMMAND_FACADE_H_

#include "base/memory/raw_ptr.h"
#include "seoul/browser/commands/browser_command.h"
#include "seoul/browser/commands/command_errors.h"
#include "seoul/browser/organization/organization_model.h"

namespace seoul {

class ModelCommandFacade {
 public:
  explicit ModelCommandFacade(OrganizationModel* model);
  ModelCommandFacade(const ModelCommandFacade&) = delete;
  ModelCommandFacade& operator=(const ModelCommandFacade&) = delete;

  CommandStatusResult Execute(const BrowserCommand& command);

 private:
  raw_ptr<OrganizationModel> model_;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_COMMANDS_MODEL_COMMAND_FACADE_H_
