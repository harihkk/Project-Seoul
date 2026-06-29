// Project Seoul native organization engine.
// Observer of organization-model changes. Notifications are ordered and emitted
// exactly once per committed mutation. No notification is emitted for a
// mutation that failed or that committed no change.

#ifndef SEOUL_BROWSER_ORGANIZATION_ORGANIZATION_OBSERVER_H_
#define SEOUL_BROWSER_ORGANIZATION_ORGANIZATION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "seoul/browser/organization/organization_types.h"

namespace seoul {

class OrganizationModelObserver : public base::CheckedObserver {
 public:
  // Called once after a mutation has fully committed to in-memory state. The
  // model is in a consistent state when this runs. Observers must not re-enter
  // the model synchronously (reentrant mutations are rejected with
  // kNoOpRejected).
  virtual void OnOrganizationChanged(const OrganizationChange& change) {}

 protected:
  ~OrganizationModelObserver() override = default;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_ORGANIZATION_ORGANIZATION_OBSERVER_H_
