// Project Seoul product runtime.
// Canonical JSON wire codec for task snapshots, defined by
// protocol/task-snapshot.schema.json. This is the typed document the Canvas
// renders task progress from (replacing free-form status strings) and the
// shape shared with the TypeScript Design Lab receipts UI.
// ParseTaskSnapshot(TaskSnapshotToValue(s)) reproduces `s`.

#ifndef SEOUL_BROWSER_PRODUCT_TASK_SNAPSHOT_WIRE_H_
#define SEOUL_BROWSER_PRODUCT_TASK_SNAPSHOT_WIRE_H_

#include <string>

#include "base/types/expected.h"
#include "base/values.h"
#include "seoul/browser/product/task_service.h"

namespace seoul {

// Wire-name mappings, exposed for the protocol parity gate and tests.
const char* PlanOriginToWire(PlanOrigin origin);
bool PlanOriginFromWire(std::string_view s, PlanOrigin* out);
bool TaskStateFromWire(std::string_view s, TaskState* out);
bool StepStatusFromWire(std::string_view s, StepStatus* out);
bool ExecutionRouteFromWire(std::string_view s, ExecutionRoute* out);
bool TaskFailureReasonFromWire(std::string_view s, TaskFailureReason* out);

base::DictValue TaskSnapshotToValue(const TaskSnapshot& snapshot);

// Parses one untrusted canonical snapshot document; the error is a precise
// human-readable reason.
base::expected<TaskSnapshot, std::string> ParseTaskSnapshot(
    const base::Value& value);

}  // namespace seoul

#endif  // SEOUL_BROWSER_PRODUCT_TASK_SNAPSHOT_WIRE_H_
