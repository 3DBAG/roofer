#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <roofer/reconstruction/PlaneIntersector.hpp>

TEST_CASE("plane intersections require overlapping inlier ranges",
          "[plane-intersector]") {
  using namespace roofer;
  using namespace roofer::reconstruction;

  // The two planes intersect along the y-axis. Their inliers sit on opposite
  // sides of it, with ranges [0, 2] and [start, start + 2].
  const std::map<size_t, std::map<size_t, size_t>> neighbours{{2, {{1, 5}}}};
  auto intersect_at = [&](double start) {
    const IndexedPlanesWithPoints planes{
        {1, {Plane(1, 0, -1, 0), {Point(0.25, 0, 0.25), Point(0.25, 2, 0.25)}}},
        {2,
         {Plane(-1, 0, -1, 0),
          {Point(-0.25, start, 0.25), Point(-0.25, start + 2, 0.25)}}}};
    auto intersector = createPlaneIntersector();
    intersector->compute(planes, neighbours);
    return intersector;
  };

  const auto overlapping = intersect_at(1);
  REQUIRE(overlapping->segments.size() == 1);
  const auto& segment = overlapping->segments[0];
  const auto bounds = std::minmax({segment[0][1], segment[1][1]});
  CHECK(bounds.first == 1);
  CHECK(bounds.second == 2);

  CHECK(intersect_at(2)->segments.size() == 0);
  CHECK(intersect_at(5)->segments.size() == 0);
}
