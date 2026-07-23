// Project Seoul workspace projection engine.

#include "seoul/browser/projection/projection_types.h"

namespace seoul {

WindowProjection::WindowProjection() = default;
WindowProjection::WindowProjection(const WindowProjection&) = default;
WindowProjection::WindowProjection(WindowProjection&&) = default;
WindowProjection& WindowProjection::operator=(const WindowProjection&) = default;
WindowProjection& WindowProjection::operator=(WindowProjection&&) = default;
WindowProjection::~WindowProjection() = default;

ProjectionSnapshot::ProjectionSnapshot() = default;
ProjectionSnapshot::ProjectionSnapshot(const ProjectionSnapshot&) = default;
ProjectionSnapshot::ProjectionSnapshot(ProjectionSnapshot&&) = default;
ProjectionSnapshot& ProjectionSnapshot::operator=(const ProjectionSnapshot&) = default;
ProjectionSnapshot& ProjectionSnapshot::operator=(ProjectionSnapshot&&) = default;
ProjectionSnapshot::~ProjectionSnapshot() = default;

}  // namespace seoul
