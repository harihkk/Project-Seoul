// Project Seoul general-purpose operator: task layer.

#include "seoul/browser/tasks/task_types.h"

namespace seoul {

TaskId::TaskId() = default;
TaskId::TaskId(const TaskId&) = default;
TaskId::TaskId(TaskId&&) = default;
TaskId& TaskId::operator=(const TaskId&) = default;
TaskId& TaskId::operator=(TaskId&&) = default;
TaskId::~TaskId() = default;

VerificationRecord::VerificationRecord() = default;
VerificationRecord::VerificationRecord(const VerificationRecord&) = default;
VerificationRecord::VerificationRecord(VerificationRecord&&) = default;
VerificationRecord& VerificationRecord::operator=(const VerificationRecord&) =
    default;
VerificationRecord& VerificationRecord::operator=(VerificationRecord&&) =
    default;
VerificationRecord::~VerificationRecord() = default;

ActionReceipt::ActionReceipt() = default;
ActionReceipt::ActionReceipt(const ActionReceipt&) = default;
ActionReceipt::ActionReceipt(ActionReceipt&&) = default;
ActionReceipt& ActionReceipt::operator=(const ActionReceipt&) = default;
ActionReceipt& ActionReceipt::operator=(ActionReceipt&&) = default;
ActionReceipt::~ActionReceipt() = default;

}  // namespace seoul
