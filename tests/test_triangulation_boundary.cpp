#include "../src/core/reconstruction/TriangulationBoundary.hpp"
#include <roofer/reconstruction/AlphaShaper.hpp>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Constrained_triangulation_face_base_2.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <set>
#include <random>

namespace {
  using namespace roofer;
  using namespace roofer::reconstruction;
  using K = CGAL::Exact_predicates_inexact_constructions_kernel;
  using Fb = CGAL::Triangulation_face_base_with_info_2<
      int, K, CGAL::Constrained_triangulation_face_base_2<K>>;
  using Tds =
      CGAL::Triangulation_data_structure_2<CGAL::Triangulation_vertex_base_2<K>,
                                           Fb>;
  using Tri =
      CGAL::Constrained_Delaunay_triangulation_2<K, Tds,
                                                 CGAL::Exact_predicates_tag>;
  void rectangle(Tri& t, double x0, double y0, double x1, double y1) {
    std::array<K::Point_2, 4> p = {K::Point_2(x0, y0), K::Point_2(x1, y0),
                                   K::Point_2(x1, y1), K::Point_2(x0, y1)};
    for (int i = 0; i < 4; ++i) t.insert_constraint(p[i], p[(i + 1) % 4]);
  }
  template <class Ring>
  double signed_area(const Ring& r) {
    double a = 0;
    for (std::size_t i = 0; i < r.size(); ++i) {
      auto p = r[i], q = r[(i + 1) % r.size()];
      a += double(p[0]) * q[1] - double(p[1]) * q[0];
    }
    return a / 2;
  }
  auto label = [](Tri::Face_handle f) { return f->info(); };
  auto lift = [](Tri::Vertex_handle v) {
    const auto& p = v->point();
    return arr3d{p.x(), p.y(), p.x() + 2 * p.y()};
  };
  IndexedPlanesWithPoints grid(int maximum, bool hole = false,
                               double offset = 0) {
    IndexedPlanesWithPoints points;
    auto& entry = points[42];
    entry.first = Plane(-2, -3, 1, -5);
    for (int x = 0; x <= maximum; ++x)
      for (int y = 0; y <= maximum; ++y) {
        if (hole && x >= 3 && x <= 5 && y >= 3 && y <= 5) continue;
        entry.second.emplace_back(x + offset, y, 2 * (x + offset) + 3 * y + 5);
      }
    return points;
  }
  void check_alpha_area(const AlphaShaperInterface& alpha) {
    double rings = 0, triangles = 0;
    auto simple = [](const auto& ring) {
      using K = CGAL::Exact_predicates_exact_constructions_kernel;
      CGAL::Polygon_2<K> polygon;
      for (std::size_t i = 0; i + 1 < ring.size(); ++i)
        polygon.push_back(typename K::Point_2(ring[i][0], ring[i][1]));
      return polygon.is_simple();
    };
    for (const auto& r : alpha.alpha_rings) {
      CHECK(simple(r));
      CHECK(signed_area(r) > 0);
      rings += signed_area(r);
      REQUIRE(r.front() == r.back());
      for (const auto& h : r.interior_rings()) {
        CHECK(simple(h));
        CHECK(signed_area(h) < 0);
        rings += signed_area(h);
      }
      for (auto p : r) CHECK(p[2] == Catch::Approx(2 * p[0] + 3 * p[1] + 5));
    }
    for (const auto& t : alpha.alpha_triangles)
      triangles += std::abs(signed_area(t));
    CHECK(rings == Catch::Approx(triangles));
    CHECK(alpha.alpha_rings.size() == alpha.roofplane_ids.size());
    for (auto id : alpha.roofplane_ids) CHECK(id == 42);
  }
}  // namespace

