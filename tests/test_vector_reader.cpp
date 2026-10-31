// Copyright (c) 2018-2026 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

#include <roofer/io/VectorReader.hpp>
#include <roofer/misc/projHelper.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include <fstream>

namespace {
  struct TemporaryVectorSource {
    std::filesystem::path path;
    TemporaryVectorSource(const char* filename, const char* contents)
        : path(std::filesystem::temp_directory_path() / filename) {
      std::ofstream(path) << contents;
    }
    ~TemporaryVectorSource() {
      std::error_code error;
      std::filesystem::remove(path, error);
    }
  };
}  // namespace

TEST_CASE("VectorReaderOGR skips invalid polygons when requested") {
  auto pj = roofer::misc::createProjHelper();
  auto vector_reader = roofer::io::createVectorReaderOGR(*pj);
  vector_reader->skip_invalid_polygons = true;
  vector_reader->open("fixtures/issue-134-mixed.geojson");

  std::vector<roofer::LinearRing> footprints;
  roofer::AttributeVecMap attributes;
  vector_reader->readPolygons(footprints, &attributes);

  REQUIRE(footprints.size() == 1);

  auto ids = attributes.get_if<std::string>("id");
  REQUIRE(ids != nullptr);
  REQUIRE(ids->size() == 1);
  REQUIRE(ids->front().has_value());
  CHECK(ids->front().value() == "valid-building");

  std::vector<roofer::io::PolygonFeature> original;
  vector_reader->readPolygons(original, {"id"}, {});
  REQUIRE(original.size() == 1);
  CHECK(original.front().text_attributes.at("id") == "valid-building");
}

TEST_CASE(
    "VectorReaderOGR retains original double coordinates and selected "
    "attributes") {
  TemporaryVectorSource input("roofer-vector-precision.geojson", R"({
    "type":"FeatureCollection","features":[{"type":"Feature",
    "properties":{"bid":9007199254740991,"ceiling":123.456789123,"missing":null,"text":"not-numeric"},
    "geometry":{"type":"MultiPolygon","coordinates":[
      [[[122000.123456789,485000.123456789,12],[122010,485000,12],[122010,485010,12],[122000.123456789,485000.123456789,12]]],
      [[[122020,485000,12],[122030,485000,12],[122030,485010,12],[122020,485010,12],[122020,485000,12]],
       [[122024,485004,12],[122024,485006,12],[122026,485006,12],[122026,485004,12],[122024,485004,12]]]
    ]}}]})");
  auto pj = roofer::misc::createProjHelper();
  auto reader = roofer::io::createVectorReaderOGR(*pj);
  reader->open(input.path.string());
  std::vector<roofer::io::PolygonFeature> polygons;
  reader->readPolygons(polygons, {"bid", "missing"}, {"ceiling"});
  REQUIRE(polygons.size() == 2);
  CHECK_FALSE(pj->data_offset.has_value());
  CHECK(polygons[0].exterior[0][0] ==
        Catch::Approx(122000.123456789).margin(1e-10).epsilon(0));
  CHECK(polygons[0].exterior[0][1] ==
        Catch::Approx(485000.123456789).margin(1e-10).epsilon(0));
  CHECK(polygons[0].text_attributes.at("bid") == "9007199254740991");
  CHECK_FALSE(polygons[0].text_attributes.at("missing"));
  CHECK(*polygons[0].numeric_attributes.at("ceiling") ==
        Catch::Approx(123.456789123).margin(1e-10).epsilon(0));
  CHECK(polygons[0].fid == polygons[1].fid);
  REQUIRE(polygons[1].holes.size() == 1);
  CHECK(polygons[1].holes[0].size() == 4);
  CHECK_THROWS(reader->readPolygons(polygons, {"absent"}, {}));
  CHECK_THROWS(reader->readPolygons(polygons, {}, {"absent"}));
  CHECK_THROWS(reader->readPolygons(polygons, {}, {"text"}));
  roofer::arr3d offset{122000, 485000, 10};
  pj->set_data_offset(offset);
  std::vector<roofer::LinearRing> local;
  reader->readPolygons(local);
  REQUIRE(local.size() == 2);
  CHECK(local[0][0][0] == Catch::Approx(0.123456789).margin(1e-7));
  CHECK(local[0][0][2] == 2);
  CHECK(local[1].interior_rings().size() == 1);
}

TEST_CASE("VectorReaderOGR applies the optional centroid filter consistently") {
  TemporaryVectorSource input("roofer-vector-filter.geojson", R"({
    "type":"FeatureCollection","features":[{"type":"Feature","properties":{"id":"crossing"},
    "geometry":{"type":"Polygon","coordinates":[[[0,0],[10,0],[10,10],[0,10],[0,0]]]}}]})");
  auto pj = roofer::misc::createProjHelper();
  auto reader = roofer::io::createVectorReaderOGR(*pj);
  reader->open(input.path.string());
  reader->region_of_interest = roofer::TBox<double>{1, 1, 0, 3, 3, 0};
  std::vector<roofer::io::PolygonFeature> raw;
  reader->readPolygons(raw);
  CHECK(raw.empty());
  reader->region_of_interest.reset();
  reader->readPolygons(raw);
  REQUIRE(raw.size() == 1);
  CHECK_FALSE(pj->data_offset.has_value());
  std::vector<roofer::LinearRing> local;
  reader->readPolygons(local);
  CHECK(local.size() == 1);
}
