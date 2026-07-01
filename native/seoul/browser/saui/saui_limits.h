// Project Seoul Adaptive UI (SAUI).
// Bounded limits for adaptive surface documents. Documents come from an
// untrusted generator (a model); every collection and string is capped before
// any content is interpreted, so a hostile document cannot exhaust memory or
// stack.

#ifndef SEOUL_BROWSER_SAUI_SAUI_LIMITS_H_
#define SEOUL_BROWSER_SAUI_SAUI_LIMITS_H_

#include <cstddef>

namespace seoul {

// Bump only with an explicit migration; unknown higher versions are rejected.
inline constexpr int kSauiSchemaVersion = 1;

inline constexpr size_t kMaxSurfaceComponents = 512;
inline constexpr size_t kMaxComponentDepth = 12;
inline constexpr size_t kMaxChildrenPerComponent = 64;
inline constexpr size_t kMaxSurfaceActions = 64;
inline constexpr size_t kMaxActionsPerComponent = 16;
inline constexpr size_t kMaxDataEntries = 128;
inline constexpr size_t kMaxSeriesPoints = 10000;
inline constexpr size_t kMaxTableRows = 2000;
inline constexpr size_t kMaxTableColumns = 64;
inline constexpr size_t kMaxRecordFields = 128;
inline constexpr size_t kMaxPropsPerComponent = 48;
inline constexpr size_t kMaxBindingsPerComponent = 8;

inline constexpr size_t kMaxComponentIdLength = 64;
inline constexpr size_t kMaxDataEntryNameLength = 64;
inline constexpr size_t kMaxPropKeyLength = 40;
inline constexpr size_t kMaxPropStringLength = 4096;
// Larger cap for source-code-carrying props ("code", "diff") only.
inline constexpr size_t kMaxCodePropLength = 65536;
inline constexpr size_t kMaxTitleLength = 200;
inline constexpr size_t kMaxLabelLength = 200;
inline constexpr size_t kMaxAccessibleNameLength = 400;
inline constexpr size_t kMaxStructuredListItems = 64;
inline constexpr size_t kMaxUrlPropLength = 4096;

inline constexpr size_t kMaxPatchOps = 128;

// Pie charts are only appropriate for a small number of categories.
inline constexpr size_t kMaxPieSlices = 12;

}  // namespace seoul

#endif  // SEOUL_BROWSER_SAUI_SAUI_LIMITS_H_
