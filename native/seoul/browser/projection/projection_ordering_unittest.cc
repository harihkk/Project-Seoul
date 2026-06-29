// Project Seoul workspace projection engine V0.

#include "seoul/browser/projection/projection_ordering.h"

#include "seoul/browser/organization/organization_model.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace seoul {
namespace {

TEST(ProjectionOrderingTest, CloseFallbackIgnoresHiddenAdjacent) {
  WindowProjection projection;
  ProjectedTab a;
  a.tab = LiveTabKey::FromSessionId(1);
  a.projected_order = 0;
  ProjectedTab b;
  b.tab = LiveTabKey::FromSessionId(2);
  b.projected_order = 1;
  projection.tabs = {a, b};
  EXPECT_EQ(ProjectionOrdering::ChooseCloseFallback(projection, a.tab), b.tab);
}

TEST(ProjectionOrderingTest, ResolveMoveWithinProjection) {
  WindowProjection projection;
  LiveWindowTabState live;
  ProjectedTab a;
  a.tab = LiveTabKey::FromSessionId(1);
  a.projected_order = 0;
  ProjectedTab b;
  b.tab = LiveTabKey::FromSessionId(2);
  b.projected_order = 1;
  projection.tabs = {a, b};
  LiveTabDescriptor da;
  da.tab = a.tab;
  da.strip_order = 5;
  LiveTabDescriptor db;
  db.tab = b.tab;
  db.strip_order = 9;
  live.tabs = {da, db};

  ProjectedMoveRequest req;
  req.tab = a.tab;
  req.projected_destination = 1;
  ResolvedMoveDestination out;
  EXPECT_TRUE(
      ProjectionOrdering::ResolveMoveDestination(projection, live, req, &out));
  EXPECT_EQ(out.raw_destination_index, 9);
}

}  // namespace
}  // namespace seoul
