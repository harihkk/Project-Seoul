// Project Seoul native browser shell V0.

#include "seoul/browser/shell/views/seoul_shell_region_host.h"

#include <map>
#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "seoul/browser/shell/views/seoul_shell_footer_view.h"
#include "seoul/browser/shell/views/seoul_shell_header_view.h"
#include "ui/views/view.h"

namespace seoul {
namespace {

std::map<VerticalTabStripRegionView*, std::unique_ptr<SeoulShellRegionHost>>&
Hosts() {
  static base::NoDestructor<std::map<VerticalTabStripRegionView*,
                                     std::unique_ptr<SeoulShellRegionHost>>>
      hosts;
  return *hosts;
}

}  // namespace

SeoulShellRegionHost* SeoulShellRegionHost::FromRegion(
    VerticalTabStripRegionView* region) {
  auto it = Hosts().find(region);
  return it != Hosts().end() ? it->second.get() : nullptr;
}

void SeoulShellRegionHost::Attach(VerticalTabStripRegionView* region,
                                  ShellController* controller) {
  if (!region || !controller) {
    return;
  }
  SeoulShellRegionHost* host = FromRegion(region);
  if (!host) {
    host = Hosts()
               .emplace(region, std::make_unique<SeoulShellRegionHost>())
               .first->second.get();
  }
  host->DoAttach(region, controller);
}

void SeoulShellRegionHost::Detach(VerticalTabStripRegionView* region) {
  if (!region) {
    return;
  }
  if (SeoulShellRegionHost* host = FromRegion(region)) {
    host->DoDetach();
    Hosts().erase(region);
  }
}

void SeoulShellRegionHost::DoAttach(VerticalTabStripRegionView* region,
                                    ShellController* controller) {
  region_ = region;
  if (!header_) {
    auto header = std::make_unique<SeoulShellHeaderView>(controller);
    header_ = region->AddChildView(std::move(header));
  } else {
    header_->BindController(controller);
  }
  if (!footer_) {
    auto footer = std::make_unique<SeoulShellFooterView>(controller);
    footer_ = region->AddChildView(std::move(footer));
  } else {
    footer_->BindController(controller);
  }

  views::View* separator = region->GetSeoulShellSeparatorAnchor();
  views::View* bottom = region->GetSeoulShellFooterAnchor();
  if (separator) {
    std::optional<size_t> separator_index = region->GetIndexOf(separator);
    if (separator_index.has_value()) {
      region->ReorderChildView(header_, separator_index.value() + 1);
      if (views::View* tab_strip = region->GetSeoulTabStripView()) {
        region->ReorderChildView(tab_strip, separator_index.value() + 2);
      }
    }
  }
  if (bottom) {
    region->ReorderChildView(footer_, region->GetIndexOf(bottom).value_or(0));
  }
}

void SeoulShellRegionHost::DoDetach() {
  if (region_ && header_) {
    region_->RemoveChildViewT(header_);
    header_ = nullptr;
  }
  if (region_ && footer_) {
    region_->RemoveChildViewT(footer_);
    footer_ = nullptr;
  }
  region_ = nullptr;
}

}  // namespace seoul