TEST_CASE("triangulation boundaries group nested holes and islands by region") {
  Tri t;
  rectangle(t, 0, 0, 10, 10);
  rectangle(t, 3, 3, 7, 7);
  rectangle(t, 4, 4, 6, 6);
  rectangle(t, 20, 0, 22, 2);
  for (auto f : t.finite_face_handles()) {
    auto p = CGAL::centroid(t.triangle(f));
    bool outer = p.x() > 0 && p.x() < 10 && p.y() > 0 && p.y() < 10;
    bool hole = p.x() > 3 && p.x() < 7 && p.y() > 3 && p.y() < 7;
    bool island = p.x() > 4 && p.x() < 6 && p.y() > 4 && p.y() < 6;
    f->info() = outer && (!hole || island)                           ? 0
                : p.x() > 20 && p.x() < 22 && p.y() > 0 && p.y() < 2 ? 7
                                                                     : -1;
  }
  auto polygons = detail::triangulation_polygons(t, label, lift);
  REQUIRE(polygons.size() == 3);
  double area = 0;
  int holes = 0;
  for (const auto& p : polygons) {
    area += signed_area(p.exterior);
    CHECK(signed_area(p.exterior) > 0);
    for (const auto& h : p.holes) {
      ++holes;
      CHECK(signed_area(h) < 0);
      area += signed_area(h);
    }
    for (auto v : p.exterior) CHECK(v[2] == v[0] + 2 * v[1]);
  }
  CHECK(holes == 1);
  CHECK(area == Catch::Approx(92));
}
TEST_CASE(
    "triangulation boundaries distinguish touching components from rejected "
    "crossings") {
  Tri t;
  rectangle(t, 0, 0, 1, 1);
  rectangle(t, 1, 1, 2, 2);
  for (auto f : t.finite_face_handles()) {
    auto p = CGAL::centroid(t.triangle(f));
    f->info() = (p.x() < 1 && p.y() < 1) || (p.x() > 1 && p.y() > 1) ? 0 : -1;
  }
  auto polygons = detail::triangulation_polygons(t, label, lift);
  CHECK(polygons.size() == 2);
  CHECK_THROWS(detail::triangulation_polygons(t, label, lift, true));
}
TEST_CASE(
    "triangulation boundaries retain interfaces between selected labels") {
  Tri t;
  rectangle(t, 0, 0, 1, 1);
  rectangle(t, 1, 0, 2, 1);
  for (auto f : t.finite_face_handles())
    f->info() = CGAL::centroid(t.triangle(f)).x() < 1 ? 4 : 5;
  auto polygons = detail::triangulation_polygons(t, label, lift, true);
  REQUIRE(polygons.size() == 2);
  std::set<int> labels;
  for (const auto& p : polygons) {
    labels.insert(p.label);
    CHECK(signed_area(p.exterior) == Catch::Approx(1));
  }
  CHECK(labels == std::set<int>{4, 5});
}
TEST_CASE(
    "triangulation boundary extraction handles empty and collinear input") {
  Tri t;
  CHECK(detail::triangulation_polygons(t, label, lift).empty());
  t.insert(K::Point_2(0, 0));
  t.insert(K::Point_2(1, 0));
  t.insert(K::Point_2(2, 0));
  CHECK(detail::triangulation_polygons(t, label, lift).empty());
}
TEST_CASE("alpha shape polygons preserve sloped source elevations and area") {
  auto alpha = createAlphaShaper();
  AlphaShaperConfig cfg;
  cfg.alpha = 0.6F;
  alpha->compute(grid(4), cfg);
  REQUIRE(alpha->alpha_rings.size() == 1);
  CHECK(alpha->alpha_rings.front().interior_rings().empty());
  check_alpha_area(*alpha);
}
TEST_CASE("alpha shape boundary extraction retains holes") {
  auto alpha = createAlphaShaper();
  AlphaShaperConfig cfg;
  cfg.alpha = 0.6F;
  alpha->compute(grid(8, true), cfg);
  REQUIRE(alpha->alpha_rings.size() == 1);
  REQUIRE(alpha->alpha_rings.front().interior_rings().size() == 1);
  check_alpha_area(*alpha);
}
TEST_CASE("alpha ring order is stable across point insertion orders") {
  auto points = grid(14, true);
  auto& cloud = points.at(42).second;
  cloud.erase(std::remove_if(cloud.begin(), cloud.end(),
                             [](const Point& p) {
                               return p.x() >= 8 && p.x() <= 10 && p.y() >= 8 &&
                                      p.y() <= 10;
                             }),
              cloud.end());
  auto other = grid(2, false, 30);
  cloud.insert(cloud.end(), other.at(42).second.begin(),
               other.at(42).second.end());
  auto alpha = createAlphaShaper();
  AlphaShaperConfig cfg;
  cfg.alpha = 0.6F;
  alpha->compute(points, cfg);
  const auto expected = alpha->alpha_rings;
  const auto expected_ids = alpha->roofplane_ids;
  REQUIRE(expected.size() == 2);
  REQUIRE(expected.front().interior_rings().size() == 2);
  check_alpha_area(*alpha);

  std::mt19937 random(42);
  for (int permutation = 0; permutation < 6; ++permutation) {
    if (permutation == 0)
      std::reverse(cloud.begin(), cloud.end());
    else
      std::shuffle(cloud.begin(), cloud.end(), random);
    alpha->compute(points, cfg);
    REQUIRE(alpha->alpha_rings.size() == expected.size());
    CHECK(alpha->roofplane_ids == expected_ids);
    for (std::size_t i = 0; i < expected.size(); ++i) {
      CHECK(alpha->alpha_rings[i] == expected[i]);
      CHECK(alpha->alpha_rings[i].interior_rings() ==
            expected[i].interior_rings());
    }
  }
}
TEST_CASE(
    "alpha shape extraction supports disconnected and point touching regions") {
  auto alpha = createAlphaShaper();
  AlphaShaperConfig cfg;
  cfg.alpha = 0.6F;
  auto points = grid(1);
  auto other = grid(1, false, 10);
  auto& target = points.at(42).second;
  target.insert(target.end(), other.at(42).second.begin(),
                other.at(42).second.end());
  alpha->compute(points, cfg);
  REQUIRE(alpha->alpha_rings.size() == 2);
  check_alpha_area(*alpha);
  points.clear();
  points[42].second = {Point(0, 0, 5), Point(-1, -0.2, 2.4),
                       Point(-1, 0.2, 3.6), Point(1, -0.2, 6.4),
                       Point(1, 0.2, 7.6)};
  cfg.alpha = 0.3F;
  alpha->compute(points, cfg);
  REQUIRE(alpha->alpha_rings.size() == 2);
  check_alpha_area(*alpha);
}
TEST_CASE(
    "alpha shape labelling works without extraction and clears outputs on "
    "reuse") {
  auto alpha = createAlphaShaper();
  AlphaShaperConfig cfg;
  cfg.alpha = 0.6F;
  alpha->compute(grid(2), cfg);
  REQUIRE_FALSE(alpha->alpha_rings.empty());
  cfg.extract_polygons = false;
  alpha->compute(grid(2), cfg);
  CHECK(alpha->alpha_rings.empty());
  CHECK(alpha->roofplane_ids.empty());
  CHECK_FALSE(alpha->alpha_triangles.empty());
  IndexedPlanesWithPoints points;
  points[42].second = {Point(0, 0, 5), Point(1, 0, 7), Point(2, 0, 9)};
  alpha->compute(points, cfg);
  CHECK(alpha->alpha_triangles.empty());
}

