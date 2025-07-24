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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

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

// ============================================================================
// CONFIGURATION HANDLING
// ============================================================================

int handle_configuration(int argc, const char* argv[], RooferConfigHandler& handler) {
  auto& logger = roofer::logger::Logger::get_logger();
  
  CLIArgs cli_args(argc, argv);

  // Parse basic command line arguments (help, version, etc.)
  try {
    handler.parse_cli_first_pass(cli_args);
  } catch (const std::exception& e) {
    logger.error("Failed to parse command line arguments.");
    logger.error("{} Use '-h' to print usage information.", e.what());
    return EXIT_FAILURE;
  }

  // Handle early exit options
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
      return handle_config_error("parse config file " + handler._config_path, e);
    }
  }

  // Parse command line arguments (these override config file values)
  try {
    handler.parse_cli_second_pass(cli_args);
  } catch (const std::exception& e) {
    return handle_config_error("parse command line arguments", e);
  }

  // Validate configuration parameters
  try {
    handler.validate();
  } catch (const std::exception& e) {
    logger.error(
        "Failed to validate parameter values. {} Use '-h' to print usage "
        "information.",
        e.what());
    return EXIT_FAILURE;
  }

  // Configure logging
  logger.set_level(handler._loglevel);
  if (handler._loglevel == roofer::logger::LogLevel::trace) {
    auto trace_interval = std::chrono::seconds(handler._trace_interval);
    logger.debug("trace interval is set to {} seconds", trace_interval.count());
  }

  logger.debug("{}", handler);
  return 0; // Success, continue processing
}

// ============================================================================
// SPATIAL REFERENCE SYSTEM SETUP
// ============================================================================

std::unique_ptr<roofer::io::SpatialReferenceSystemInterface> 
setup_spatial_reference_system(const RooferConfigHandler& handler) {
  auto& logger = roofer::logger::Logger::get_logger();
  auto project_srs = roofer::io::createSpatialReferenceSystemOGR();

  if (!handler.cfg_.srs_override.empty()) {
    project_srs->import(handler.cfg_.srs_override);
    if (!project_srs->is_valid()) {
      logger.error("Invalid user override SRS: {}", handler.cfg_.srs_override);
      return nullptr;
    } else {
      logger.info("Using user override SRS: {}", handler.cfg_.srs_override);
    }
  }

  return project_srs;
}

// ============================================================================
// POINT CLOUD DATA PREPARATION
// ============================================================================

bool prepare_pointcloud_data(RooferConfigHandler& handler,
                             roofer::io::SpatialReferenceSystemInterface* srs) {
  auto& logger = roofer::logger::Logger::get_logger();
  
  logger.debug("Preparing point cloud data for {} input pointclouds", 
               handler.input_pointclouds_.size());

  for (auto& ipc : handler.input_pointclouds_) {
    get_las_extents(ipc, srs);
    ipc.rtree = roofer::misc::createRTreeGEOS();
    for (auto& item : ipc.file_extents) {
      ipc.rtree->insert(item.second, &item);
    }
  }

  logger.info("Successfully prepared point cloud data");
  return true;
}

// ============================================================================
// VECTOR DATA READING
// ============================================================================

std::tuple<std::unique_ptr<roofer::io::VectorReaderInterface>, roofer::TBox<double>>
read_vector_data(const RooferConfigHandler& handler,
                 roofer::io::SpatialReferenceSystemInterface* project_srs) {
  auto& logger = roofer::logger::Logger::get_logger();
  
  logger.debug("Reading vector footprint data from: {}", handler.cfg_.source_footprints);

  auto pj = roofer::misc::createProjHelper();
  auto VectorReader = roofer::io::createVectorReaderOGR(*pj);
  VectorReader->layer_name = handler.cfg_.layer_name;
  VectorReader->layer_id = handler.cfg_.layer_id;
  VectorReader->attribute_filter = handler.cfg_.attribute_filter;

  try {
    VectorReader->open(handler.cfg_.source_footprints);

    if (!project_srs->is_valid()) {
      VectorReader->get_crs(project_srs);
    }
  } catch (const std::exception& e) {
    logger.error("Failed to read vector data: {}", e.what());
    throw;
  }

  // Determine region of interest
  roofer::TBox<double> roi;
  if (handler.cfg_.region_of_interest.has_value()) {
    roi = *handler.cfg_.region_of_interest;
    logger.info("Using user-defined region of interest");
  } else {
    roi = VectorReader->layer_extent;
    logger.info("Using layer extent as region of interest");
  }

  logger.info("Successfully read vector data with extent: [{}, {}, {}, {}]",
              roi.min()[0], roi.min()[1], 
              roi.max()[0], roi.max()[1]);

  return std::make_tuple(std::move(VectorReader), roi);
}

// ============================================================================
// CROPPING STAGE
// ============================================================================

std::tuple<std::vector<BuildingObject>, roofer::AttributeVecMap>
perform_cropping(const roofer::TBox<double>& roi,
                 std::vector<InputPointcloud>& input_pointclouds,
                 const RooferConfig& config,
                 roofer::io::SpatialReferenceSystemInterface* srs,
                 roofer::misc::projHelperInterface& proj_helper) {
  auto& logger = roofer::logger::Logger::get_logger();
  
  logger.info("[CROP] Starting cropping stage for region of interest");
  
  try {
    logger.debug("[CROP] Extracting building footprints and point data");
    auto [buildings, attributes] = crop_tile(roi, input_pointclouds, config, srs, proj_helper);
    
    logger.info("[CROP] Successfully cropped {} buildings", buildings.size());
    return std::make_tuple(std::move(buildings), std::move(attributes));
    
  } catch (const std::exception& e) {
    logger.error("[CROP] Failed to crop tile: {}", e.what());
    throw;
  } catch (...) {
    logger.error("[CROP] Failed to crop tile: unknown error");
    throw;
  }
}

