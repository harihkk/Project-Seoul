// Project Seoul workspace projection engine V0.
// Pure, deterministic projection calculator. No Chromium dependencies.

#ifndef SEOUL_BROWSER_PROJECTION_PROJECTION_CALCULATOR_H_
#define SEOUL_BROWSER_PROJECTION_PROJECTION_CALCULATOR_H_

#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/projection/projection_types.h"

namespace seoul {

class ProjectionCalculator {
 public:
  static WindowProjection Compute(const OrganizationModel& model,
                                  const LiveWindowTabState& live,
                                  WorkspaceId active_workspace,
                                  ProjectionGeneration generation,
                                  bool fail_open = false);

 private:
  static bool TabBelongsToWorkspace(const OrganizationModel& model,
                                    const LiveTabDescriptor& tab,
                                    const WorkspaceId& workspace);
  static int RoleSortKey(TabRole role);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_PROJECTION_CALCULATOR_H_
