#include <catch2/catch_test_macros.hpp>
#include <roofer/reconstruction/LineRegulariserBase.hpp>

#include <vector>

using K = CGAL::Exact_predicates_inexact_constructions_kernel;

TEST_CASE("snap threshold is a distance, not a squared distance", "[linereg]") {
  // The supporting lines of a and b meet at (0.99, 0), which is 0.7 away
  // from the gap between a's end (0, 0) and b's start (0.99, 0.99).
  K::Plane_3 plane(0, 0, 1, 0);
  K::Segment_3 a({-2, 0, 0}, {0, 0, 0});
  K::Segment_3 b({0.99, 0.99, 0}, {0.99, 3, 0});

  auto chained_points = [&](float snap_threshold) {
    std::vector<K::Point_3> ring_pts;
    roofer::linereg::chain<K>(plane, a, b, ring_pts, snap_threshold, 0);
    return ring_pts.size();
  };
  CHECK(chained_points(0.8F) == 1);
  CHECK(chained_points(0.5F) == 2);
}
