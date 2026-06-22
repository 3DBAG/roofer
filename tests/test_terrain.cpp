#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <roofer/common/Raster.hpp>
#include <roofer/io/CityJsonWriter.hpp>
#include <roofer/io/StreamCropper.hpp>
#include <roofer/misc/projHelper.hpp>

#include <sstream>

#include "../apps/roofer-app/config.hpp"

namespace {

  roofer::RasterTools::Raster make_grid(float lower_left, float lower_right,
                                        float upper_left, float upper_right) {
    roofer::RasterTools::Raster grid(10.0, 0.0, 10.0, 0.0, 10.0);
    grid.prefill_arrays(roofer::RasterTools::MIN);
    grid.set_val(0, 0, lower_left);
    grid.set_val(1, 0, lower_right);
    grid.set_val(0, 1, upper_left);
    grid.set_val(1, 1, upper_right);
    return grid;
  }

  roofer::LinearRing make_triangle(const roofer::arr3f& a,
                                   const roofer::arr3f& b,
                                   const roofer::arr3f& c) {
    roofer::LinearRing triangle;
    triangle.push_back(a);
    triangle.push_back(b);
    triangle.push_back(c);
    return triangle;
  }

}  // namespace

TEST_CASE(
    "terrain source selection uses quality, date, and configuration order") {
  std::vector<InputPointcloud> pointclouds(4);
  pointclouds[0].quality = 2;
  pointclouds[0].date = 2024;
  pointclouds[1].quality = 1;
  pointclouds[1].date = 2022;
  pointclouds[2].quality = 1;
  pointclouds[2].date = 2023;
  pointclouds[3].quality = 0;
  pointclouds[3].date = 2025;
  pointclouds[3].select_only_for_date = true;

  REQUIRE(select_terrain_pointcloud(pointclouds) == 2);
  pointclouds[1].date = 2023;
  REQUIRE(select_terrain_pointcloud(pointclouds) == 1);
}

TEST_CASE("terrain source selection rejects date-only sources") {
  std::vector<InputPointcloud> pointclouds(2);
  pointclouds[0].select_only_for_date = true;
  pointclouds[1].select_only_for_date = true;

  CHECK_FALSE(select_terrain_pointcloud(pointclouds).has_value());
}

TEST_CASE("terrain grid triangulation uses cell centres and tie diagonal") {
  auto grid = make_grid(1.0, 1.0, 1.0, 1.0);
  const auto triangles = roofer::io::triangulateTerrainGrid(grid);

  REQUIRE(triangles.size() == 2);
  CHECK(triangles[0][0] == roofer::arr3f{5.0F, 5.0F, 1.0F});
  CHECK(triangles[0][1] == roofer::arr3f{15.0F, 5.0F, 1.0F});
  CHECK(triangles[0][2] == roofer::arr3f{15.0F, 15.0F, 1.0F});
  CHECK(triangles[1][0] == roofer::arr3f{5.0F, 5.0F, 1.0F});
  CHECK(triangles[1][1] == roofer::arr3f{15.0F, 15.0F, 1.0F});
  CHECK(triangles[1][2] == roofer::arr3f{5.0F, 15.0F, 1.0F});
  CHECK(triangles[0].signed_area() > 0.0F);
  CHECK(triangles[1].signed_area() > 0.0F);
}

TEST_CASE("terrain grid triangulation selects the shortest 3D diagonal") {
  auto grid = make_grid(0.0, 2.0, 2.0, 10.0);
  const auto triangles = roofer::io::triangulateTerrainGrid(grid);

  REQUIRE(triangles.size() == 2);
  CHECK(triangles[0][2] == roofer::arr3f{5.0F, 15.0F, 2.0F});
  CHECK(triangles[1][0] == roofer::arr3f{15.0F, 5.0F, 2.0F});
}

TEST_CASE("terrain grid triangulation omits incomplete quads") {
  auto grid = make_grid(1.0, 1.0, 1.0, 1.0);
  grid.set_val(1, 1, grid.noDataVal_);

  CHECK(roofer::io::triangulateTerrainGrid(grid).empty());
}

TEST_CASE(
    "terrain grid triangulation emits a local triangle for three samples") {
  auto grid = make_grid(1.0, 2.0, 3.0, 4.0);
  grid.set_val(1, 1, grid.noDataVal_);

  const auto triangles = roofer::io::triangulateTerrainGrid(
      grid, roofer::io::TerrainNoDataMode::LOCAL_TRIANGLES);
  REQUIRE(triangles.size() == 1);
  CHECK(triangles[0].signed_area() > 0.0F);
  CHECK(triangles[0][0] == roofer::arr3f{5.0F, 5.0F, 1.0F});
  CHECK(triangles[0][1] == roofer::arr3f{15.0F, 5.0F, 2.0F});
  CHECK(triangles[0][2] == roofer::arr3f{5.0F, 15.0F, 3.0F});
}

