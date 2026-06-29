// Project Seoul workspace projection engine V0.
// Chromium-facing vertical tab presentation filtering.

#ifndef SEOUL_BROWSER_PROJECTION_VERTICAL_PRESENTATION_ADAPTER_H_
#define SEOUL_BROWSER_PROJECTION_VERTICAL_PRESENTATION_ADAPTER_H_

#include "seoul/browser/projection/projection_types.h"
#include "seoul/browser/projection/vertical_presentation_filter.h"

class VerticalTabStripRegionView;
namespace views {
class View;
}

namespace seoul {

class VerticalPresentationAdapter {
 public:
  VerticalPresentationAdapter();
  ~VerticalPresentationAdapter();

  void UpdateProjection(const WindowProjection& projection);
  void SetDisabled(bool disabled);
  void EnterFailOpen();
  void ClearFailOpen();

  void ApplyToVerticalTabStripRegion(VerticalTabStripRegionView* region);

  static views::View* FindDefaultFocusableChild(
      VerticalTabStripRegionView* region,
      const WindowProjection& projection);

 private:
  VerticalPresentationFilter filter_;
  bool fail_open_ = false;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_PROJECTION_VERTICAL_PRESENTATION_ADAPTER_H_
