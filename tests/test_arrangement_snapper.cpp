#include <array>
#include <map>
#include <variant>

#include <catch2/catch_test_macros.hpp>

#include <CGAL/Arr_walk_along_line_point_location.h>

#include <roofer/reconstruction/ArrangementSnapper.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>

namespace {
  using roofer::Arrangement_2;
  using roofer::Point_2;
  using roofer::Segment_2;
  using Face_const_handle = Arrangement_2::Face_const_handle;

  Arrangement_2 cross_arrangement(const std::array<double, 4>& heights) {
    Arrangement_2 arrangement;
    const std::array<Point_2, 4> corners = {Point_2(0, 0), Point_2(10, 0),
                                            Point_2(10, 10), Point_2(0, 10)};
    for (std::size_t i = 0; i < corners.size(); ++i) {
      CGAL::insert(arrangement,
                   Segment_2(corners[i], corners[(i + 1) % corners.size()]));
    }
    CGAL::insert(arrangement, Segment_2(Point_2(0, 5), Point_2(10, 5)));
    CGAL::insert(arrangement, Segment_2(Point_2(5, 0), Point_2(5, 10)));

    using PointLocation =
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>;
    PointLocation point_location(arrangement);
    const std::array<Point_2, 4> samples = {
        Point_2(7.5, 7.5), Point_2(2.5, 7.5), Point_2(2.5, 2.5),
        Point_2(7.5, 2.5)};
    for (std::size_t i = 0; i < samples.size(); ++i) {
      auto object = point_location.locate(samples[i]);
      auto face = std::get_if<Face_const_handle>(&object);
      REQUIRE(face != nullptr);
      auto mutable_face = arrangement.non_const_handle(*face);
      mutable_face->data().in_footprint = true;
      mutable_face->data().segid = static_cast<int>(i + 1);
      mutable_face->data().plane = roofer::Plane(0, 0, 1, -heights[i]);
    }
    return arrangement;
  }

  Arrangement_2::Face_handle face_at(Arrangement_2& arrangement,
                                     const Point_2& point) {
    using PointLocation =
        CGAL::Arr_walk_along_line_point_location<Arrangement_2>;
    PointLocation point_location(arrangement);
    auto object = point_location.locate(point);
    auto face = std::get_if<Face_const_handle>(&object);
    REQUIRE(face != nullptr);
    return arrangement.non_const_handle(*face);
  }

  Arrangement_2 repeated_face_arrangement() {
    auto arrangement = cross_arrangement({1, 2, 1, 3});
    auto repeated_face = face_at(arrangement, Point_2(7.5, 7.5));
    auto other_face = face_at(arrangement, Point_2(2.5, 2.5));
    other_face->data().segid = repeated_face->data().segid;
    other_face->data().plane = repeated_face->data().plane;
    return arrangement;
  }

  Arrangement_2 boundary_junction_arrangement() {
    Arrangement_2 arrangement;
    const std::array<Point_2, 4> corners = {Point_2(0, 0), Point_2(10, 0),
                                            Point_2(10, 10), Point_2(0, 10)};
    for (std::size_t i = 0; i < corners.size(); ++i) {
      CGAL::insert(arrangement,
                   Segment_2(corners[i], corners[(i + 1) % corners.size()]));
    }
    CGAL::insert(arrangement, Segment_2(Point_2(5, 0), Point_2(2, 10)));
    CGAL::insert(arrangement, Segment_2(Point_2(5, 0), Point_2(8, 10)));

    auto left = face_at(arrangement, Point_2(1, 5));
    left->data().in_footprint = true;
    left->data().segid = 1;
    left->data().plane = roofer::Plane(0, 0, 1, -15);

    auto middle = face_at(arrangement, Point_2(5, 5));
    middle->data().in_footprint = true;
    middle->data().segid = 2;
    middle->data().plane = roofer::Plane(0, 0, 1, -5);

    auto right = face_at(arrangement, Point_2(9, 5));
    right->data().in_footprint = true;
    right->data().segid = 3;
    right->data().plane = roofer::Plane(0, 0, 1, -15);

    arrangement.unbounded_face()->data().in_footprint = false;
    arrangement.unbounded_face()->data().segid = 0;
    arrangement.unbounded_face()->data().plane = roofer::Plane(0, 0, 1, 10);

    return arrangement;
  }

  Arrangement_2 finite_exterior_junction_arrangement() {
    auto arrangement = cross_arrangement({15, 5, 15, 0});

    auto hole = face_at(arrangement, Point_2(7.5, 2.5));
    hole->data().in_footprint = false;
    hole->data().is_footprint_hole = true;
    hole->data().segid = 0;
    hole->data().plane = roofer::Plane(0, 0, 1, 10);
    return arrangement;
  }

  Arrangement_2::Vertex_handle vertex_at(Arrangement_2& arrangement,
                                         const Point_2& point) {
    for (auto vertex : arrangement.vertex_handles()) {
      if (vertex->point() == point) return vertex;
    }
    return {};
  }

