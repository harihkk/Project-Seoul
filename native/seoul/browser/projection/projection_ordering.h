// Project Seoul workspace projection engine V0.
// Translates projected workspace ordering to raw TabStripModel destinations.

#ifndef SEOUL_BROWSER_PROJECTION_PROJECTION_ORDERING_H_
#define SEOUL_BROWSER_PROJECTION_PROJECTION_ORDERING_H_

#include "seoul/browser/organization/organization_model.h"
#include "seoul/browser/projection/projection_types.h"

namespace seoul {

struct ProjectedMoveRequest {
  LiveTabKey tab;
  int projected_destination = -1;
};

struct ResolvedMoveDestination {
  LiveTabKey tab;
  int raw_destination_index = -1;
  bool crosses_pinned_boundary = false;
};

class ProjectionOrdering {
 public:
  // Computes the raw TabStripModel destination for a move among projected tabs.
  // Does not store indices; resolves from the supplied live state immediately.
  static bool ResolveMoveDestination(const WindowProjection& projection,
                                     const LiveWindowTabState& live,
                                     const ProjectedMoveRequest& request,
                                     ResolvedMoveDestination* out);

  // Chooses a deterministic fallback tab after closing `closed_tab` within the
  // projection. Never selects a tab outside the projection.
  static LiveTabKey ChooseCloseFallback(const WindowProjection& projection,
                                        LiveTabKey closed_tab);

  // Selects activation target when switching workspace per V0 policy.
  static LiveTabKey SelectSwitchTarget(
      const OrganizationModel& model,
      const WindowProjection& target_projection,
      std::string_view window_key,
      WorkspaceId target_workspace);
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_PROJECTION_ORDERING_H_
