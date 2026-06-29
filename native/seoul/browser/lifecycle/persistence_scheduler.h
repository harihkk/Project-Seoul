// Project Seoul native lifecycle bridge.
// Coalesces a burst of persist requests into a single posted write. There is no
// timer-based guessing here: a burst within one task turn collapses to one
// write on the next sequenced turn. Failures are observable, never corrupt the
// in-memory model, and never spin in a retry loop (a failed write stays pending
// and is retried only on the next genuine request). Shutdown supports one final
// synchronous flush.

#ifndef SEOUL_BROWSER_LIFECYCLE_PERSISTENCE_SCHEDULER_H_
#define SEOUL_BROWSER_LIFECYCLE_PERSISTENCE_SCHEDULER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"

namespace seoul {

class PersistenceScheduler {
 public:
  // `writer` performs the actual write and returns true on success.
  // `task_runner` receives the coalesced write task. Both required.
  PersistenceScheduler(base::RepeatingCallback<bool()> writer,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);
  PersistenceScheduler(const PersistenceScheduler&) = delete;
  PersistenceScheduler& operator=(const PersistenceScheduler&) = delete;
  ~PersistenceScheduler();

  // Request a write. Bursts coalesce into a single posted write.
  void ScheduleWrite();

  // Write now if dirty (e.g. shutdown). Returns true if a write occurred and
  // succeeded. Permitted exactly to flush remaining state at shutdown.
  bool Flush();

  // After shutdown, further ScheduleWrite calls are ignored (counted). Flush
  // still works for the final write.
  void Shutdown();

  bool has_pending() const { return dirty_; }
  int writes_committed() const { return committed_; }
  int writes_failed() const { return failed_; }
  int writes_skipped_after_shutdown() const { return skipped_after_shutdown_; }

 private:
  void DoScheduledWrite();

  base::RepeatingCallback<bool()> writer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool dirty_ = false;   // a write is needed
  bool posted_ = false;  // a coalesced task is already queued
  bool shutdown_ = false;

  int committed_ = 0;
  int failed_ = 0;
  int skipped_after_shutdown_ = 0;

  base::WeakPtrFactory<PersistenceScheduler> weak_factory_{this};
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_LIFECYCLE_PERSISTENCE_SCHEDULER_H_
