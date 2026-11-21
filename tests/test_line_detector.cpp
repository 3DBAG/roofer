#include <catch2/catch_test_macros.hpp>
#include <roofer/reconstruction/LineDetector.hpp>

TEST_CASE("the line detector uses the configured neighbour count",
          "[line_detector]") {
  using namespace roofer;
  using namespace roofer::reconstruction;

  // The bottom edge is two collinear runs of 5 points, 1 m apart: 5
  // neighbours (the point itself included) don't reach across the gap and
  // find two lines there, 10 find one.
  LinearRing ring;
  for (float x : {0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 1.4f, 1.5f, 1.6f, 1.7f, 1.8f})
    ring.push_back({x, 0, 0});
  for (float x = 1.8f; x > -0.05f; x -= 0.1f) ring.push_back({x, 2, 0});
  const IndexedPlanesWithPoints planes{{1, {Plane(0, 0, 1, 0), {}}}};

  auto line_count = [&](int neighbour_count) {
    LineDetectorConfig config;
    config.neighbour_count = neighbour_count;
    config.perform_chaining = false;
    auto detector = createLineDetector();
    detector->detect({ring}, {1}, planes, config);
    return detector->edge_segments.size();
  };
  CHECK(line_count(5) == 3);
  CHECK(line_count(10) == 2);
}
