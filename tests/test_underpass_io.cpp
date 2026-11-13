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
      [[[1020,2000],[1025,2000],[1025,2005],[1020,2005],[1020,2000]]]]}}
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

TEST_CASE(
    "null and unset underpass heights use the configured floor clearance") {
  Source source(R"({"type":"FeatureCollection","features":[
    {"type":"Feature","properties":{"bid":"null","z":null},"geometry":{
      "type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}},
    {"type":"Feature","properties":{"bid":"unset"},"geometry":{
      "type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}},
    {"type":"Feature","properties":{"bid":"zero","z":0.0},"geometry":{
      "type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}},
    {"type":"Feature","properties":{"bid":"explicit","z":123.456},"geometry":{
      "type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}}
  ]})");
  const auto index =
      roofer::io::readUnderpasses(source.path.string(), "", "bid", "z");
  REQUIRE(index.size() == 4);
  for (const auto& id : {"null", "unset"}) {
    const auto& input = index.at(id).at(0);
    CHECK_FALSE(input.ceiling_elevation);
    // The floor is already local: use floor + clearance without subtracting
    // the coordinate offset again.
    CHECK(input.localize({0, 0, 100}, -98.5F, 2.5F).ceiling_elevation ==
          Catch::Approx(-96.0));
    CHECK(input.localize({0, 0, 100}, -98.5F, 4.0F).ceiling_elevation ==
          Catch::Approx(-94.5));
  }
  // Explicit values, including zero, remain absolute elevations.
  CHECK(index.at("zero")
            .at(0)
            .localize({0, 0, 100}, -98.5F, 2.5F)
            .ceiling_elevation == -100.0);
  CHECK(index.at("explicit")
            .at(0)
            .localize({0, 0, 100}, -98.5F, 2.5F)
            .ceiling_elevation == Catch::Approx(23.456));
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

TEST_CASE(
    "selected underpass attributes retain types, nulls and multipart "
    "ownership") {
  Source source(R"({"type":"FeatureCollection","features":[
    {"type":"Feature","properties":{"bid":"test","name":"passage","open":true,
      "count":3,"large_id":9007199254740993,"height":4.123456789,"unknown":null,
      "ignored":"omit"},"geometry":{"type":"MultiPolygon","coordinates":[
      [[[0,0],[4,0],[4,4],[0,4],[0,0]]],
      [[[6,0],[10,0],[10,4],[6,4],[6,0]]]]}},
    {"type":"Feature","properties":{"bid":"unset"},"geometry":{
      "type":"Polygon","coordinates":[[[0,0],[1,0],[1,1],[0,1],[0,0]]]}}
  ]})");
  const std::vector<std::string> names{"name",     "open",   "count",
                                       "large_id", "height", "unknown"};
  const auto index = roofer::io::readUnderpasses(source.path.string(), "",
                                                 "bid", "", "", names);
  REQUIRE(index.at("test").size() == 2);
  for (const auto& feature : index.at("test")) {
    const auto local = feature.localize({0, 0, 0}, 0, 4);
    const auto& attributes = local.attributes;
    CHECK(attributes._attributes.size() == names.size());
    CHECK(*attributes.get_if<std::string>("name") == "passage");
    CHECK(*attributes.get_if<bool>("open"));
    CHECK(*attributes.get_if<int>("count") == 3);
    CHECK(*attributes.get_if<std::int64_t>("large_id") == 9007199254740993LL);
    CHECK(*attributes.get_if<double>("height") == 4.123456789);
    CHECK(attributes.is_null("unknown"));
    CHECK_FALSE(attributes.has_name("ignored"));
  }
  for (const auto& name : names)
    CHECK(index.at("unset")[0].attributes.is_null(name));
  CHECK_THROWS(roofer::io::readUnderpasses(source.path.string(), "", "bid", "",
                                           "", {"missing"}));
  const auto defaults =
      roofer::io::readUnderpasses(source.path.string(), "", "bid", "");
  CHECK(defaults.at("test")[0].attributes._attributes.empty());
}

TEST_CASE("underpass fixture loads polygons and preserves pillar holes") {
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
        2);  // One source polygon is self-intersecting and is skipped.
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
  mesh.get_attributes().resize(2);
  auto& ceiling_attributes = mesh.get_attributes()[0];
  ceiling_attributes.insert("name", std::string("passage"));
  ceiling_attributes.insert("open", true);
  ceiling_attributes.insert("count", 3);
  ceiling_attributes.insert("large_id", std::int64_t(9007199254740993LL));
  ceiling_attributes.insert("height", 4.123456789);
  ceiling_attributes.set_null("unknown");
  // Source fields cannot replace the CityJSON semantic type.
  ceiling_attributes.insert("type", std::string("input type"));
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
  const auto& ceiling_semantic =
      semantics["surfaces"][semantics["values"][0][0].get<int>()];
  CHECK(ceiling_semantic["name"] == "passage");
  CHECK(ceiling_semantic["open"] == true);
  CHECK(ceiling_semantic["count"] == 3);
  CHECK(ceiling_semantic["large_id"] == 9007199254740993LL);
  CHECK(ceiling_semantic["height"] == 4.123456789);
  CHECK(ceiling_semantic["unknown"].is_null());
  CHECK_FALSE(
      semantics["surfaces"][semantics["values"][0][1].get<int>()].contains(
          "name"));
}
