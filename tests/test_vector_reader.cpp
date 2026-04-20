// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

#include <roofer/io/VectorReader.hpp>
#include <roofer/misc/projHelper.hpp>

#include <catch2/catch_test_macros.hpp>

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
}