  void check_two_manifold_edges(const roofer::Mesh& mesh) {
    using Edge = std::pair<roofer::arr3f, roofer::arr3f>;
    std::map<Edge, std::size_t> edge_incidence;

    auto add_ring = [&](const roofer::vec3f& ring) {
      for (std::size_t i = 0; i < ring.size(); ++i) {
        auto first = ring[i];
        auto second = ring[(i + 1) % ring.size()];
        if (second < first) std::swap(first, second);
        if (first != second) ++edge_incidence[{first, second}];
      }
    };
    for (const auto& polygon : mesh.get_polygons()) {
      add_ring(polygon);
      for (const auto& hole : polygon.interior_rings()) add_ring(hole);
    }
    for (const auto& [edge, incidence] : edge_incidence) {
      CAPTURE(edge.first, edge.second);
      CHECK(incidence == 2);
    }
  }
}  // namespace

TEST_CASE("snapper repairs an alternating four-way roof junction") {
  // Heights in NE, NW, SW, SE order. The uniquely lowest NW face must absorb
  // the repair area.
  auto arrangement = cross_arrangement({15, 5, 15, 6});
  auto snapper = roofer::reconstruction::createArrangementSnapper();
  snapper->compute(arrangement, {.dist_thres = 0.001F,
                                 .repair_non_manifold_vertices = true,
                                 .manifold_repair_radius = 0.5F,
                                 .manifold_height_tolerance = 1e-4F});

  CHECK(vertex_at(arrangement, Point_2(5, 5)) ==
        Arrangement_2::Vertex_handle());
  CHECK(face_at(arrangement, Point_2(5, 5))->data().segid == 2);

  std::size_t roof_faces = 0;
  for (auto face : arrangement.face_handles()) {
    if (face->data().in_footprint) ++roof_faces;
  }
  CHECK(roof_faces == 4);
  for (auto vertex : arrangement.vertex_handles()) {
    CHECK(vertex->degree() <= 3);
  }

  auto extruder = roofer::reconstruction::createArrangementExtruder();
  extruder->compute(arrangement, 0.0F);
  REQUIRE(extruder->meshes.size() == 1);
  check_two_manifold_edges(extruder->meshes.front());
}

TEST_CASE("snapper preserves a manifold four-way roof junction") {
  auto arrangement = cross_arrangement({0, 1, 2, 3});
  auto snapper = roofer::reconstruction::createArrangementSnapper();
  snapper->compute(arrangement, {.dist_thres = 0.001F,
                                 .repair_non_manifold_vertices = true,
                                 .manifold_repair_radius = 0.5F,
                                 .manifold_height_tolerance = 1e-4F});

  auto center = vertex_at(arrangement, Point_2(5, 5));
  REQUIRE(center != Arrangement_2::Vertex_handle());
  CHECK(center->degree() == 4);
}

TEST_CASE("snapper repairs a repeated-face junction") {
  auto arrangement = repeated_face_arrangement();
  auto repeated_segid = face_at(arrangement, Point_2(7.5, 7.5))->data().segid;

  auto snapper = roofer::reconstruction::createArrangementSnapper();
  snapper->compute(arrangement, {.dist_thres = 0.001F,
                                 .repair_non_manifold_vertices = true,
                                 .manifold_repair_radius = 0.5F,
                                 .manifold_height_tolerance = 1e-4F});

  CHECK(vertex_at(arrangement, Point_2(5, 5)) ==
        Arrangement_2::Vertex_handle());
  CHECK(face_at(arrangement, Point_2(5, 5))->data().segid == repeated_segid);
}

TEST_CASE("snapper keeps a boundary repair cell inside the footprint") {
  auto arrangement = boundary_junction_arrangement();

  auto snapper = roofer::reconstruction::createArrangementSnapper();
  snapper->compute(arrangement, {.dist_thres = 0.001F,
                                 .repair_non_manifold_vertices = true,
                                 .manifold_repair_radius = 0.5F,
                                 .manifold_height_tolerance = 1e-4F});

  CHECK(vertex_at(arrangement, Point_2(5, 0)) ==
        Arrangement_2::Vertex_handle());
  CHECK(face_at(arrangement, Point_2(5, 0.05))->data().in_footprint);
}

TEST_CASE("snapper keeps a finite exterior repair cell inside the footprint") {
  auto arrangement = finite_exterior_junction_arrangement();

  auto snapper = roofer::reconstruction::createArrangementSnapper();
  snapper->compute(arrangement, {.dist_thres = 0.001F,
                                 .repair_non_manifold_vertices = true,
                                 .manifold_repair_radius = 0.5F,
                                 .manifold_height_tolerance = 1e-4F});

  CHECK(vertex_at(arrangement, Point_2(5, 5)) ==
        Arrangement_2::Vertex_handle());
  CHECK(face_at(arrangement, Point_2(5, 5))->data().segid == 2);
}
