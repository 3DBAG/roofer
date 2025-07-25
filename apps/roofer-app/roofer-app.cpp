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

// New BuildingFeature architecture
#include "BuildingFeature.hpp"
#include "BuildingFeatureSerializer.hpp"

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

int handle_configuration(int argc, const char* argv[],
                         RooferConfigHandler& handler) {
  auto& logger = roofer::logger::Logger::get_logger();
  logger.set_level(roofer::logger::LogLevel::info);
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

  // Read configuration file if provided
  if (!handler._config_path.empty()) {
    logger.info("Reading configuration from file {}", handler._config_path);
    try {
      handler.parse_config_file();
    } catch (const std::exception& e) {
      logger.error("Failed to parse config file {}: {}. Use '-h' for usage information.", handler._config_path, e.what());
      return EXIT_FAILURE;
    }
  }

  // Parse command line arguments (these override config file values)
  try {
    handler.parse_cli_second_pass(cli_args);
  } catch (const std::exception& e) {
    logger.error("Failed to parse command line arguments: {}. Use '-h' for usage information.", e.what());
    return EXIT_FAILURE;
  }

  // Validate configuration parameters
  try {
    handler.validate();
  } catch (const std::exception& e) {
    logger.error("Exception during configuration validation: {}", e.what());
    return EXIT_FAILURE;
  }

  // Configure logging
  logger.debug("Configuring logging with level: {}", static_cast<int>(handler._loglevel));
  logger.set_level(handler._loglevel);
  return 0;  // Success, continue processing
}

// ============================================================================
// SPATIAL REFERENCE SYSTEM SETUP
// ============================================================================
std::unique_ptr<roofer::io::SpatialReferenceSystemInterface>
setup_spatial_reference_system(const RooferConfigHandler& handler) {
  auto& logger = roofer::logger::Logger::get_logger();
  logger.info("Setting up spatial reference system");
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

  logger.info("Preparing point cloud data for {} input pointclouds",
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
// VECTOR DATA PREPARATION
// ============================================================================

bool read_vector_roi(const RooferConfigHandler& handler,
                     roofer::io::SpatialReferenceSystemInterface* project_srs,
                     roofer::TBox<double>& roi) {
  auto& logger = roofer::logger::Logger::get_logger();
  logger.info("Reading vector file to determine SRS and/or ROI");
  // Only read vector data if we need to determine ROI or SRS
  if (!project_srs->is_valid() ||
      !handler.cfg_.region_of_interest.has_value()) {
    logger.debug("Reading vector file to determine SRS and/or ROI");

    auto pj = roofer::misc::createProjHelper();
    auto VectorReader = roofer::io::createVectorReaderOGR(*pj);
    VectorReader->layer_name = handler.cfg_.layer_name;
    VectorReader->layer_id = handler.cfg_.layer_id;
    VectorReader->attribute_filter = handler.cfg_.attribute_filter;

    try {
      VectorReader->open(handler.cfg_.source_footprints);

      if (!project_srs->is_valid()) {
        VectorReader->get_crs(project_srs);
        logger.info("Extracted SRS from vector file");
      }

      // Determine region of interest
      if (handler.cfg_.region_of_interest.has_value()) {
        roi = *handler.cfg_.region_of_interest;
        logger.info("Using user-defined region of interest");
      } else {
        roi = VectorReader->layer_extent;
        logger.info("Using layer extent as region of interest");
      }

    } catch (const std::exception& e) {
      logger.error("Failed to read vector metadata: {}", e.what());
      return false;
    }
  } else {
    // We have both SRS and ROI from user config
    roi = *handler.cfg_.region_of_interest;
    logger.info("Using user-defined SRS and region of interest");
  }

  logger.info("Vector ROI determined: [{}, {}, {}, {}]", roi.min()[0],
              roi.min()[1], roi.max()[0], roi.max()[1]);
  return true;
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
    return config_result;  // EXIT_SUCCESS for help/version, EXIT_FAILURE for
                           // errors
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
  // 4. VECTOR ROI DETERMINATION
  // -------------------------------------------------------------------------
  roofer::TBox<double> roi;
  if (!read_vector_roi(handler, project_srs.get(), roi)) {
    return EXIT_FAILURE;
  }

  // -------------------------------------------------------------------------
  // 5. PROCESSING (CROP, RECONSTRUCT, SERIALIZE)
  // -------------------------------------------------------------------------
  auto proj_helper = roofer::misc::createProjHelper();

  BuildingFeatureCollection building_features;
  // Cropping stage - directly produces BuildingFeatures
  try {
    logger.info("Cropping buildings from point clouds...");
    building_features = crop_tile(roi, handler.input_pointclouds_, handler.cfg_, project_srs.get(), *proj_helper);
    logger.info("Cropped {} building features", building_features.size());

    if (building_features.empty()) {
      logger.error("No buildings found in region of interest");
      return EXIT_FAILURE;
    }
  } catch (const std::exception& e) {
    logger.error("Cropping failed: {}", e.what());
    return EXIT_FAILURE;
  }

  // Reconstruction stage - work with BuildingFeatures directly
  try {
    logger.info("Reconstructing 3D building models...");

    for (auto& feature_ptr : building_features) {
      auto& feature = *feature_ptr;

      // Reconstruct using the new function that handles attributes internally
      reconstruct_building_feature(feature, handler.cfg_);
    }

    logger.info("Successfully reconstructed {} buildings", building_features.size());
  } catch (const std::exception& e) {
    logger.error("Reconstruction failed: {}", e.what());
    return EXIT_FAILURE;
  }

  // Serialization stage - direct BuildingFeature serialization
  try {
    logger.info("Serializing to CityJSON...");
    BuildingFeatureSerializer serializer(handler.cfg_, project_srs.get());
    bool success = serializer.serialize(building_features, *proj_helper);

    if (success) {
      logger.info("Successfully processed and serialized {} building features", building_features.size());
    } else {
      logger.error("Failed to serialize building features");
      return EXIT_FAILURE;
    }
  } catch (const std::exception& e) {
    logger.error("Serialization failed: {}", e.what());
    return EXIT_FAILURE;
  }

  logger.info("Roofer processing completed successfully!");
  return EXIT_SUCCESS;
}
