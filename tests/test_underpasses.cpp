#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <CGAL/Arr_naive_point_location.h>
#include <CGAL/squared_distance_3.h>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <roofer/reconstruction/MeshTriangulator.hpp>
#include <algorithm>
#include <map>
#include <limits>
#include "../src/core/reconstruction/WallAssembler.hpp"

namespace {
  using namespace roofer;
  using namespace roofer::reconstruction;

  LinearRing rectangle(float x0, float y0, float x1, float y1) {
    LinearRing p;
    p.insert(p.end(), {{x0, y0, 0}, {x1, y0, 0}, {x1, y1, 0}, {x0, y1, 0}});
    return p;
  }

  Arrangement_2 roof(bool split = false, float left = 10, float right = 10) {
    Arrangement_2 arr;
    auto p = rectangle(0, 0, 10, 10);
    for (std::size_t i = 0; i < p.size(); ++i)
      CGAL::insert(arr, Segment_2(Point_2(p[i][0], p[i][1]),
                                  Point_2(p[(i + 1) % p.size()][0],
                                          p[(i + 1) % p.size()][1])));
    if (split) CGAL::insert(arr, Segment_2(Point_2(5, 0), Point_2(5, 10)));
    for (auto f : arr.face_handles()) {
      if (f->is_unbounded()) continue;
      f->data().in_footprint = true;
      f->data().part_id = 0;
      f->data().segid = 1;
      f->data().plane = Plane(0, 0, 1, -left);
      f->data().elevation_70p = left;
    }
    if (split) {
      CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
      auto object = locate.locate(Point_2(7, 5));
      auto f = arr.non_const_handle(
          std::get<Arrangement_2::Face_const_handle>(object));
      f->data().plane = Plane(0, 0, 1, -right);
      f->data().segid = 2;
    }
    return arr;
  }

  void check_closed(const Mesh& mesh) {
    using Edge = std::pair<arr3f, arr3f>;
    std::map<Edge, int> directed;
    auto ring = [&](const vec3f& p) {
      REQUIRE(p.size() >= 3);
      for (std::size_t i = 0; i < p.size(); ++i) {
        auto a = p[i], b = p[(i + 1) % p.size()];
        REQUIRE(a != b);
        ++directed[{a, b}];
      }
    };
    for (const auto& p : mesh.get_polygons()) {
      ring(p);
      for (const auto& hole : p.interior_rings()) ring(hole);
    }
    for (const auto& [edge, count] : directed) {
      CAPTURE(edge.first, edge.second);
      CHECK(count == 1);
      CHECK(directed.count({edge.second, edge.first}) == 1);
    }
  }

  double volume(const Mesh& mesh) {
    auto t = createMeshTriangulatorLegacy();
    t->compute(std::vector<Mesh>{mesh});
    REQUIRE(t->volumes.size() == 1);
    return t->volumes.front();
  }

  auto extrude(Arrangement_2& arr, const std::vector<Underpass>& input,
               ArrangementExtruderConfig cfg = {}) {
    auto e = createArrangementExtruder();
    e->compute(arr, 0.0F, input, cfg);
    REQUIRE(e->meshes.size() == 1);
    check_closed(e->meshes.front());
    return e;
  }
}  // namespace

TEST_CASE(
    "underpass crosses the footprint without splitting the roof interior") {
  auto arr = roof();
  auto e = extrude(arr, {{rectangle(-2, 3, 12, 7), 4}});
  CHECK(arr.number_of_faces() == 2);
  CHECK(volume(e->meshes.front()) == Catch::Approx(840));
  CHECK(std::count(e->labels.begin(), e->labels.end(), 4) == 1);
  CHECK(std::count(e->labels.begin(), e->labels.end(), 2) == 4);
  CHECK(e->faces.size() == e->labels.size());
}

