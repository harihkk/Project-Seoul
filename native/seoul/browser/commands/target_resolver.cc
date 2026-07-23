// Project Seoul outbound browser command layer.

#include "seoul/browser/commands/target_resolver.h"

namespace seoul {

ResolvedSplitTarget::ResolvedSplitTarget() = default;
ResolvedSplitTarget::ResolvedSplitTarget(const ResolvedSplitTarget&) = default;
ResolvedSplitTarget::ResolvedSplitTarget(ResolvedSplitTarget&&) = default;
ResolvedSplitTarget& ResolvedSplitTarget::operator=(
    const ResolvedSplitTarget&) = default;
ResolvedSplitTarget& ResolvedSplitTarget::operator=(ResolvedSplitTarget&&) =
    default;
ResolvedSplitTarget::~ResolvedSplitTarget() = default;

}  // namespace seoul
