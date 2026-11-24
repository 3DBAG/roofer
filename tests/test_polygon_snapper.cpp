#include <roofer/reconstruction/PolygonSnapper.hpp>
#include <roofer/reconstruction/ArrangementBase.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <limits>

using namespace roofer;
using namespace roofer::reconstruction;
namespace {
  PolygonRings rectangle(double size = 10) {
    return {{{0, 0, 3}, {size, 0, 3}, {size, size, 3}, {0, size, 3}}, {}};
  }
  double area(const PolygonRings& polygon) {
    auto ring = [](const auto& points) {
      Polygon_2 p;
      for (auto v : points) p.push_back(Point_2(v[0], v[1]));
      REQUIRE(p.is_simple());
      return std::abs(CGAL::to_double(p.area()));
    };
    double result = ring(polygon.exterior);
    for (const auto& h : polygon.holes) result -= ring(h);
    return result;
  }
}  // namespace
TEST_CASE("polygon snapping is explicitly disabled with zero threshold") {
  auto p = rectangle();
  p.exterior[1] = {10, 10, 3};
  p.exterior[2] = {10, 0, 3};
  auto result = snap_polygon(p, 0);
  REQUIRE_FALSE(result.error);
  REQUIRE(result.polygons.size() == 1);
  CHECK(result.polygons[0].exterior == p.exterior);
}
TEST_CASE("polygon snapping preserves valid geometry holes and elevations") {
  auto p = rectangle();
  p.holes.push_back({{2, 2, 3}, {2, 4, 3}, {4, 4, 3}, {4, 2, 3}});
  auto r = snap_polygon(p, 0.001);
  REQUIRE_FALSE(r.error);
  REQUIRE(r.polygons.size() == 1);
  CHECK(area(r.polygons[0]) == Catch::Approx(96));
  REQUIRE(r.polygons[0].holes.size() == 1);
  for (auto v : r.polygons[0].exterior) CHECK(v[2] == 3);
}
TEST_CASE("polygon snapping rejects unresolved large crossings") {
  PolygonRings p{{{0, 0, 0}, {10, 10, 0}, {0, 10, 0}, {10, 0, 0}}, {}};
  auto result = snap_polygon(p, 0.001);
  CHECK(result.error.has_value());
  CHECK(result.polygons.empty());
}

TEST_CASE(
    "successive reroutes preserve a footprint wider than the snap distance") {
  // The arc reaches 0.35 m from its closing chord. Each local reroute can
  // move less than 0.021 m and still erase the footprint over many steps.
  PolygonRings polygon;
  for (int i = 0; i <= 100; ++i) {
    const double x = i / 10.0;
    polygon.exterior.push_back({x, 0.014 * x * (x - 10), 0});
  }

  auto result = snap_polygon(polygon, 0.021);
  INFO(result.error.value_or(""));
  REQUIRE_FALSE(result.error);
  REQUIRE(result.polygons.size() == 1);
  Polygon_2 exterior;
  for (const auto& point : result.polygons.front().exterior)
    exterior.push_back(Point_2(point[0], point[1]));
  CHECK(exterior.has_on_bounded_side(Point_2(5, -0.175)));
}

TEST_CASE("polygon snapping validates thresholds and coordinates") {
  for (double t : {-1.0, std::numeric_limits<double>::infinity(),
                   std::numeric_limits<double>::quiet_NaN()})
    CHECK_THROWS(snap_polygon(rectangle(), t));
  auto p = rectangle();
  p.exterior[0][0] = std::numeric_limits<double>::infinity();
  CHECK(snap_polygon(p, 0.001).error.has_value());
  CHECK(snap_polygon(rectangle(0.00001), 0.001).error.has_value());
}
TEST_CASE(
    "polygon snapping repairs thin backtracking spikes before float "
    "conversion") {
  PolygonRings p{{{0, 0, 0},
                  {10, 0, 0},
                  {10, 10, 0},
                  {0, 10, 0},
                  {0, 5, 0},
                  {-0.05, 5, 0},
                  {0, 5.000000001, 0}},
                 {}};
  auto r = snap_polygon(p, 0.001);
  REQUIRE_FALSE(r.error);
  REQUIRE(r.polygons.size() == 1);
  CHECK(area(r.polygons[0]) == Catch::Approx(100));
}

TEST_CASE(
    "polygon snapping can split a collapsed narrow bridge into valid parts") {
  PolygonRings p{{{0, 0, 1},
                  {4, 0, 1},
                  {4, 2, 1},
                  {6, 2, 1},
                  {6, 0, 1},
                  {10, 0, 1},
                  {10, 4, 1},
                  {6, 4, 1},
                  {6, 2.0001, 1},
                  {4, 2.0001, 1},
                  {4, 4, 1},
                  {0, 4, 1}},
                 {}};
  auto result = snap_polygon(p, 0.001);
  REQUIRE_FALSE(result.error);
  REQUIRE(result.polygons.size() == 2);
  CHECK(area(result.polygons[0]) + area(result.polygons[1]) ==
        Catch::Approx(32).margin(0.001));
}

TEST_CASE("polygon snapping is independent of source coordinate translation") {
  auto p = rectangle();
  p.exterior.insert(p.exterior.begin() + 1, {0.0001, 0, 3});
  auto a = snap_polygon(p, 0.001);
  for (auto& v : p.exterior) {
    v[0] += 122000;
    v[1] += 486000;
  }
  auto b = snap_polygon(p, 0.001);
  REQUIRE_FALSE(a.error);
  REQUIRE_FALSE(b.error);
  REQUIRE(a.polygons.size() == b.polygons.size());
  for (std::size_t i = 0; i < a.polygons.size(); ++i) {
    CHECK(area(a.polygons[i]) ==
          Catch::Approx(area(b.polygons[i])).margin(1e-8));
    for (auto v : b.polygons[i].exterior) CHECK(v[2] == 3);
  }
}

TEST_CASE("polygon snapping preserves retained nonconstant Z values") {
  auto p = rectangle();
  p.exterior[0][2] = 1.123456789;
  p.exterior[1][2] = 9.876543210;
  p.exterior[2][2] = 123.456789012;
  p.exterior[3][2] = -32.123456789;
  auto r = snap_polygon(p, 0.001);
  REQUIRE_FALSE(r.error);
  REQUIRE(r.polygons.size() == 1);
  for (const auto& v : r.polygons[0].exterior) {
    auto it = std::find_if(
        p.exterior.begin(), p.exterior.end(),
        [&](const auto& a) { return a[0] == v[0] && a[1] == v[1]; });
    REQUIRE(it != p.exterior.end());
    CHECK(v[2] == (*it)[2]);
  }
}
