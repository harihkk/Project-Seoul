// Project Seoul native lifecycle bridge.

#include "seoul/browser/lifecycle/persistence_scheduler.h"

#include <utility>

#include "base/functional/bind.h"

namespace seoul {

PersistenceScheduler::PersistenceScheduler(
    base::RepeatingCallback<bool()> writer,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : writer_(std::move(writer)), task_runner_(std::move(task_runner)) {}

PersistenceScheduler::~PersistenceScheduler() = default;

void PersistenceScheduler::ScheduleWrite() {
  if (shutdown_) {
    ++skipped_after_shutdown_;
    return;
  }
  dirty_ = true;
  if (posted_) {
    return;  // A coalesced write is already queued; the burst collapses into
             // it.
  }
  posted_ = true;
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&PersistenceScheduler::DoScheduledWrite,
                                        weak_factory_.GetWeakPtr()));
}

void PersistenceScheduler::DoScheduledWrite() {
  posted_ = false;
  if (!dirty_) {
    return;
  }
  // Clear dirty before writing so a request arriving during the write is not
  // lost; on failure we re-mark dirty (bounded: retried only on a later
  // request).
  dirty_ = false;
  if (writer_.Run()) {
    ++committed_;
  } else {
    ++failed_;
    dirty_ =
        true;  // Keep the pending state; do not spin, do not corrupt memory.
  }
}

bool PersistenceScheduler::Flush() {
  if (!dirty_) {
    return false;
  }
  dirty_ = false;
  if (writer_.Run()) {
    ++committed_;
    return true;
  }
  ++failed_;
  dirty_ = true;
  return false;
}

void PersistenceScheduler::Shutdown() {
  shutdown_ = true;
}

}  // namespace seoul