// ============================================================================
// RECONSTRUCTION STAGE
// ============================================================================

std::vector<BuildingObject> 
perform_reconstruction(std::vector<BuildingObject>& cropped_buildings,
                      RooferConfig* config) {
  auto& logger = roofer::logger::Logger::get_logger();
  
  logger.info("[RECONSTRUCT] Starting reconstruction stage for {} buildings", 
              cropped_buildings.size());

  std::vector<BuildingObject> reconstructed_buildings;
  reconstructed_buildings.reserve(cropped_buildings.size());

  try {
    for (auto& building : cropped_buildings) {
      auto reconstructed_building = reconstruct_building(building, config);
      reconstructed_buildings.push_back(std::move(reconstructed_building));
    }

    logger.info("[RECONSTRUCT] Successfully reconstructed {} buildings",
                reconstructed_buildings.size());
    return reconstructed_buildings;
    
  } catch (const std::exception& e) {
    logger.error("[RECONSTRUCT] Failed to reconstruct buildings: {}", e.what());
    throw;
  }
}

// ============================================================================
// SERIALIZATION STAGE
// ============================================================================

bool perform_serialization(BuildingTile& building_tile,
                           const RooferConfig& config,
                           roofer::io::SpatialReferenceSystemInterface* srs) {
  auto& logger = roofer::logger::Logger::get_logger();
  
  logger.info("[SERIALIZE] Starting serialization for tile {} with {} buildings",
              building_tile.id, building_tile.buildings.size());

  try {
    BuildingTileSerializer serializer(config, srs);
    bool success = serializer.serialize(building_tile);

    if (success) {
      logger.info("[SERIALIZE] Successfully serialized tile {}", building_tile.id);
    } else {
      logger.error("[SERIALIZE] Failed to serialize tile {}", building_tile.id);
    }

    return success;
  } catch (const std::exception& e) {
    logger.error("[SERIALIZE] Serialization failed: {}", e.what());
    return false;
  }
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================

int main(int argc, const char* argv[]) {
  auto& logger = roofer::logger::Logger::get_logger();

  // -------------------------------------------------------------------------
  // 1. CONFIGURATION
  // -------------------------------------------------------------------------
  RooferConfigHandler handler;
  int config_result = handle_configuration(argc, argv, handler);
  if (config_result != 0) {
    return config_result; // EXIT_SUCCESS for help/version, EXIT_FAILURE for errors
  }

  // -------------------------------------------------------------------------
  // 2. SPATIAL REFERENCE SYSTEM SETUP
  // -------------------------------------------------------------------------
  auto project_srs = setup_spatial_reference_system(handler);
  if (!project_srs) {
    return EXIT_FAILURE;
  }

  // -------------------------------------------------------------------------
  // 3. POINT CLOUD DATA PREPARATION
  // -------------------------------------------------------------------------
  if (!prepare_pointcloud_data(handler, project_srs.get())) {
    return EXIT_FAILURE;
  }

  // -------------------------------------------------------------------------
  // 4. VECTOR DATA READING
  // -------------------------------------------------------------------------
  roofer::TBox<double> roi;
  std::unique_ptr<roofer::io::VectorReaderInterface> VectorReader;
  try {
    std::tie(VectorReader, roi) = read_vector_data(handler, project_srs.get());
  } catch (const std::exception& e) {
    return EXIT_FAILURE;
  }

  // -------------------------------------------------------------------------
  // 5. BUILDING TILE SETUP
  // -------------------------------------------------------------------------
  BuildingTile building_tile;
  building_tile.id = 0;
  building_tile.extent = roi;
  building_tile.proj_helper = roofer::misc::createProjHelper();

  // -------------------------------------------------------------------------
  // 6. CROPPING STAGE
  // -------------------------------------------------------------------------
  std::vector<BuildingObject> cropped_buildings;
  roofer::AttributeVecMap tile_attributes;
  try {
    std::tie(cropped_buildings, tile_attributes) = 
        perform_cropping(roi, handler.input_pointclouds_, handler.cfg_,
                        project_srs.get(), *building_tile.proj_helper);
  } catch (const std::exception& e) {
    return EXIT_FAILURE;
  }

  // -------------------------------------------------------------------------
  // 7. RECONSTRUCTION STAGE
  // -------------------------------------------------------------------------
  std::vector<BuildingObject> reconstructed_buildings;
  try {
    reconstructed_buildings = perform_reconstruction(cropped_buildings, &handler.cfg_);
  } catch (const std::exception& e) {
    return EXIT_FAILURE;
  }

  // -------------------------------------------------------------------------
  // 8. SERIALIZATION STAGE
  // -------------------------------------------------------------------------
  building_tile.buildings = std::move(reconstructed_buildings);
  building_tile.attributes = std::move(tile_attributes);

  bool serialization_success = perform_serialization(building_tile, handler.cfg_, project_srs.get());
  if (!serialization_success) {
    return EXIT_FAILURE;
  }

  logger.info("Roofer processing completed successfully!");
  return EXIT_SUCCESS;
}
