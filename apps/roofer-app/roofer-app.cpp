// Copyright (c) 2018-2025 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
// and Balazs Dukai (3DGI)

// This file is part of roofer (https://github.com/3DBAG/roofer)

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
// Ravi Peters
// Balazs Dukai

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
namespace fs = std::filesystem;

#include <stdlib.h>

// common
#include <roofer/logger/logger.h>
#include <roofer/misc/projHelper.hpp>
#include <roofer/common/datastructures.hpp>
#include <roofer/io/SpatialReferenceSystem.hpp>

// crop
#include <roofer/io/PointCloudReader.hpp>
#include <roofer/io/PointCloudWriter.hpp>
#include <roofer/io/RasterWriter.hpp>
#include <roofer/io/StreamCropper.hpp>
#include <roofer/io/VectorReader.hpp>
#include <roofer/io/VectorWriter.hpp>
#include <roofer/misc/NodataCircleComputer.hpp>
#include <roofer/misc/PointcloudRasteriser.hpp>
#include <roofer/misc/Vector2DOps.hpp>
#include <roofer/misc/select_pointcloud.hpp>
#if RF_USE_VAL3DITY
#include <roofer/misc/Val3dator.hpp>
#endif

// reconstruct
#include <roofer/misc/PC2MeshDistCalculator.hpp>
#include <roofer/misc/MeshPropertyCalculator.hpp>
#include <roofer/reconstruction/AlphaShaper.hpp>
#include <roofer/reconstruction/ArrangementBuilder.hpp>
#include <roofer/reconstruction/ArrangementDissolver.hpp>
#include <roofer/reconstruction/ArrangementExtruder.hpp>
#include <roofer/reconstruction/ArrangementOptimiser.hpp>
#include <roofer/reconstruction/ArrangementSnapper.hpp>
#include <roofer/reconstruction/LineDetector.hpp>
#include <roofer/reconstruction/LineRegulariser.hpp>
#include <roofer/reconstruction/MeshTriangulator.hpp>
#include <roofer/reconstruction/PlaneDetector.hpp>
#include <roofer/reconstruction/PlaneIntersector.hpp>
#include <roofer/reconstruction/SegmentRasteriser.hpp>
#include <roofer/reconstruction/SimplePolygonExtruder.hpp>

// serialisation
#include <roofer/io/CityJsonWriter.hpp>

#include "BS_thread_pool.hpp"

#ifdef RF_USE_RERUN
#include <rerun.hpp>
#endif

#if defined(IS_LINUX) || defined(IS_MACOS)
#include <new>
#include <mimalloc-override.h>
#else
#undef RF_ENABLE_HEAP_TRACING
#endif
#include "allocators.hpp"

#include "config.hpp"
#include "types.hpp"
#include "BuildingTileSerializer.hpp"

// Structures defined in types.hpp

#include "crop_tile.hpp"
#include "reconstruct_building.hpp"

void get_las_extents(InputPointcloud& ipc,
                     roofer::io::SpatialReferenceSystemInterface* srs) {
  auto pj = roofer::misc::createProjHelper();
  for (auto& fp : ipc.paths) {
    auto PointReader = roofer::io::createPointCloudReaderLASlib(*pj);
    PointReader->open(fp);
    if (!srs->is_valid()) PointReader->get_crs(srs);
    ipc.file_extents.push_back(std::make_pair(fp, PointReader->getExtent()));
  }
}