TEST_CASE(
    "triangulation boundary walks split when a hole touches the exterior") {
  Tri t;
  rectangle(t, 0, 0, 4, 4);
  std::array<K::Point_2, 3> h = {K::Point_2(0, 0), K::Point_2(1, 2),
                                 K::Point_2(2, 1)};
  for (int i = 0; i < 3; ++i) t.insert_constraint(h[i], h[(i + 1) % 3]);
  CGAL::Polygon_2<K> hole(h.begin(), h.end());
  for (auto f : t.finite_face_handles()) {
    auto point = CGAL::centroid(t.triangle(f));
    f->info() = hole.bounded_side(point) == CGAL::ON_BOUNDED_SIDE ? -1 : 0;
  }
  auto p = detail::triangulation_polygons(t, label, lift);
  REQUIRE(p.size() == 1);
  REQUIRE(p[0].holes.size() == 1);
  CHECK(signed_area(p[0].exterior) + signed_area(p[0].holes[0]) ==
        Catch::Approx(14.5));
  CHECK_THROWS(detail::triangulation_polygons(t, label, lift, true));
}

TEST_CASE("irregular alpha boundaries preserve the selected triangle domain") {
  std::mt19937 random(42);
  std::uniform_real_distribution<double> position(0, 10);
  auto alpha = createAlphaShaper();
  for (int sample = 0; sample < 20; ++sample) {
    IndexedPlanesWithPoints points;
    points[42].first = Plane(-2, -3, 1, -5);
    for (int i = 0; i < 150; ++i) {
      double x = position(random), y = position(random);
      points[42].second.emplace_back(x, y, 2 * x + 3 * y + 5);
    }
    AlphaShaperConfig cfg;
    cfg.alpha = sample % 2 ? 0.4F : 0.8F;
    alpha->compute(points, cfg);
    REQUIRE_FALSE(alpha->alpha_rings.empty());
    check_alpha_area(*alpha);
  }
}
