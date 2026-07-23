// Project Seoul Site Layers.

#include "seoul/browser/site_layers/site_layer_types.h"

namespace seoul {

// Out-of-line copy, move, and destruction for the Site Layer value structs.
// Every member is deep-copyable, so all six special members are member-wise.
SiteAdjustment::SiteAdjustment() = default;
SiteAdjustment::SiteAdjustment(const SiteAdjustment&) = default;
SiteAdjustment::SiteAdjustment(SiteAdjustment&&) = default;
SiteAdjustment& SiteAdjustment::operator=(const SiteAdjustment&) = default;
SiteAdjustment& SiteAdjustment::operator=(SiteAdjustment&&) = default;
SiteAdjustment::~SiteAdjustment() = default;

SiteLayer::SiteLayer() = default;
SiteLayer::SiteLayer(const SiteLayer&) = default;
SiteLayer::SiteLayer(SiteLayer&&) = default;
SiteLayer& SiteLayer::operator=(const SiteLayer&) = default;
SiteLayer& SiteLayer::operator=(SiteLayer&&) = default;
SiteLayer::~SiteLayer() = default;

}  // namespace seoul