TEST_CASE("underpass holes retain ground patches and pillar walls") {
  auto arr = roof();
  auto passage = rectangle(-1, 2, 11, 8);
  passage.interior_rings().push_back(rectangle(4, 4, 6, 6));
  auto e = extrude(arr, {{passage, 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(776));
  int ceiling_holes = 0;
  for (std::size_t i = 0; i < e->labels.size(); ++i)
    if (e->labels[i] == 4) ceiling_holes += e->faces[i].interior_rings().size();
  CHECK(ceiling_holes == 1);
}

TEST_CASE("overlapping underpasses carve the union of their volumes") {
  auto arr = roof();
  auto e =
      extrude(arr, {{rectangle(-1, 2, 6, 6), 3}, {rectangle(4, 4, 11, 8), 5}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(820));
  auto other = roof();
  auto reversed = extrude(
      other, {{rectangle(4, 4, 11, 8), 5}, {rectangle(-1, 2, 6, 6), 3}});
  CHECK(volume(reversed->meshes.front()) == Catch::Approx(820));
}

TEST_CASE("ceiling is lowered below every overlapping roof part") {
  auto arr = roof(true, 5, 9);
  auto e = extrude(arr, {{rectangle(-1, 3, 11, 7), 8}});
  for (std::size_t i = 0; i < e->labels.size(); ++i)
    if (e->labels[i] == 4)
      for (auto p : e->faces[i]) CHECK(p[2] <= 5.0F - 0.03F);
  CHECK(arr.number_of_faces() == 3);
}

TEST_CASE("a sloped roof clamps against a minimum inside the underpass") {
  auto arr = roof(true);
  CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
  for (auto [x, a, d] : {std::tuple(2, 1.0, -10.0), std::tuple(8, -1.0, 0.0)}) {
    auto object = locate.locate(Point_2(x, 5));
    arr.non_const_handle(std::get<Arrangement_2::Face_const_handle>(object))
        ->data()
        .plane = Plane(a, 0, 1, d);
  }
  auto e = extrude(arr, {{rectangle(-1, 3, 11, 7), 12}});
  for (std::size_t i = 0; i < e->labels.size(); ++i)
    if (e->labels[i] == 4)
      for (auto p : e->faces[i]) CHECK(p[2] <= 5.0F - 0.03F);
}

TEST_CASE(
    "nonintersecting and unusable underpasses leave the building intact") {
  for (const auto& p : std::vector<Underpass>{{rectangle(20, 20, 30, 30), 4},
                                              {rectangle(2, 2, 8, 8), -1}}) {
    auto arr = roof();
    auto e = extrude(arr, {p});
    CHECK(volume(e->meshes.front()) == Catch::Approx(1000));
    CHECK(std::count(e->labels.begin(), e->labels.end(), 4) == 0);
  }
}

TEST_CASE(
    "point touching raised regions are skipped instead of creating a "
    "nonmanifold edge") {
  auto arr = roof();
  auto e =
      extrude(arr, {{rectangle(1, 1, 5, 5), 4}, {rectangle(5, 5, 9, 9), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1000));
}

TEST_CASE("coplanar facade pieces merge without underpasses") {
  auto arr = roof(true, 10, 15);
  auto e = extrude(arr, {});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1250));
  CHECK(std::count(e->labels.begin(), e->labels.end(), 2) == 4);
}

TEST_CASE("LoD1 extrusion ignores underpass inputs") {
  auto arr = roof();
  auto e = extrude(arr, {{rectangle(-1, 3, 11, 7), 4}}, {.lod2 = false});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1000));
}

TEST_CASE("floor maximum includes interpolated vertices inside the domain") {
  auto input = rectangle(0, 0, 10, 10);
  input.push_back({5, 5, 6});
  auto provider =
      createElevationProvider(proj_tri_util::cdt_from_linearing(input));
  CGAL::Polygon_2<EPECK> boundary;
  for (auto p : rectangle(2, 2, 8, 8)) boundary.push_back(Point_2(p[0], p[1]));
  CHECK(provider->get_max_elevation(
            CGAL::Polygon_with_holes_2<EPECK>(boundary)) == Catch::Approx(6));
  auto arr = roof();
  auto e = createArrangementExtruder();
  const std::vector<Underpass> passages{{rectangle(2, 2, 8, 8), 4}};
  e->compute(arr, *provider, passages, {});
  REQUIRE(e->meshes.size() == 1);
  check_closed(e->meshes.front());
  CHECK(std::count(e->labels.begin(), e->labels.end(), 4) == 0);
}

