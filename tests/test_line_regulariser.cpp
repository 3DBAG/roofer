#include <catch2/catch_test_macros.hpp>
#include <roofer/reconstruction/LineRegulariserBase.hpp>

#include <type_traits>

using namespace roofer::linereg;

static_assert(!std::is_default_constructible_v<AngleCluster>);
static_assert(!std::is_default_constructible_v<DistCluster>);
static_assert(std::is_const_v<decltype(AngleCluster::id)>);
static_assert(std::is_const_v<decltype(DistCluster::id)>);

TEST_CASE("duplicate cluster IDs fail instead of losing lines", "[linereg]") {
  ClusterSet<AngleClusterH> angles;
  auto first = std::make_shared<AngleCluster>(7);
  insert_unique_cluster(angles, first);
  CHECK_THROWS_AS(
      insert_unique_cluster(angles, std::make_shared<AngleCluster>(7)),
      std::logic_error);
  REQUIRE(angles.size() == 1);
  CHECK(*angles.begin() == first);

  ClusterSet<DistClusterH> distances;
  insert_unique_cluster(distances, std::make_shared<DistCluster>(8));
  insert_unique_cluster(distances, std::make_shared<DistCluster>(3));
  CHECK((*distances.begin())->id == 3);
  CHECK_THROWS_AS(
      insert_unique_cluster(distances, std::make_shared<DistCluster>(3)),
      std::logic_error);
  CHECK(distances.size() == 2);
}

TEST_CASE("cluster IDs span angle groups and reset on repeated runs",
          "[linereg]") {
  LineRegulariser regulariser;
  regulariser.angle_threshold = 0.05;
  regulariser.dist_threshold = 0.2;
  roofer::SegmentCollection segments;
  segments.push_back({roofer::arr3f{0, 0, 0}, roofer::arr3f{0, 10, 0}});
  segments.push_back({roofer::arr3f{0.1F, 0, 0}, roofer::arr3f{0.1F, 10, 0}});
  segments.push_back({roofer::arr3f{0, 20, 0}, roofer::arr3f{10, 20, 0}});
  regulariser.add_segments(0, segments);

  for (int run = 0; run < 2; ++run) {
    CAPTURE(run);
    regulariser.perform_angle_clustering();
    regulariser.perform_distance_clustering();
    REQUIRE(regulariser.angle_clusters.size() == 2);
    REQUIRE(regulariser.dist_clusters.size() == 2);
    auto cluster = regulariser.dist_clusters.begin();
    CHECK((*cluster)->id == 0);
    CHECK((*cluster)->lines.size() == 2);
    ++cluster;
    CHECK((*cluster)->id == 2);
    CHECK((*cluster)->lines.size() == 1);
    CHECK(regulariser.lines[0].dist_cluster ==
          regulariser.lines[1].dist_cluster);
    CHECK(regulariser.lines[0].dist_cluster !=
          regulariser.lines[2].dist_cluster);
  }
}
