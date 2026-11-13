#include <catch2/catch_test_macros.hpp>

#include <roofer/common/Raster.hpp>
#include <roofer/misc/PointcloudRasteriser.hpp>

#include <cmath>
#include <limits>

namespace {
  const float inf = std::numeric_limits<float>::infinity();
  const float not_a_number = std::numeric_limits<float>::quiet_NaN();

  std::vector<roofer::arr3f> outside_points() {
    // For a 3x3 grid at (0, 0) with unit cells. The left/right cases used
    // to alias a valid cell in the preceding/following row.
    return {{-0.1F, 1.5F, 99},
            {3, 0.5F, 99},
            {0.5F, -0.1F, 99},
            {0.5F, 3, 99},
            {3, 3, 99},
            {inf, 1, 99},
            {-inf, 1, 99},
            {not_a_number, 1, 99},
            {1, inf, 99},
            {1, -inf, 99},
            {1, not_a_number, 99},
            {std::numeric_limits<float>::max(), 1, 99}};
  }

  void append(roofer::PointCollection& cloud, roofer::arr3f point, int cls) {
    cloud.push_back(point);
    cloud.attributes.get_if<int>("classification")->push_back(cls);
  }

  roofer::PointCollection make_cloud() {
    roofer::PointCollection cloud;
    cloud.attributes.insert_vec<int>("classification");
    return cloud;
  }
}  // namespace

TEST_CASE("raster rejects invalid coordinates without changing cells",
          "[raster]") {
  using namespace roofer::RasterTools;
  Raster raster(1, 0, 2, 0, 2);
  raster.prefill_arrays(ZERO);
  const auto before = *raster.vals_;
  for (const auto& point : outside_points()) {
    double x = point[0], y = point[1];
    CAPTURE(x, y);
    CHECK_FALSE(raster.check_point(x, y));
    CHECK(raster.getLinearCoord(x, y) == invalid_index);
    CHECK_FALSE(raster.add_value(x, y, 1));
    CHECK_FALSE(raster.add_point(x, y, 100, MAX));
    CHECK_FALSE(raster.add_point(x, y, -100, MIN));
    CHECK(raster.sample(x, y) == raster.getNoDataVal());
    CHECK(raster.isNoData(x, y));
  }
  CHECK(*raster.vals_ == before);
  CHECK(raster.getCol(-0.1, 1) == raster.dimx_);
  CHECK(raster.getRow(1, -0.1) == raster.dimy_);
  CHECK(raster.getCol(not_a_number, 1) == raster.dimx_);
  CHECK(raster.getRow(1, inf) == raster.dimy_);

  for (const auto& cell : std::vector<std::array<size_t, 2>>{
           {3, 0}, {0, 3}, {invalid_index, 1}, {1, invalid_index}}) {
    const auto col = cell[0], row = cell[1];
    CHECK(raster.getLinearCoord(row, col) == invalid_index);
    raster.set_val(col, row, 42);
    CHECK(raster.get_val(col, row) == raster.getNoDataVal());
    CHECK(raster.isNoData(col, row));
    CHECK(raster.getPointFromRasterCoords(col, row)[2] ==
          raster.getNoDataVal());
  }
  CHECK(*raster.vals_ == before);
}

TEST_CASE("raster retains existing cell boundary coverage", "[raster]") {
  using namespace roofer::RasterTools;
  Raster raster(0.5, -10, -9, 20, 21);
  raster.prefill_arrays(ZERO);
  REQUIRE(raster.dimx_ == 3);
  REQUIRE(raster.dimy_ == 3);
  CHECK(raster.getLinearCoord(-10., 20.) == 0);
  CHECK(raster.getLinearCoord(-9.5, 20.5) == 4);
  // The constructor deliberately includes the cell containing the input max.
  CHECK(raster.getLinearCoord(-9., 21.) == 8);
  CHECK(raster.getLinearCoord(std::nextafter(-8.5, -inf),
                              std::nextafter(21.5, -inf)) == 8);
  CHECK_FALSE(raster.check_point(-8.5, 20));
  CHECK_FALSE(raster.check_point(-10, 21.5));
  CHECK_FALSE(raster.check_point(std::nextafter(-10., -inf), 20));
  CHECK_FALSE(raster.check_point(-10, std::nextafter(20., -inf)));
  CHECK(raster.add_value(-9.5, 20.5, 2));
  CHECK_FALSE(raster.add_value(-9.5, 20.5, 3));
  CHECK(raster.get_val(1, 1) == 5);
  raster.add_point(-9.5, 20.5, 7, MAX);
  CHECK(raster.get_val(1, 1) == 7);
  raster.add_point(-9.5, 20.5, 4, MIN);
  CHECK(raster.get_val(1, 1) == 4);
}

TEST_CASE("outside points do not contaminate point cloud rasters", "[raster]") {
  roofer::LinearRing footprint;
  for (const auto& p :
       std::vector<roofer::arr3f>{{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}})
    footprint.push_back(p);
  auto clean = make_cloud();
  append(clean, {0.5F, 0.5F, 10}, 6);
  append(clean, {1.5F, 1.5F, 2}, 2);
  auto mixed = clean;
  for (auto p : outside_points()) append(mixed, p, 2);
  roofer::ImageMap expected, actual;
  roofer::misc::RasterisePointcloud(clean, footprint, expected, 1);
  roofer::misc::RasterisePointcloud(mixed, footprint, actual, 1);
  REQUIRE(actual.size() == expected.size());
  for (const auto& [name, image] : expected) {
    CAPTURE(name);
    CHECK(actual.at(name).array == image.array);
  }
}

TEST_CASE("outside points do not change seeded thinning decisions",
          "[raster]") {
  roofer::Image counts{};
  counts.dim_x = counts.dim_y = 3;
  counts.cellsize = 1;
  counts.array.assign(9, 100);
  auto clean = make_cloud();
  auto mixed = make_cloud();
  for (int i = 0; i < 100; ++i) {
    roofer::arr3f p{0.5F, 0.5F, float(i)};
    append(clean, p, 6);
    for (auto outside : outside_points()) append(mixed, outside, 2);
    append(mixed, p, 6);
  }
  roofer::misc::gridthinPointcloud(clean, counts, 50);
  roofer::misc::gridthinPointcloud(mixed, counts, 50);
  REQUIRE_FALSE(clean.empty());
  REQUIRE(clean.size() < 100);
  REQUIRE(mixed.size() == clean.size());
  for (size_t i = 0; i < clean.size(); ++i) CHECK(mixed[i] == clean[i]);
  CHECK(*mixed.attributes.get_if<int>("classification") ==
        *clean.attributes.get_if<int>("classification"));
}

TEST_CASE("thinning handles empty and invalid count grids", "[raster]") {
  roofer::Image counts{};
  counts.dim_x = counts.dim_y = 3;
  counts.cellsize = 1;
  SECTION("empty backing array") {}
  SECTION("empty grid dimensions") {
    counts.dim_x = counts.dim_y = 0;
    counts.array.assign(9, 1);
  }
  SECTION("invalid cell size") {
    counts.cellsize = not_a_number;
    counts.array.assign(9, 1);
  }
  SECTION("short backing array") { counts.array.assign(1, 1); }
  auto cloud = make_cloud();
  append(cloud, {1.5F, 1.5F, 10}, 6);
  roofer::misc::gridthinPointcloud(cloud, counts);
  CHECK(cloud.empty());
}