TEST_CASE("terrain grid triangulation fills an isolated sample") {
  roofer::RasterTools::Raster grid(10.0, 0.0, 20.0, 0.0, 20.0);
  grid.prefill_arrays(roofer::RasterTools::MIN);
  for (size_t row = 0; row < 3; ++row) {
    for (size_t col = 0; col < 3; ++col) {
      grid.set_val(col, row, float(col + 2 * row));
    }
  }
  grid.set_val(1, 1, grid.noDataVal_);

  const auto triangles = roofer::io::triangulateTerrainGrid(
      grid, roofer::io::TerrainNoDataMode::FILL_SMALL_GAPS);
  REQUIRE(triangles.size() == 8);
  const auto filled_sample = roofer::arr3f{15.0F, 15.0F, 3.0F};
  CHECK(std::any_of(triangles.begin(), triangles.end(),
                    [&filled_sample](const auto& triangle) {
                      return std::find(triangle.begin(), triangle.end(),
                                       filled_sample) != triangle.end();
                    }));
}

TEST_CASE("terrain components require a shared edge") {
  const auto first =
      make_triangle({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
  const auto second =
      make_triangle({1.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F});
  const auto vertex_touching = make_triangle(
      {0.0F, 0.0F, 0.0F}, {-1.0F, 0.0F, 0.0F}, {0.0F, -1.0F, 0.0F});

  const auto components = roofer::io::splitTerrainConnectedComponents(
      {first, second, vertex_touching});
  REQUIRE(components.size() == 2);
  CHECK(components[0].size() == 2);
  CHECK(components[1].size() == 1);
}

TEST_CASE("terrain components split a globally connected bow-tie vertex") {
  const roofer::arr3f vertex{0.0F, 0.0F, 0.0F};
  const roofer::arr3f a{1.0F, 0.0F, 0.0F};
  const roofer::arr3f b{1.0F, 1.0F, 0.0F};
  const roofer::arr3f c{-1.0F, 1.0F, 0.0F};
  const roofer::arr3f d{-1.0F, 0.0F, 0.0F};
  const std::vector triangles{make_triangle(vertex, a, b),
                              make_triangle(a, c, b), make_triangle(a, d, c),
                              make_triangle(vertex, c, d)};

  const auto components =
      roofer::io::splitTerrainConnectedComponents(triangles);
  REQUIRE(components.size() == 2);
  CHECK(components[0].size() + components[1].size() == 4);
}

TEST_CASE("terrain is written as a CityJSON TINRelief feature") {
  auto grid = make_grid(1.0, 1.0, 1.0, 1.0);
  const auto triangles = roofer::io::triangulateTerrainGrid(grid);
  auto components = roofer::io::splitTerrainConnectedComponents(triangles);
  auto detached_triangle = triangles.front();
  for (auto& vertex : detached_triangle) vertex[0] += 100.0F;
  components.push_back({detached_triangle});
  auto proj_helper = roofer::misc::createProjHelper();
  auto writer = roofer::io::createCityJsonWriter(*proj_helper);
  writer->scale_x_ = 1.0;
  writer->scale_y_ = 1.0;
  writer->scale_z_ = 1.0;

  roofer::AttributeMapRow attributes;
  attributes.insert("rf_pc_source", std::string("best"));
  attributes.insert("rf_pc_quality", 0);
  std::stringstream output;
  writer->write_tin_relief_feature(output, "terrain-7", components, attributes);

  const auto json = nlohmann::json::parse(output.str());
  REQUIRE(json["type"] == "CityJSONFeature");
  REQUIRE(json["id"] == "terrain-7");
  const auto& terrain = json["CityObjects"]["terrain-7"];
  CHECK(terrain["type"] == "TINRelief");
  CHECK(terrain["attributes"]["rf_pc_source"] == "best");
  REQUIRE(terrain["geometry"].size() == 2);
  CHECK(terrain["geometry"][0]["type"] == "CompositeSurface");
  CHECK(terrain["geometry"][0]["lod"] == "1");
  CHECK(terrain["geometry"][0]["boundaries"].size() == 2);
  CHECK(terrain["geometry"][1]["boundaries"].size() == 1);
  CHECK(json["vertices"].size() == 7);
}
