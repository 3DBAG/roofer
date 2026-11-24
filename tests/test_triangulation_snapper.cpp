#include "../src/core/reconstruction/TriangulationSnapper.hpp"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <catch2/catch_test_macros.hpp>
#include <set>
#include <vector>

namespace {
  using K = CGAL::Exact_predicates_inexact_constructions_kernel;
  using Vb = CGAL::Triangulation_vertex_base_with_info_2<bool, K>;
  using Tds = CGAL::Triangulation_data_structure_2<
      Vb, CGAL::Constrained_triangulation_face_base_2<K>>;
  using Tri =
      CGAL::Constrained_Delaunay_triangulation_2<K, Tds,
                                                 CGAL::Exact_predicates_tag>;
  using Point = Tri::Point_2;

  bool constrained_path(const Tri& tri, Tri::Vertex_handle from,
                        Tri::Vertex_handle to) {
    std::set<Tri::Vertex_handle> seen{from};
    std::vector<Tri::Vertex_handle> todo{from};
    while (!todo.empty()) {
      auto v = todo.back();
      todo.pop_back();
      if (v == to) return true;
      auto e = tri.incident_edges(v), done = e;
      do {
        if (tri.is_infinite(*e) || !tri.is_constrained(*e)) continue;
        auto other = e->first->vertex(tri.cw(e->second));
        if (other == v) other = e->first->vertex(tri.ccw(e->second));
        if (seen.insert(other).second) todo.push_back(other);
      } while (++e != done);
    }
    return false;
  }

  void snap(Tri& tri) {
    for (auto v : tri.finite_vertex_handles()) v->info() = false;
    roofer::reconstruction::detail::snap_triangulation(
        tri, 0.021 * 0.021, [](auto&&...) { FAIL("no edge is short"); },
        [&](Tri::Edge e, Tri::Vertex_handle opposite) {
          auto a = e.first->vertex(tri.cw(e.second)),
               b = e.first->vertex(tri.ccw(e.second));
          bool to_a = tri.is_constrained({e.first, tri.ccw(e.second)}),
               to_b = tri.is_constrained({e.first, tri.cw(e.second)});
          tri.remove_constrained_edge(e.first, e.second);
          if (!to_a) tri.insert_constraint(a, opposite);
          if (!to_b) tri.insert_constraint(opposite, b);
        },
        [] {});
  }
}  // namespace

TEST_CASE("snapper keeps a constraint chain when a flip moves the face") {
  Tri tri;
  tri.insert(Point(5.4322130877666295, 1.1119360827559548));
  tri.insert(Point(2.0638799393678995, 7.1437011824426087));
  tri.insert_constraint(Point(6.7761458407864774, 6.1329067774005628),
                        Point(3.3875957516243909, 1.2802921707562118));
  tri.insert_constraint(Point(6.7761458407864774, 6.1329067774005628),
                        Point(4.4099154818613515, 2.7462974232707311));
  auto a = tri.insert(Point(1.0753021463626002, 3.7806497077095798));
  auto b = tri.insert(Point(2.6381455587640952, 5.8692220536828579));
  tri.insert_constraint(b, a);
  tri.insert_constraint(
      tri.insert(Point(2.3971727858769145, 5.5318491685262758)), a);
  tri.insert_constraint(
      tri.insert(Point(1.8476771784467267, 4.8075182346765679)), a);
  snap(tri);

  REQUIRE(tri.is_valid());
  CHECK(constrained_path(tri, a, b));
}

TEST_CASE(
    "snapper ends when two vertices closer than the threshold share no "
    "edge") {
  Tri tri;
  auto a = tri.insert(Point(1.0652932766558521, 5.1447675266883968));
  std::vector<Tri::Vertex_handle> ends{
      tri.insert(Point(3.7485101324739332, 5.9436660584025001)),
      tri.insert(Point(3.3505735878553251, 5.8172631028561135)),
      tri.insert(Point(3.3638078726473055, 5.8327757233701245))};
  for (auto end : ends) tri.insert_constraint(end, a);
  snap(tri);

  REQUIRE(tri.is_valid());
  for (auto end : ends) CHECK(constrained_path(tri, a, end));
}
