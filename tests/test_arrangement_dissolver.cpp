#include <array>

#include <catch2/catch_test_macros.hpp>

#include <roofer/reconstruction/ArrangementDissolver.hpp>
#include <roofer/reconstruction/ElevationProvider.hpp>

namespace {
  roofer::Arrangement_2 square_arrangement() {
    using roofer::Point_2;
    using roofer::Segment_2;

    roofer::Arrangement_2 arrangement;
    std::array<Point_2, 4> points = {Point_2(0, 0), Point_2(10, 0),
                                     Point_2(10, 10), Point_2(0, 10)};
    for (size_t i = 0; i < points.size(); ++i) {
      CGAL::insert(arrangement,
                   Segment_2(points[i], points[(i + 1) % points.size()]));
    }

    for (auto face : arrangement.face_handles()) {
      if (face->is_unbounded()) continue;
      face->data().is_finite = true;
      face->data().in_footprint = true;
      face->data().segid = 1;
    }
    return arrangement;
  }

  roofer::RasterTools::Raster empty_heightfield() {
    roofer::RasterTools::Raster raster(1, -1, 11, -1, 11);
    raster.prefill_arrays(roofer::RasterTools::MIN);
    return raster;
  }
}  // namespace

TEST_CASE("terrain clipping marks a boundary subface as ground") {
  auto arrangement = square_arrangement();
  for (auto face : arrangement.face_handles()) {
    if (!face->is_unbounded()) face->data().plane = roofer::Plane(1, 0, -1, 0);
  }

  auto elevation_provider =
      roofer::reconstruction::createElevationProvider(5.0F);
  auto dissolver = roofer::reconstruction::createArrangementDissolver();
  auto heightfield = empty_heightfield();
  dissolver->compute(
      arrangement, heightfield, *elevation_provider,
      {.dissolve_seg_edges = false, .dissolve_outside_fp = false});

  size_t roof_faces = 0;
  size_t ground_faces = 0;
  for (auto face : arrangement.face_handles()) {
    if (face->is_unbounded()) continue;
    if (face->data().in_footprint) ++roof_faces;
    if (face->data().is_ground) {
      ++ground_faces;
      CHECK_FALSE(face->data().in_footprint);
      CHECK_FALSE(face->data().is_footprint_hole);
    }
  }
  CHECK(roof_faces == 1);
  CHECK(ground_faces == 1);
}

TEST_CASE("terrain clipping marks an enclosed subface as a footprint hole") {
  auto arrangement = square_arrangement();
  for (auto face : arrangement.face_handles()) {
    if (!face->is_unbounded()) face->data().plane = roofer::Plane(0, 0, 1, -5);
  }

  roofer::LinearRing terrain;
  terrain.insert(terrain.end(),
                 {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}, {5, 5, 10}});
  auto terrain_cdt = roofer::proj_tri_util::cdt_from_linearing(terrain);
  auto elevation_provider =
      roofer::reconstruction::createElevationProvider(terrain_cdt);
  auto dissolver = roofer::reconstruction::createArrangementDissolver();
  auto heightfield = empty_heightfield();
  dissolver->compute(
      arrangement, heightfield, *elevation_provider,
      {.dissolve_seg_edges = false, .dissolve_outside_fp = false});

  size_t enclosed_ground_faces = 0;
  for (auto face : arrangement.face_handles()) {
    if (face->is_unbounded()) continue;
    if (face->data().is_ground && face->data().is_footprint_hole) {
      ++enclosed_ground_faces;
      CHECK_FALSE(face->data().in_footprint);
    }
  }
  CHECK(enclosed_ground_faces == 1);
}
