#include <initializer_list>

#include <catch2/catch_test_macros.hpp>

#include <roofer/reconstruction/ArrangementBase.hpp>

using namespace roofer;
using namespace roofer::reconstruction;

namespace {
  Polygon_2 make_polygon(std::initializer_list<Point_2> points) {
    return Polygon_2(points.begin(), points.end());
  }

  void insert_ring(Arrangement_2& arrangement, const Polygon_2& ring) {
    for (const auto& edge : ring.edges()) CGAL::insert(arrangement, edge);
  }
}  // namespace

TEST_CASE("exact face helpers preserve narrow concave domains and holes") {
  const EPECK::FT x = 1000000;
  const EPECK::FT unit = EPECK::FT(1) / 1000000000;
  const auto outer = make_polygon({{x, 0},
                                   {x + 6 * unit, 0},
                                   {x + 6 * unit, 6},
                                   {x + 4 * unit, 6},
                                   {x + 4 * unit, 10},
                                   {x, 10}});
  const auto hole = make_polygon(
      {{x + unit, 2}, {x + 2 * unit, 2}, {x + 2 * unit, 4}, {x + unit, 4}});
  Arrangement_2 arrangement;
  insert_ring(arrangement, outer);
  insert_ring(arrangement, hole);
  CGAL::Arr_naive_point_location<Arrangement_2> location(arrangement);

  size_t bounded_faces = 0;
  size_t holed_faces = 0;
  for (auto face : arrangement.face_handles()) {
    auto sample = arr_face_interior_point(face, location);
    if (face->is_unbounded()) {
      CHECK_FALSE(sample);
      continue;
    }
    ++bounded_faces;
    REQUIRE(sample);
    auto object = location.locate(*sample);
    const auto* located = std::get_if<Face_const_handle>(&object);
    REQUIRE(located);
    CHECK(*located == face);
    const auto polygon = arr_face_polygon(face);
    CHECK(polygon_contains(polygon, *sample,
                           PolygonBoundaryPolicy::strict_interior));
    CHECK(polygon.outer_boundary().is_counterclockwise_oriented());
    if (polygon.number_of_holes() == 0) continue;
    ++holed_faces;
    REQUIRE(polygon.number_of_holes() == 1);
    CHECK(polygon.outer_boundary().area() == 52 * unit);
    CHECK(polygon.holes_begin()->area() == -2 * unit);
  }
  CHECK(bounded_faces == 2);
  CHECK(holed_faces == 1);
}

TEST_CASE("polygon membership makes outer boundary inclusion explicit") {
  const auto outer = make_polygon({{0, 0}, {10, 0}, {10, 10}, {0, 10}});
  const auto hole = make_polygon({{3, 3}, {3, 7}, {7, 7}, {7, 3}});
  const Polygon_with_holes_2 polygon(outer, &hole, &hole + 1);
  const auto strict = PolygonBoundaryPolicy::strict_interior;
  const auto inclusive = PolygonBoundaryPolicy::include_outer_boundary;

  for (auto policy : {strict, inclusive}) {
    CHECK(polygon_contains(polygon, Point_2(1, 1), policy));
    CHECK_FALSE(polygon_contains(polygon, Point_2(-1, 1), policy));
    CHECK_FALSE(polygon_contains(polygon, Point_2(5, 5), policy));
    CHECK_FALSE(polygon_contains(polygon, Point_2(3, 5), policy));
    CHECK_FALSE(polygon_contains(polygon, Point_2(3, 3), policy));
  }
  for (auto point : {Point_2(0, 0), Point_2(0, 5)}) {
    CHECK_FALSE(polygon_contains(polygon, point, strict));
    CHECK(polygon_contains(polygon, point, inclusive));
  }
}

TEST_CASE("plane evaluation retains XY precision until mesh emission") {
  const EPECK::FT step = EPECK::FT(1) / 1024;
  const Point_2 point(1000000 + step, 500000);
  // A small displacement on a sloped plane must survive at large XY values.
  CHECK(plane_height(Plane(2, -4, 2, -10), point) == 5.0 - 1.0 / 1024);
  CHECK(plane_height(Plane(0, 0, -4, 30), point) == 7.5);
}
