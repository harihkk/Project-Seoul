// Project Seoul Adaptive UI (SAUI).

#include "seoul/browser/saui/saui_types.h"

namespace seoul {

// Out-of-line special members for the surface types whose members are
// non-trivial (strings, containers, ids). The value-holding structs that need
// clone-based copies (DataTable, DataEntry, SurfaceAction, ComponentNode)
// define theirs in saui_document.cc alongside the parser that fills them.
SurfaceId::SurfaceId() = default;
SurfaceId::SurfaceId(const SurfaceId&) = default;
SurfaceId::SurfaceId(SurfaceId&&) = default;
SurfaceId& SurfaceId::operator=(const SurfaceId&) = default;
SurfaceId& SurfaceId::operator=(SurfaceId&&) = default;
SurfaceId::~SurfaceId() = default;

DataSeries::DataSeries() = default;
DataSeries::DataSeries(const DataSeries&) = default;
DataSeries::DataSeries(DataSeries&&) = default;
DataSeries& DataSeries::operator=(const DataSeries&) = default;
DataSeries& DataSeries::operator=(DataSeries&&) = default;
DataSeries::~DataSeries() = default;

SurfaceProvenance::SurfaceProvenance() = default;
SurfaceProvenance::SurfaceProvenance(const SurfaceProvenance&) = default;
SurfaceProvenance::SurfaceProvenance(SurfaceProvenance&&) = default;
SurfaceProvenance& SurfaceProvenance::operator=(const SurfaceProvenance&) =
    default;
SurfaceProvenance& SurfaceProvenance::operator=(SurfaceProvenance&&) = default;
SurfaceProvenance::~SurfaceProvenance() = default;

AdaptiveSurface::AdaptiveSurface() = default;
AdaptiveSurface::AdaptiveSurface(const AdaptiveSurface&) = default;
AdaptiveSurface::AdaptiveSurface(AdaptiveSurface&&) = default;
AdaptiveSurface& AdaptiveSurface::operator=(const AdaptiveSurface&) = default;
AdaptiveSurface& AdaptiveSurface::operator=(AdaptiveSurface&&) = default;
AdaptiveSurface::~AdaptiveSurface() = default;

}  // namespace seoul
