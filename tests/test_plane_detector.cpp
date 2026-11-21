#include <catch2/catch_test_macros.hpp>
#include <roofer/reconstruction/PlaneDetector.hpp>

TEST_CASE("roof elevations come from small planes when there are no large ones",
          "[plane_detector]") {
  using namespace roofer;
  using namespace roofer::reconstruction;

  // A gable roof of two 40-point planes rising from z = 2 to z = 4. Roof
  // elevations take the points of planes with more than 4 * min_plane_points
  // (60) points: neither plane has that many, together they have more.
  PointCollection points;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 10; ++j) {
      float x = 0.5f * i + 0.25f, y = 0.5f * j, z = 2 + x;
      points.push_back({x, y, z});
      points.push_back({4 - x, y, z});
    }

  auto detector = createPlaneDetector();
  detector->detect(points);
  REQUIRE(detector->roof_type == "slanted");
  CHECK(detector->roof_elevation_min == 2.25f);
  CHECK(detector->roof_elevation_50p == 2.75f);
  CHECK(detector->roof_elevation_70p == 3.25f);
  CHECK(detector->roof_elevation_max == 3.75f);
}
