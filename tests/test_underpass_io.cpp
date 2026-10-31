#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <roofer/io/UnderpassReader.hpp>
#include <roofer/io/CityJsonWriter.hpp>
#include <roofer/misc/projHelper.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
  class Source {
   public:
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 "roofer-underpass-reader.geojson";
    explicit Source(std::string_view text) { std::ofstream(path) << text; }
    ~Source() {
      std::error_code error;
      std::filesystem::remove(path, error);
    }
  };
}  // namespace

TEST_CASE(
    "underpass source preserves numeric IDs, multipart geometry, holes and "
    "local offsets") {
  Source source(R"({"type":"FeatureCollection","features":[
    {"type":"Feature","properties":{"bid":"0123456789012345","z":123.456},"geometry":{
      "type":"MultiPolygon","coordinates":[[[[1000,2000],[1010,2000],[1010,2010],[1000,2010],[1000,2000]],
      [[1004,2004],[1004,2006],[1006,2006],[1006,2004],[1004,2004]]],
      [[[1020,2000],[1025,2000],[1025,2005],[1020,2005],[1020,2000]]]]}},
    {"type":"Feature","properties":{"bid":"invalid-height","z":null},"geometry":{
      "type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}}
  ]})");
  auto index =
      roofer::io::readUnderpasses(source.path.string(), "", "bid", "z");
  REQUIRE(index.size() == 1);
  auto& values = index.at("0123456789012345");
  REQUIRE(values.size() == 2);
  auto local = values[0].localize({1000, 2000, 100}, 1, 4);
  CHECK(local.ceiling_elevation == Catch::Approx(23.456));
  CHECK(local.polygon.front() == roofer::arr3f{0, 0, 0});
  REQUIRE(local.polygon.interior_rings().size() == 1);
  CHECK(local.polygon.interior_rings()[0].front() == roofer::arr3f{4, 4, 0});
  CHECK_THROWS(
      roofer::io::readUnderpasses(source.path.string(), "missing", "bid", "z"));
  CHECK_THROWS(
      roofer::io::readUnderpasses(source.path.string(), "", "missing", "z"));
  CHECK_THROWS(
      roofer::io::readUnderpasses(source.path.string(), "", "bid", "missing"));
  CHECK_THROWS(roofer::io::readUnderpasses(source.path.string(), "", "bid", "z",
                                           "EPSG:28992"));
}

TEST_CASE("four metre default underpass clearance uses the local floor") {
  roofer::io::UnderpassFeature input;
  input.exterior = {
      {122000.123456, 485000.123456}, {122010, 485000}, {122010, 485010}};
  auto local = input.localize({122000, 485000, 50}, -48.5F, 4.0F);
  CHECK(local.ceiling_elevation == Catch::Approx(-44.5));
  CHECK(local.polygon[0][0] == Catch::Approx(0.123456));
  CHECK(local.polygon[0][1] == Catch::Approx(0.123456));
  // A caller using feet converts the metre-based clearance, not absolute Z.
  CHECK(input.localize({0, 0, 0}, 5, 4 / 0.3048F).ceiling_elevation ==
        Catch::Approx(5 + 4 / 0.3048F));
}

TEST_CASE("database tile fixture loads polygons and preserves pillar holes") {
  const auto index = roofer::io::readUnderpasses(
      std::string(UNDERPASS_DATA_DIR) + "/underpasses.geojson", "",
      "identificatie", "", "EPSG:28992");
  std::size_t polygons = 0, holes = 0;
  for (const auto& [id, features] : index) {
    for (const auto& input : features) {
      ++polygons;
      auto local = input.localize({122000, 485000, 0}, 0, 4);
      holes += local.polygon.interior_rings().size();
      CHECK(local.ceiling_elevation == 4);
    }
  }
  CHECK(polygons ==
        93);  // One source polygon is self-intersecting and is skipped.
  CHECK(holes > 0);
}

TEST_CASE(
    "CityJSON writes ceiling semantics without treating ceilings as roofs") {
  roofer::LinearRing ceiling;
  ceiling.insert(ceiling.end(), {{0, 0, 4}, {0, 2, 4}, {2, 2, 4}, {2, 0, 4}});
  roofer::Mesh mesh;
  mesh.push_polygon(ceiling, 4);
  auto roof = ceiling;
  roof.set_z(10);
  mesh.push_polygon(roof, 1);
  std::unordered_map<int, roofer::Mesh> geometry{{0, mesh}};
  auto helper = roofer::misc::createProjHelper();
  auto writer = roofer::io::createCityJsonWriter(*helper);
  writer->identifier_attribute = "id";
  roofer::AttributeMapRow attributes;
  attributes.insert("id", std::string("test"));
  std::stringstream stream;
  writer->write_feature(stream, ceiling, nullptr, nullptr, &geometry,
                        attributes);
  const auto json = nlohmann::json::parse(stream.str());
  const auto& solid = json["CityObjects"]["test-0"]["geometry"][0];
  const auto& semantics = solid["semantics"];
  CHECK(semantics["values"][0].size() == 2);
  CHECK(semantics["surfaces"][semantics["values"][0][0].get<int>()]["type"] ==
        "OuterCeilingSurface");
  CHECK(semantics["surfaces"][semantics["values"][0][1].get<int>()]["type"] ==
        "RoofSurface");
}
