// Project Seoul workspace projection engine V0.

#ifndef SEOUL_BROWSER_PROJECTION_PROJECTION_OBSERVER_H_
#define SEOUL_BROWSER_PROJECTION_PROJECTION_OBSERVER_H_

#include "base/observer_list_types.h"
#include "seoul/browser/projection/projection_types.h"

namespace seoul {

class ProjectionObserver : public base::CheckedObserver {
 public:
  ~ProjectionObserver() override = default;
  virtual void OnProjectionChanged(const ProjectionChange& change,
                                   const WindowProjection& projection) = 0;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_PROJECTION_OBSERVER_H_