int main(int argc, const char* argv[]) {
  auto& logger = roofer::logger::Logger::get_logger();

  // read cmdl options
  CLIArgs cli_args(argc, argv);
  RooferConfigHandler handler;

  // Parse basic command line arguments (not yet the configuration parameters)
  try {
    handler.parse_cli_first_pass(cli_args);
  } catch (const std::exception& e) {
    logger.error("Failed to parse command line arguments.");
    logger.error("{} Use '-h' to print usage information.", e.what());
    // handler.print_help(cli_args.program_name);
    return EXIT_FAILURE;
  }
  if (handler._print_help) {
    handler.print_help(cli_args.program_name);
    return EXIT_SUCCESS;
  }
  if (handler._print_attributes) {
    handler.print_attributes();
    return EXIT_SUCCESS;
  }
  if (handler._print_version) {
    handler.print_version();
    return EXIT_SUCCESS;
  }

  // Helper lambda for error handling
  auto handle_config_error = [&](const std::string& context,
                                 const std::exception& e) -> int {
    logger.error("Failed to {}: {}. Use '-h' for usage information.", context,
                 e.what());
    return EXIT_FAILURE;
  };

  // Read configuration file if provided
  if (!handler._config_path.empty()) {
    logger.info("Reading configuration from file {}", handler._config_path);
    try {
      handler.parse_config_file();
    } catch (const std::exception& e) {
      return handle_config_error("parse config file " + handler._config_path,
                                 e);
    }
  }

  // Parse command line arguments (these override config file values)
  try {
    handler.parse_cli_second_pass(cli_args);
  } catch (const std::exception& e) {
    return handle_config_error("parse command line arguments", e);
  }

  // validate configuration parameters
  try {
    handler.validate();
  } catch (const std::exception& e) {
    logger.error(
        "Failed to validate parameter values. {} Use '-h' to print usage "
        "information.",
        e.what());
    // handler.print_help(cli_args.program_name);
    return EXIT_FAILURE;
  }

  bool do_tracing = false;
  auto trace_interval = std::chrono::seconds(handler._trace_interval);
  logger.set_level(handler._loglevel);
  if (handler._loglevel == roofer::logger::LogLevel::trace) {
    trace_interval = std::chrono::seconds(handler._trace_interval);
    do_tracing = true;
    logger.debug("trace interval is set to {} seconds", trace_interval.count());
  }

  auto project_srs = roofer::io::createSpatialReferenceSystemOGR();

  if (!handler.cfg_.srs_override.empty()) {
    project_srs->import(handler.cfg_.srs_override);
    if (!project_srs->is_valid()) {
      logger.error("Invalid user override SRS: {}", handler.cfg_.srs_override);
      return EXIT_FAILURE;
    } else {
      logger.info("Using user override SRS: {}", handler.cfg_.srs_override);
    }
  }

  logger.debug("{}", handler);

  for (auto& ipc : handler.input_pointclouds_) {
    get_las_extents(ipc, project_srs.get());
    ipc.rtree = roofer::misc::createRTreeGEOS();
    for (auto& item : ipc.file_extents) {
      ipc.rtree->insert(item.second, &item);
    }
  }

  auto pj = roofer::misc::createProjHelper();
  auto VectorReader = roofer::io::createVectorReaderOGR(*pj);
  VectorReader->layer_name = handler.cfg_.layer_name;
  VectorReader->layer_id = handler.cfg_.layer_id;
  VectorReader->attribute_filter = handler.cfg_.attribute_filter;
  try {
    VectorReader->open(handler.cfg_.source_footprints);
  } catch (const std::exception& e) {
    logger.error("{}", e.what());
    return EXIT_FAILURE;
  }
  if (!project_srs->is_valid()) {
    VectorReader->get_crs(project_srs.get());
  }

  roofer::TBox<double> roi;
  if (handler.cfg_.region_of_interest.has_value()) {
    roi = *handler.cfg_.region_of_interest;
  } else {
    roi = VectorReader->layer_extent;
  }

  // Create building tile with basic setup
  BuildingTile building_tile;
  building_tile.id = 0;
  building_tile.extent = roi;
  building_tile.proj_helper = roofer::misc::createProjHelper();

  // Stage 1: Crop point clouds to extract building footprints and point data
  std::vector<BuildingObject> cropped_buildings;
  roofer::AttributeVecMap tile_attributes;
  try {
    logger.debug("[cropper] Cropping tile to extract building data");
    auto [buildings, attributes] =
        crop_tile(roi, handler.input_pointclouds_, handler.cfg_,
                  project_srs.get(), *building_tile.proj_helper);
    cropped_buildings = std::move(buildings);
    tile_attributes = std::move(attributes);
    logger.info("[cropper] Successfully cropped {} buildings",
                cropped_buildings.size());
  } catch (const std::exception& e) {
    logger.error("[cropper] Failed to crop tile: {}", e.what());
    return EXIT_FAILURE;
  } catch (...) {
    logger.error("[cropper] Failed to crop tile: unknown error");
    return EXIT_FAILURE;
  }

  // Stage 2: Reconstruct 3D models for each building
  std::vector<BuildingObject> reconstructed_buildings;
  try {
    logger.debug("[reconstructor] Processing {} buildings",
                 cropped_buildings.size());
    reconstructed_buildings.reserve(cropped_buildings.size());

    for (auto& building : cropped_buildings) {
      auto reconstructed_building =
          reconstruct_building(building, &handler.cfg_);
      reconstructed_buildings.push_back(std::move(reconstructed_building));
    }

    logger.info("[reconstructor] Successfully reconstructed {} buildings",
                reconstructed_buildings.size());
  } catch (const std::exception& e) {
    logger.error("[reconstructor] Failed to reconstruct buildings: {}",
                 e.what());
    return EXIT_FAILURE;
  }

  // Stage 3: Prepare final building tile for serialization
  building_tile.buildings = std::move(reconstructed_buildings);
  building_tile.attributes = std::move(tile_attributes);

  logger.info("[serializer] Serializing tile {} with {} buildings",
              building_tile.id, building_tile.buildings.size());

  // Create and use the serializer
  BuildingTileSerializer serializer(handler.cfg_, project_srs.get());
  bool serialization_success = serializer.serialize(building_tile);

  if (!serialization_success) {
    logger.error("[serializer] Failed to serialize tile {}", building_tile.id);
    return EXIT_FAILURE;
  }

  logger.info("[serializer] Successfully serialized tile {}", building_tile.id);
  return EXIT_SUCCESS;
}
