// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/projection_ordering.h"

#include <algorithm>
#include <map>

namespace seoul {

bool ProjectionOrdering::ResolveMoveDestination(
    const WindowProjection& projection,
    const LiveWindowTabState& live,
    const ProjectedMoveRequest& request,
    ResolvedMoveDestination* out) {
  if (!out || !request.tab.is_valid() || request.projected_destination < 0) {
    return false;
  }
  int source_projected = -1;
  for (const ProjectedTab& pt : projection.tabs) {
    if (pt.tab == request.tab) {
      source_projected = pt.projected_order;
      break;
    }
  }
  if (source_projected < 0) {
    return false;
  }
  if (request.projected_destination >=
      static_cast<int>(projection.tabs.size())) {
    return false;
  }

  const ProjectedTab& dest_tab = projection.tabs[request.projected_destination];
  std::map<LiveTabKey, int> strip_order;
  for (const LiveTabDescriptor& tab : live.tabs) {
    strip_order[tab.tab] = tab.strip_order;
  }
  auto src_it = strip_order.find(request.tab);
  auto dst_it = strip_order.find(dest_tab.tab);
  if (src_it == strip_order.end() || dst_it == strip_order.end()) {
    return false;
  }

  out->tab = request.tab;
  out->raw_destination_index = dst_it->second;
  bool src_pinned = false;
  bool dst_pinned = false;
  for (const LiveTabDescriptor& tab : live.tabs) {
    if (tab.tab == request.tab) {
      src_pinned = tab.chromium_pinned;
    }
    if (tab.tab == dest_tab.tab) {
      dst_pinned = tab.chromium_pinned;
    }
  }
  out->crosses_pinned_boundary = src_pinned != dst_pinned;
  if (out->crosses_pinned_boundary) {
    return false;
  }
  return true;
}

LiveTabKey ProjectionOrdering::ChooseCloseFallback(
    const WindowProjection& projection,
    LiveTabKey closed_tab) {
  if (projection.tabs.empty()) {
    return LiveTabKey();
  }
  int closed_order = -1;
  for (const ProjectedTab& pt : projection.tabs) {
    if (pt.tab == closed_tab) {
      closed_order = pt.projected_order;
      break;
    }
  }
  if (closed_order < 0) {
    return projection.tabs.front().tab;
  }
  for (const ProjectedTab& pt : projection.tabs) {
    if (pt.projected_order == closed_order + 1) {
      return pt.tab;
    }
  }
  for (const ProjectedTab& pt : projection.tabs) {
    if (pt.projected_order == closed_order - 1) {
      return pt.tab;
    }
  }
  return projection.tabs.front().tab;
}

LiveTabKey ProjectionOrdering::SelectSwitchTarget(
    const OrganizationModel& model,
    const WindowProjection& target_projection,
    std::string_view window_key,
    WorkspaceId target_workspace) {
  (void)target_workspace;
  if (!target_projection.tabs.empty() &&
      target_projection.active_tab.is_valid()) {
    for (const ProjectedTab& pt : target_projection.tabs) {
      if (pt.tab == target_projection.active_tab) {
        return pt.tab;
      }
    }
  }
  for (const ProjectedTab& pt : target_projection.tabs) {
    if (pt.role == TabRole::kPinned) {
      return pt.tab;
    }
  }
  for (const ProjectedTab& pt : target_projection.tabs) {
    if (pt.role == TabRole::kRetained) {
      return pt.tab;
    }
  }
  for (const ProjectedTab& pt : target_projection.tabs) {
    if (pt.role == TabRole::kTemporary) {
      return pt.tab;
    }
  }
  (void)window_key;
  (void)model;
  return LiveTabKey();
}

}  // namespace seoul
