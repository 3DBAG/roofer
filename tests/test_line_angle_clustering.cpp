#include <catch2/catch_test_macros.hpp>
#include <roofer/reconstruction/LineRegulariserBase.hpp>

#include <algorithm>
#include <vector>

using namespace roofer::linereg;

TEST_CASE("near-vertical lines cluster across the angle wrap-around",
          "[linereg]") {
  // Two almost north-south lines, tilted in opposite directions. Their
  // angles end up near 0 and near pi, but they have nearly the same direction.
  LineRegulariser regulariser;
  regulariser.angle_threshold = 0.05;
  regulariser.dist_threshold = 0.2;
  roofer::SegmentCollection segments;
  segments.push_back({roofer::arr3f{0, 0, 0}, roofer::arr3f{0.05F, 10, 0}});
  segments.push_back({roofer::arr3f{0.1F, 0, 0}, roofer::arr3f{0.05F, 10, 0}});
  regulariser.add_segments(0, segments);
  regulariser.perform_angle_clustering();
  regulariser.perform_distance_clustering();
  REQUIRE(regulariser.angle_clusters.size() == 1);
  REQUIRE(regulariser.dist_clusters.size() == 1);

  std::vector<linetype*> lines;
  for (auto& line : regulariser.lines) lines.push_back(&line);
  double mean = calc_mean_angle(lines);
  CHECK(std::min(mean, pi - mean) < 0.01);
}
