// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of geoflow-roofer (https://github.com/3DBAG/geoflow-roofer)

// geoflow-roofer was created as part of the 3DBAG project by the TU Delft 3D
// geoinformation group (3d.bk.tudelf.nl) and 3DGI (3dgi.nl)

// geoflow-roofer is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version. geoflow-roofer is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
// Public License for more details. You should have received a copy of the GNU
// General Public License along with geoflow-roofer. If not, see
// <https://www.gnu.org/licenses/>.

// Author(s):
// Ivan Paden

#include <CGAL/IO/OBJ.h>
#include <roofer/logger/logger.h>
#include <roofer/roofer.h>

#include <roofer/io/PointCloudReader.hpp>
#include <roofer/io/VectorReader.hpp>
#include <roofer/misc/cgal_utils.hpp>

#include "argh.h"
#include "fmt/format.h"
#include "git.h"

typedef CGAL::Exact_predicates_inexact_constructions_kernel K;
typedef CGAL::Point_3<K> Point_3;

void print_help(std::string program_name) {
  // see http://docopt.org/
  std::cout << "Usage:" << "\n";
  std::cout << "   " << program_name;
  std::cout << "Options:" << "\n";
  std::cout << "   -h, --help                   Show this help message" << "\n";
  std::cout << "   -V, --version                Show version" << "\n";
  std::cout << "   -v, --verbose                Be more verbose" << "\n";
}
// ... get the input pointcloud and footprint polygon for your building
void print_version() {
  std::cout << fmt::format(
      "roofer {} ({}{}{})\n", git_Describe(),
      std::strcmp(git_Branch(), "main") ? ""
                                        : fmt::format("{}, ", git_Branch()),
      git_AnyUncommittedChanges() ? "dirty, " : "", git_CommitDate());
}

int main(int argc, const char* argv[]) {
  auto cmdl = argh::parser();

  cmdl.parse(argc, argv);
  std::string program_name = cmdl[0];

  bool verbose = cmdl[{"-v", "--verbose"}];
  bool version = cmdl[{"-V", "--version"}];

  if (cmdl[{"-h", "--help"}]) {
    print_help(program_name);
    return EXIT_SUCCESS;
  }
  if (version) {
    print_version();
    return EXIT_SUCCESS;
  }

  auto& logger = roofer::logger::Logger::get_logger();

  if (verbose) {
    logger.set_level(roofer::logger::LogLevel::debug);
  } else {
    logger.set_level(roofer::logger::LogLevel::warning);
  }
  std::string path_pointcloud =
      "data/wippolder/objects/503100000030812/crop/"
      "503100000030812_pointcloud.las";
  std::string path_footprint =
      "data/wippolder/objects/503100000030812/crop/503100000030812.gpkg";
  float floor_elevation = -0.16899998486042023;

  // read inputs
  auto pj = roofer::misc::createProjHelper();
  auto PointReader = roofer::io::createPointCloudReaderLASlib(*pj);
  auto VectorReader = roofer::io::createVectorReaderOGR(*pj);

  VectorReader->open(path_footprint);
  std::vector<roofer::LinearRing> footprints;
  VectorReader->readPolygons(footprints);

  PointReader->open(path_pointcloud);
  logger.info("Reading pointcloud from {}", path_pointcloud);
  roofer::vec1i classification;
  roofer::PointCollection points, points_ground, points_roof;
  roofer::AttributeVecMap attributes;
  PointReader->readPointCloud(points, &classification);
  logger.info("Read {} points", points.size());

  // split into ground and roof points
  for (size_t i = 0; i < points.size(); ++i) {
    if (2 == classification[i]) {
      points_ground.push_back(points[i]);
    } else if (6 == classification[i]) {
      points_roof.push_back(points[i]);
    }
  }
  logger.info("{} ground points and {} roof points", points_ground.size(),
              points_roof.size());

  // reconstruct
  logger.info("Reconstructing LoD2.2");
  auto mesh_lod22 =
      roofer::reconstruct(points_roof, points_ground, footprints.front(),
                          {.floor_elevation = floor_elevation,
                           .override_with_floor_elevation = true});

  logger.info("Reconstructing LoD1.3");
  auto mesh_lod13 =
      roofer::reconstruct(points_roof, points_ground, footprints.front(),
                          {.lod = 13,
                           .lod13_step_height = 2,
                           .floor_elevation = floor_elevation,
                           .override_with_floor_elevation = true});

  logger.info("Reconstructing LoD1.2");
  auto mesh_lod12 =
      roofer::reconstruct(points_roof, points_ground, footprints.front(),
                          {.lod = 12,
                           .floor_elevation = floor_elevation,
                           .override_with_floor_elevation = true});

  logger.info("Outputting to OBJ files");
  // lod22
  {
    int i = 0;
    for (auto& mesh : mesh_lod22) {
      CGAL::Surface_mesh<Point_3> cgalmesh =
          roofer::misc::Mesh2CGALSurfaceMesh<Point_3>(mesh);

      std::string filename("output_lod22_" + std::to_string(i) + ".obj");
      CGAL::IO::write_OBJ(filename, cgalmesh);
      ++i;
    }
  }

  // lod13
  {
    int i = 0;
    for (auto& mesh : mesh_lod13) {
      CGAL::Surface_mesh<Point_3> cgalmesh =
          roofer::misc::Mesh2CGALSurfaceMesh<Point_3>(mesh);

      std::string filename("output_lod13_" + std::to_string(i) + ".obj");
      CGAL::IO::write_OBJ(filename, cgalmesh);
      ++i;
    }
  }

  // lod12
  {
    int i = 0;
    for (auto& mesh : mesh_lod12) {
      CGAL::Surface_mesh<Point_3> cgalmesh =
          roofer::misc::Mesh2CGALSurfaceMesh<Point_3>(mesh);

      std::string filename("output_lod12_" + std::to_string(i) + ".obj");
      CGAL::IO::write_OBJ(filename, cgalmesh);
      ++i;
    }
  }

  logger.info("Completed reconstruction");
}
