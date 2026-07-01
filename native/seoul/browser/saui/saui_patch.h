// Project Seoul Adaptive UI (SAUI).
// Typed incremental surface updates. Live surfaces change through bounded,
// validated patch operations addressed by stable component and data-entry ids;
// the whole Canvas is never rebuilt for a minor update. A patch applies
// atomically: if any operation or the resulting surface is invalid, the
// surface is left untouched.

#ifndef SEOUL_BROWSER_SAUI_SAUI_PATCH_H_
#define SEOUL_BROWSER_SAUI_SAUI_PATCH_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "seoul/browser/saui/saui_errors.h"
#include "seoul/browser/saui/saui_types.h"

namespace seoul {

enum class PatchOpKind {
  kSetProps,            // merge validated props into a component
  kSetState,            // set component state (+ optional message)
  kSetTitle,            // set the surface title
  kUpsertDataEntry,     // insert or replace one data entry
  kAppendSeriesPoints,  // append points to an existing series entry
  kAppendChild,         // append a component under a container
  kRemoveComponent,     // remove a component (and its subtree)
  kReplaceComponent,    // replace a component in place
  kSetActions,          // replace the surface action list
};

// Holds a move-only base::Value::Dict (props) plus copyable SAUI parts, so it
// declares clone-based copy semantics (defined in saui_patch.cc).
struct SurfacePatchOp {
  SurfacePatchOp();
  SurfacePatchOp(const SurfacePatchOp&);
  SurfacePatchOp(SurfacePatchOp&&);
  SurfacePatchOp& operator=(const SurfacePatchOp&);
  SurfacePatchOp& operator=(SurfacePatchOp&&);
  ~SurfacePatchOp();

  PatchOpKind kind = PatchOpKind::kSetProps;
  std::string target_component;                   // component ops
  std::string entry_name;                         // data ops
  base::Value::Dict props;                        // kSetProps
  ComponentState state = ComponentState::kReady;  // kSetState
  std::string state_message;                      // kSetState
  std::string title;                              // kSetTitle
  DataEntry entry;                                // kUpsertDataEntry
  std::vector<SeriesPoint> points;                // kAppendSeriesPoints
  ComponentNode component;             // kAppendChild / kReplaceComponent
  std::vector<SurfaceAction> actions;  // kSetActions
};

struct SurfacePatch {
  SurfaceId surface_id;
  std::vector<SurfacePatchOp> ops;
};

// The renderer consumes this to update only what changed.
struct AppliedPatch {
  uint64_t new_revision = 0;
  std::vector<std::string> changed_component_ids;
  std::vector<std::string> changed_entry_names;
  bool title_changed = false;
  bool actions_changed = false;
};

// Parses an untrusted patch document (same trust level as ParseSurface).
SauiResult<SurfacePatch> ParseSurfacePatch(const base::Value& value);

// Applies `patch` to `surface` atomically. On success the surface revision is
// bumped once and the change summary is returned; on failure the surface is
// unchanged.
SauiResult<AppliedPatch> ApplySurfacePatch(AdaptiveSurface& surface,
                                           const SurfacePatch& patch);

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_PATCH_H_