TEST_CASE("underpass boundaries snap to nearby footprint constraints") {
  auto arr = roof(true);
  auto e = extrude(arr, {{rectangle(-1, 0.00001F, 11, 5), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(800));
  for (const auto& p : e->faces)
    for (auto v : p) CHECK((v[1] == 0 || v[1] >= 0.001F));
}

TEST_CASE("existing empty-brace extrusion configuration stays unambiguous") {
  auto arr = roof();
  auto e = createArrangementExtruder();
  e->compute(arr, 0.0F, {});
  REQUIRE(e->meshes.size() == 1);
  check_closed(e->meshes.front());
}

TEST_CASE("an underpass can contain the complete footprint") {
  auto arr = roof();
  auto e = extrude(arr, {{rectangle(-1, -1, 11, 11), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(600));
  CHECK(std::count(e->labels.begin(), e->labels.end(), 0) == 0);
}

TEST_CASE("courtyard boundaries remain linked around a carved opening") {
  auto arr = roof();
  auto courtyard = rectangle(4, 4, 6, 6);
  for (std::size_t i = 0; i < courtyard.size(); ++i) {
    auto p = courtyard[i], q = courtyard[(i + 1) % courtyard.size()];
    CGAL::insert(arr, Segment_2(Point_2(p[0], p[1]), Point_2(q[0], q[1])));
  }
  CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
  auto object = locate.locate(Point_2(5, 5));
  auto hole =
      arr.non_const_handle(std::get<Arrangement_2::Face_const_handle>(object));
  hole->data().in_footprint = false;
  hole->data().is_footprint_hole = true;
  auto baseline_arr = arr;
  auto baseline = extrude(baseline_arr, {});
  CHECK(volume(baseline->meshes.front()) == Catch::Approx(960));
  auto e = extrude(arr, {{rectangle(-1, 4, 5, 6), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(928));
}

TEST_CASE("separate building parts retain their own floor and walls") {
  auto arr = roof();
  auto second = rectangle(20, 0, 30, 10);
  for (std::size_t i = 0; i < second.size(); ++i) {
    auto p = second[i], q = second[(i + 1) % second.size()];
    CGAL::insert(arr, Segment_2(Point_2(p[0], p[1]), Point_2(q[0], q[1])));
  }
  CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
  auto object = locate.locate(Point_2(25, 5));
  auto f =
      arr.non_const_handle(std::get<Arrangement_2::Face_const_handle>(object));
  f->data().in_footprint = true;
  f->data().part_id = 1;
  f->data().segid = 2;
  f->data().plane = Plane(0, 0, 1, -10);
  auto baseline_arr = arr;
  auto baseline = createArrangementExtruder();
  baseline->compute(baseline_arr, 0.0F, {});
  REQUIRE(baseline->multisolid.size() == 2);
  for (const auto& [part, mesh] : baseline->multisolid) {
    check_closed(mesh);
    CHECK(volume(mesh) == Catch::Approx(1000));
  }
  auto e = createArrangementExtruder();
  std::vector<Underpass> passages{{rectangle(-1, 3, 31, 7), 4}};
  e->compute(arr, 0.0F, passages, {});
  REQUIRE(e->multisolid.size() == 2);
  for (const auto& [part, mesh] : e->multisolid) {
    check_closed(mesh);
    CHECK(volume(mesh) == Catch::Approx(840));
  }
}

TEST_CASE("tiny overlap strips do not produce collapsed underside rings") {
  auto arr = roof();
  auto e = extrude(
      arr, {{rectangle(2, 2, 8, 8), 4}, {rectangle(2.00001F, 2, 8, 8), 5}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(820.0003));
}

TEST_CASE("a sub-tolerance gap between cutouts is reported and skipped") {
  auto arr = roof();
  auto e = extrude(
      arr, {{rectangle(2, 2, 4, 8), 4}, {rectangle(4.00001F, 2, 6, 8), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1000));
}

TEST_CASE(
    "equal-height overlapping underpasses dissolve internal constraint "
    "cycles") {
  auto arr = roof();
  auto e = extrude(arr, {{rectangle(1, 1, 6, 6), 4},
                         {rectangle(4, 1, 9, 6), 4},
                         {rectangle(2, 4, 8, 9), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(768));
  CHECK(std::count(e->labels.begin(), e->labels.end(), 4) == 1);
}

TEST_CASE(
    "practically equal ceilings do not create sub-tolerance vertical walls") {
  auto arr = roof();
  auto e = extrude(
      arr, {{rectangle(2, 2, 5, 8), 4}, {rectangle(5, 2, 8, 8), 4.00001}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(856));
  CHECK(std::count(e->labels.begin(), e->labels.end(), 4) == 1);
}

TEST_CASE("rejecting a narrow higher patch retains the broad underpass") {
  std::vector<Underpass> input{{rectangle(1, 1, 9, 9), 4},
                               {rectangle(4, 2, 4.0001F, 8), 5}};
  for (int order = 0; order < 2; ++order) {
    auto arr = roof();
    auto e = extrude(arr, input);
    CHECK(volume(e->meshes.front()) == Catch::Approx(744));
    std::reverse(input.begin(), input.end());
  }
}

TEST_CASE("rejecting a thin extension retains the shared ceiling") {
  std::vector<Underpass> input{{rectangle(1, 1, 6, 9), 4},
                               {rectangle(5, 4, 9, 4.0001F), 4}};
  for (int order = 0; order < 2; ++order) {
    auto arr = roof();
    auto e = extrude(arr, input);
    CHECK(volume(e->meshes.front()) == Catch::Approx(840));
    std::reverse(input.begin(), input.end());
  }
}

TEST_CASE("roof profile crossings remain closed after vertical conformance") {
  auto arr = roof(true);
  CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
  auto object = locate.locate(Point_2(2, 5));
  arr.non_const_handle(std::get<Arrangement_2::Face_const_handle>(object))
      ->data()
      .plane = Plane(0, -0.4, 1, -8);
  auto e = extrude(arr, {{rectangle(-1, 3, 11, 7), 4}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(840));
}

TEST_CASE("walls at a junction include all incident roof heights") {
  auto arr = roof(true, 8, 12);
  CGAL::insert(arr, Segment_2(Point_2(0, 5), Point_2(10, 5)));
  CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
  for (auto [x, y, height] : {std::tuple(2, 2, 8), std::tuple(7, 2, 10),
                              std::tuple(7, 7, 12), std::tuple(2, 7, 14)}) {
    auto object = locate.locate(Point_2(x, y));
    auto face = arr.non_const_handle(
        std::get<Arrangement_2::Face_const_handle>(object));
    face->data().in_footprint = true;
    face->data().part_id = 0;
    face->data().segid = 1;
    face->data().plane = Plane(0, 0, 1, -height);
  }
  auto e = extrude(arr, {});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1100));
}

namespace {
  class CountingElevationProvider : public ElevationProvider {
   public:
    mutable int clearance_queries = 0;
    float get(Point_2) const override { return 0; }
    float get_percentile(float) const override { return 0; }
    float get_max_elevation(
        const CGAL::Polygon_with_holes_2<EPECK>&) const override {
      ++clearance_queries;
      return 0;
    }
    std::vector<Segment_2> get_intersections(
        const Plane&, const CGAL::Polygon_with_holes_2<EPECK>&) const override {
      return {};
    }
  };
}  // namespace

TEST_CASE("terrain clearance work is independent of roof partition count") {
  int baseline_queries = 0;
  for (bool split : {false, true}) {
    auto arr = roof(split);
    auto e = createArrangementExtruder();
    CountingElevationProvider elevations;
    const std::vector<Underpass> input{{rectangle(2, 2, 8, 8), 4}};
    e->compute(arr, elevations, input, {});
    REQUIRE(e->meshes.size() == 1);
    check_closed(e->meshes.front());
    CHECK(volume(e->meshes.front()) == Catch::Approx(856));
    if (!split) baseline_queries = elevations.clearance_queries;
    CHECK(elevations.clearance_queries == baseline_queries);
  }
  CHECK(baseline_queries > 0);
}

TEST_CASE("missing roof planes bypass underpass clearance") {
  auto arr = roof();
  for (auto face : arr.face_handles()) face->data().segid = 0;
  CountingElevationProvider elevations;
  auto e = createArrangementExtruder();
  const std::vector<Underpass> input{{rectangle(-1, 3, 11, 7), 4}};
  e->compute(arr, elevations, input, {});
  CHECK(elevations.clearance_queries == 0);
  REQUIRE(e->meshes.size() == 1);
  REQUIRE(e->labels.size() == 1);
  CHECK(e->labels.front() == 0);
}

TEST_CASE("discarded inputs leave no tiny splits on the footprint") {
  auto arr = roof();
  auto e = extrude(
      arr, {{rectangle(1, 1, 9, 9), 4}, {rectangle(-1, 4, 5, 4.0001F), -1}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(744));
}

TEST_CASE("straight underpass sides form larger stepped polygons") {
  auto arr = roof();
  auto e =
      extrude(arr, {{rectangle(2, 2, 5, 8), 4}, {rectangle(5, 2, 8, 8), 5}});
  CHECK(volume(e->meshes.front()) == Catch::Approx(838));
  // Four sides around the combined opening, plus the step between ceilings.
  CHECK(std::count(e->labels.begin(), e->labels.end(), 3) == 5);
  bool ground_split = false, junction = false;
  for (const auto& polygon : e->faces) {
    auto visit = [&](const vec3f& ring) {
      for (auto p : ring) {
        ground_split |= p == arr3f{5, 2, 0};
        junction |= p == arr3f{5, 2, 4};
      }
    };
    visit(polygon);
    for (const auto& hole : polygon.interior_rings()) visit(hole);
  }
  CHECK_FALSE(ground_split);
  CHECK(junction);
}

TEST_CASE("straight roof steps merge across changing upper roofs") {
  auto arr = roof(true);
  CGAL::insert(arr, Segment_2(Point_2(0, 5), Point_2(10, 5)));
  CGAL::Arr_naive_point_location<Arrangement_2> locate(arr);
  for (auto [x, y, height] : {std::tuple(2, 2, 8), std::tuple(2, 7, 8),
                              std::tuple(7, 2, 12), std::tuple(7, 7, 14)}) {
    auto object = locate.locate(Point_2(x, y));
    auto face = arr.non_const_handle(
        std::get<Arrangement_2::Face_const_handle>(object));
    face->data().in_footprint = true;
    face->data().part_id = 0;
    face->data().segid = 1;
    face->data().plane = Plane(0, 0, 1, -height);
  }
  auto e = extrude(arr, {});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1050));
  CHECK(std::count(e->labels.begin(), e->labels.end(), 3) == 2);
}

TEST_CASE("collinear mesh subdivisions disappear from all incident surfaces") {
  auto arr = roof();
  CGAL::insert_point(arr, Point_2(5, 0));
  auto e = extrude(arr, {});
  CHECK(volume(e->meshes.front()) == Catch::Approx(1000));
  REQUIRE(e->faces.size() == 6);
  for (const auto& polygon : e->faces) CHECK(polygon.size() == 4);
}

TEST_CASE("wall spans join independently of insertion order") {
  roofer::reconstruction::detail::WallAssembler walls;
  walls.add(Point_2(2, 0), Point_2(3, 0), {2, 2}, {6, 6}, 0, 3);
  walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
  walls.add(Point_2(1, 0), Point_2(2, 0), {1, 1}, {5, 5}, 0, 3);
  std::unordered_map<int, Mesh> meshes;
  walls.emit(meshes);
  REQUIRE(meshes.size() == 1);
  REQUIRE(meshes.at(0).get_polygons().size() == 1);
  const auto& polygon = meshes.at(0).get_polygons().front();
  CHECK(polygon.size() == 12);
  CHECK(polygon.front() == arr3f{0, 0, 0});
  CHECK(polygon.back() == arr3f{0, 0, 4});
}

TEST_CASE("wall merging preserves semantics, orientation and connectivity") {
  roofer::reconstruction::detail::WallAssembler walls;
  SECTION("different building parts") {
    walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
    walls.add(Point_2(1, 0), Point_2(2, 0), {0, 0}, {4, 4}, 1, 3);
  }
  SECTION("different surface labels") {
    walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 2);
    walls.add(Point_2(1, 0), Point_2(2, 0), {0, 0}, {4, 4}, 0, 3);
  }
  SECTION("opposite facing walls") {
    walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
    walls.add(Point_2(1, 0), Point_2(0, 0), {1, 1}, {5, 5}, 0, 3);
  }
  SECTION("point-only contact") {
    walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
    walls.add(Point_2(1, 0), Point_2(2, 0), {4, 4}, {8, 8}, 0, 3);
  }
  SECTION("disconnected height intervals") {
    walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
    walls.add(Point_2(1, 0), Point_2(2, 0), {5, 5}, {8, 8}, 0, 3);
  }
  SECTION("turning a corner") {
    walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
    walls.add(Point_2(1, 0), Point_2(1, 1), {0, 0}, {4, 4}, 0, 3);
  }
  std::unordered_map<int, Mesh> meshes;
  walls.emit(meshes);
  std::size_t polygons = 0;
  for (const auto& [part, mesh] : meshes)
    polygons += mesh.get_polygons().size();
  CHECK(polygons == 2);
}

TEST_CASE("wall merging stops at ambiguous branches") {
  roofer::reconstruction::detail::WallAssembler walls;
  walls.add(Point_2(0, 0), Point_2(1, 0), {0, 0}, {4, 4}, 0, 3);
  walls.add(Point_2(1, 0), Point_2(2, 0), {0, 0}, {4, 4}, 0, 3);
  walls.add(Point_2(1, 0), Point_2(3, 0), {1, 1}, {5, 5}, 0, 3);
  std::unordered_map<int, Mesh> meshes;
  walls.emit(meshes);
  REQUIRE(meshes.size() == 1);
  CHECK(meshes.at(0).get_polygons().size() == 3);
}

TEST_CASE("mesh cleanup tolerates rounding on shared roof and wall edges") {
  auto arr = roof();
  auto e = extrude(arr, {});
  const arr3f a{0, 0, 10}, b{10, 0, 10}, rounded{5, 0.00001F, 10};
  auto& mesh = e->multisolid.at(0);
  for (auto& polygon : mesh.get_polygons()) {
    vec3f ring;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
      const auto p = polygon[i], q = polygon[(i + 1) % polygon.size()];
      ring.push_back(p);
      if ((p == a && q == b) || (p == b && q == a)) ring.push_back(rounded);
    }
    polygon.assign(ring.begin(), ring.end());
  }
  check_closed(mesh);
  auto has_rounded = [&] {
    for (const auto& polygon : mesh.get_polygons())
      if (std::find(polygon.begin(), polygon.end(), rounded) != polygon.end())
        return true;
    return false;
  };
  roofer::reconstruction::detail::remove_redundant_vertices(e->multisolid, 0);
  CHECK(has_rounded());
  roofer::reconstruction::detail::remove_redundant_vertices(e->multisolid,
                                                            0.001F);
  CHECK_FALSE(has_rounded());
  check_closed(mesh);
  CHECK(volume(mesh) == Catch::Approx(1000));
}

TEST_CASE("mesh cleanup bounds deviation across an entire degree-two chain") {
  LinearRing wall;
  wall.insert(wall.end(), {{0, 0, 0}, {10, 0, 0}});
  for (int x = 10; x >= 0; --x)
    wall.push_back({float(x), 0, 10 + 0.0008F * x * (10 - x)});
  const auto original = wall;
  std::unordered_map<int, Mesh> meshes;
  meshes[0].push_polygon(wall, 3);
  constexpr float tolerance = 0.001F;
  roofer::reconstruction::detail::remove_redundant_vertices(meshes, tolerance);
  const auto& result = meshes.at(0).get_polygons().front();
  REQUIRE(result.size() < original.size());
  CHECK(std::find(result.begin(), result.end(), arr3f{5, 0, 10.02F}) !=
        result.end());
  auto point = [](const arr3f& p) { return EPICK::Point_3(p[0], p[1], p[2]); };
  for (const auto& p : original) {
    double minimum = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < result.size(); ++i)
      minimum = std::min(
          minimum, CGAL::squared_distance(
                       point(p), EPICK::Segment_3(
                                     point(result[i]),
                                     point(result[(i + 1) % result.size()]))));
    CHECK(minimum <= double(tolerance) * tolerance);
  }
}
