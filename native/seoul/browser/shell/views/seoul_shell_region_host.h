// Project Seoul native browser shell V0.

#ifndef SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_REGION_HOST_H_
#define SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_REGION_HOST_H_

#include "base/memory/raw_ptr.h"

class VerticalTabStripRegionView;

namespace views {
class View;
}

namespace seoul {

class SeoulShellFooterView;
class SeoulShellHeaderView;
class ShellController;

class SeoulShellRegionHost {
 public:
  static void Attach(VerticalTabStripRegionView* region,
                     ShellController* controller);
  static void Detach(VerticalTabStripRegionView* region);

 private:
  static SeoulShellRegionHost* FromRegion(VerticalTabStripRegionView* region);
  void DoAttach(VerticalTabStripRegionView* region,
                ShellController* controller);
  void DoDetach();

  raw_ptr<VerticalTabStripRegionView> region_ = nullptr;
  raw_ptr<SeoulShellHeaderView> header_ = nullptr;
  raw_ptr<SeoulShellFooterView> footer_ = nullptr;
};

}  // namespace seoul

#endif  // SEOUL_BROWSER_SHELL_VIEWS_SEOUL_SHELL_REGION_HOST_H_
