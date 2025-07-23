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

enum ExtrusionMode { STANDARD, LOD11_FALLBACK, SKIP };

/**
 * @brief A single building object
 *
 * Contains the footprint polygon, the point cloud, the reconstructed model and
 * some attributes that are set during the reconstruction.
 */
struct BuildingObject {
  roofer::PointCollection pointcloud_ground;
  roofer::PointCollection pointcloud_building;
  roofer::LinearRing footprint;
  float z_offset = 0;

  std::unordered_map<int, roofer::Mesh> multisolids_lod12;
  std::unordered_map<int, roofer::Mesh> multisolids_lod13;
  std::unordered_map<int, roofer::Mesh> multisolids_lod22;

  size_t attribute_index;
  bool reconstruction_success = false;
  int reconstruction_time = 0;

  // set in crop
  fs::path jsonl_path;
  float h_ground;       // without offset
  float h_pc_98p;       // without offset
  float h_pc_roof_70p;  // with offset!
  bool force_lod11;     // force_lod11 / fallback_lod11
  bool pointcloud_insufficient;
  bool is_glass_roof;
  std::optional<float> roof_h_fallback;
  ExtrusionMode extrusion_mode = STANDARD;

  // set in reconstruction
  // optionals may not get assigned a valid value
  std::string roof_type = "unknown";
  std::optional<float> roof_elevation_50p;
  std::optional<float> roof_elevation_70p;
  std::optional<float> roof_elevation_min;
  std::optional<float> roof_elevation_max;
  std::optional<float> roof_elevation_ridge;
  std::optional<int> roof_n_planes;
  std::optional<float> rmse_lod12;
  std::optional<float> rmse_lod13;
  std::optional<float> rmse_lod22;
  std::optional<float> volume_lod12;
  std::optional<float> volume_lod13;
  std::optional<float> volume_lod22;
  std::optional<int> roof_n_ridgelines;
  std::optional<std::string> val3dity_lod12;
  std::optional<std::string> val3dity_lod13;
  std::optional<std::string> val3dity_lod22;
  // bool was_skipped;  // b3_reconstructie_onvolledig;
};

/**
 * @brief Indicates the progress of the object through the whole roofer process.
 */
enum Progress : std::uint8_t {
  CROP_NOT_STARTED,
  CROP_IN_PROGRESS,
  CROP_SUCCEEDED,
  CROP_FAILED,
  RECONSTRUCTION_IN_PROGRESS,
  RECONSTRUCTION_SUCCEEDED,
  RECONSTRUCTION_FAILED,
  SERIALIZATION_IN_PROGRESS,
  SERIALIZATION_SUCCEEDED,
  SERIALIZATION_FAILED,
};

auto format_as(Progress p) { return fmt::underlying(p); }

/**
 * @brief Used for passing a BuildingObject reference to the parallel
 * reconstructor.
 *
 * We cannot guarantee the order of reconstructed buildings, thus we need to
 * keep track of their tile and place in the BuildingTile.buildings container,
 * so that the BuildingTile.attributes will match the building after
 * reconstruction.
 *
 * ( BuildingTile.id, index of a BuildingObject in BuildingTile.buildings,
 * a BuildingObject from BuildingTile.buildings )
 */
struct BuildingObjectRef {
  size_t tile_id;
  size_t building_idx;
  BuildingObject building;
  Progress progress;
  BuildingObjectRef(size_t tile_id, size_t building_idx,
                    BuildingObject building, Progress progress)
      : tile_id(tile_id),
        building_idx(building_idx),
        building(building),
        progress(progress) {}
};

/**
 * @brief A single batch for processing
 *
 * It contains a tile ID, all the buildings of the tile, their attributes,
 * the progress of each building, the tile extent and projection information.
 * Buildings and attributes are stored in separate containers and they are
 * matched on their index in the container.
 */
struct BuildingTile {
  std::size_t id = 0;
  std::vector<BuildingObject> buildings;
  roofer::AttributeVecMap attributes;
  // offset
  std::unique_ptr<roofer::misc::projHelperInterface> proj_helper;
  // extent
  roofer::TBox<double> extent;
};

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

std::vector<roofer::TBox<double>> create_tiles(roofer::TBox<double>& roi,
                                               double tilesize_x,
                                               double tilesize_y) {
  size_t dimx_ = (roi.max()[0] - roi.min()[0]) / tilesize_x + 1;
  size_t dimy_ = (roi.max()[1] - roi.min()[1]) / tilesize_y + 1;
  std::vector<roofer::TBox<double>> tiles;

  for (size_t col = 0; col < dimx_; ++col) {
    for (size_t row = 0; row < dimy_; ++row) {
      tiles.emplace_back(roofer::TBox<double>{
          roi.min()[0] + col * tilesize_x, roi.min()[1] + row * tilesize_y, 0.,
          roi.min()[0] + (col + 1) * tilesize_x,
          roi.min()[1] + (row + 1) * tilesize_y, 0.});
    }
  }
  return tiles;
}

int main(int argc, const char* argv[]) {
  // auto cmdl = argh::parser({"-c", "--config"});
  // cmdl.add_params({"trace-interval", "-j", "--jobs"});

  // cmdl.parse(argc, argv);
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

  // Read configuration file, config path has already been checked for existence
  if (handler._config_path.size()) {
    logger.info("Reading configuration from file {}", handler._config_path);
    try {
      handler.parse_config_file();
    } catch (const std::exception& e) {
      logger.error(
          "Unable to parse config file {}. {} Use '-h' to print usage "
          "information.",
          handler._config_path, e.what());
      return EXIT_FAILURE;
    }
  }

  // Parse further command line arguments, those will override values from
  // config file
  try {
    handler.parse_cli_second_pass(cli_args);
  } catch (const std::exception& e) {
    logger.error(
        "Failed to parse command line arguments. {} Use '-h' to print usage "
        "information.",
        e.what());
    // handler.print_help(cli_args.program_name);
    return EXIT_FAILURE;
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

  BuildingTile building_tile;
  building_tile.id = 0;
  building_tile.extent = roi;
  building_tile.proj_helper = roofer::misc::createProjHelper();

  try {
    logger.debug("[cropper] Cropping tile");
    crop_tile(building_tile.extent, handler.input_pointclouds_, building_tile, handler.cfg_, project_srs.get());

    for (auto& building : building_tile.buildings) {
      reconstruct_building(building, &handler.cfg_);
    }
    
  } catch (...) {
    logger.debug("[cropper] Failed cropping tile");
    exit(EXIT_FAILURE);
  }

  logger.info("[serializer] Serializing tile {} with {} buildings",
               building_tile.id, building_tile.buildings.size());

  // create status attribute
  building_tile.attributes.maybe_insert_vec<bool>(handler.cfg_.a_success);
  
  // create time attribute
  auto attr_time = building_tile.attributes.maybe_insert_vec<int>(handler.cfg_.a_reconstruction_time);
  if (attr_time.has_value()) {
    for (auto& building : building_tile.buildings) {
      attr_time->get().push_back(building.reconstruction_time);
    }
  }
  // output reconstructed buildings
  auto CityJsonWriter = roofer::io::createCityJsonWriter(*building_tile.proj_helper);
  CityJsonWriter->written_features_count = building_tile.buildings.size();
  CityJsonWriter->identifier_attribute = handler.cfg_.id_attribute;
  // user provided offset
  if (handler.cfg_.cj_translate.has_value()) {
    CityJsonWriter->translate_x_ = (*handler.cfg_.cj_translate)[0];
    CityJsonWriter->translate_y_ = (*handler.cfg_.cj_translate)[1];
    CityJsonWriter->translate_z_ = (*handler.cfg_.cj_translate)[2];
    // auto offset from data
  } else if (building_tile.proj_helper->data_offset.has_value()) {
    CityJsonWriter->translate_x_ = (*building_tile.proj_helper->data_offset)[0];
    CityJsonWriter->translate_y_ = (*building_tile.proj_helper->data_offset)[1];
    CityJsonWriter->translate_z_ = (*building_tile.proj_helper->data_offset)[2];
  } else {
    throw std::runtime_error(fmt::format(
        "Tile {} has no data offset, cannot write to cityjson",
        building_tile.id));
  }
  CityJsonWriter->scale_x_ = handler.cfg_.cj_scale[0];
  CityJsonWriter->scale_y_ = handler.cfg_.cj_scale[1];
  CityJsonWriter->scale_z_ = handler.cfg_.cj_scale[2];

  std::ofstream ofs;
  if (!handler.cfg_.split_cjseq) {
    // get bottom left corner coordinates
    int minx = building_tile.extent.min()[0];
    int miny = building_tile.extent.min()[1];
    auto jsonl_tile_path = fs::path(handler.cfg_.output_path) / fmt::format("{:06d}_{:06d}.city.jsonl", minx, miny);
    fs::create_directories(jsonl_tile_path.parent_path());
    ofs.open(jsonl_tile_path);
    if (!handler.cfg_.omit_metadata)
      CityJsonWriter->write_metadata(
          ofs, project_srs.get(), building_tile.extent,
          {.identifier = std::to_string(building_tile.id)});
  } else {
    if (!handler.cfg_.omit_metadata) {
      std::string metadata_json_file = fmt::format(
          fmt::runtime(handler.cfg_.metadata_json_file_spec),
          fmt::arg("path", handler.cfg_.output_path));
      fs::create_directories(
          fs::path(metadata_json_file).parent_path());
      ofs.open(metadata_json_file);
      CityJsonWriter->write_metadata(
          ofs, project_srs.get(), building_tile.extent,
          {.identifier = std::to_string(building_tile.id)});
      ofs.close();
    }
  }

  for (auto& building : building_tile.buildings) {
    if (handler.cfg_.split_cjseq) {
      fs::create_directories(building.jsonl_path.parent_path());
      ofs.open(building.jsonl_path);
    }
    try {
      auto attrow = roofer::AttributeMapRow(building_tile.attributes,
                                            building.attribute_index);

      if (!handler.cfg_.a_h_ground.empty())
        attrow.insert(handler.cfg_.a_h_ground, building.h_ground);
      if (!handler.cfg_.a_h_pc_98p.empty())
        attrow.insert(handler.cfg_.a_h_pc_98p, building.h_pc_98p);
      if (!handler.cfg_.a_is_glass_roof.empty())
        attrow.insert(handler.cfg_.a_is_glass_roof, building.is_glass_roof);
      if (!handler.cfg_.a_pointcloud_unusable.empty())
        attrow.insert(handler.cfg_.a_pointcloud_unusable, building.pointcloud_insufficient);
      if (!handler.cfg_.a_roof_type.empty())
        attrow.insert(handler.cfg_.a_roof_type, building.roof_type);
      if (!handler.cfg_.a_h_roof_50p.empty())
        attrow.insert_optional(handler.cfg_.a_h_roof_50p, building.roof_elevation_50p);
      if (!handler.cfg_.a_h_roof_70p.empty())
        attrow.insert_optional(handler.cfg_.a_h_roof_70p, building.roof_elevation_70p);
      if (!handler.cfg_.a_h_roof_min.empty())
        attrow.insert_optional(handler.cfg_.a_h_roof_min, building.roof_elevation_min);
      if (!handler.cfg_.a_h_roof_max.empty())
        attrow.insert_optional(handler.cfg_.a_h_roof_max, building.roof_elevation_max);
      if (!handler.cfg_.a_h_roof_ridge.empty())
        attrow.insert_optional(handler.cfg_.a_h_roof_ridge, building.roof_elevation_ridge);
      if (!handler.cfg_.a_roof_n_planes.empty())
        attrow.insert_optional(handler.cfg_.a_roof_n_planes, building.roof_n_planes);
      if (!handler.cfg_.a_roof_n_ridgelines.empty())
        attrow.insert_optional(handler.cfg_.a_roof_n_ridgelines, building.roof_n_ridgelines);
      if (!handler.cfg_.a_extrusion_mode.empty()) {
        std::string extrusion_mode_str;
        switch (building.extrusion_mode) {
          case STANDARD:
            extrusion_mode_str = "standard";
            break;
          case LOD11_FALLBACK:
            extrusion_mode_str = "lod11_fallback";
            break;
          case SKIP:
            extrusion_mode_str = "skip";
            break;
          default:
            extrusion_mode_str = "unknown";
            break;
        }
        attrow.insert(handler.cfg_.a_extrusion_mode,
                      extrusion_mode_str);
      }

      std::unordered_map<int, roofer::Mesh>* ms12 = nullptr;
      std::unordered_map<int, roofer::Mesh>* ms13 = nullptr;
      std::unordered_map<int, roofer::Mesh>* ms22 = nullptr;
      if (handler.cfg_.lod_12) {
        ms12 = &building.multisolids_lod12;

        if (!handler.cfg_.a_rmse_lod12.empty())
          attrow.insert_optional(handler.cfg_.a_rmse_lod12, building.rmse_lod12);
        if (!handler.cfg_.a_volume_lod12.empty())
          attrow.insert_optional(handler.cfg_.a_volume_lod12, building.volume_lod12);
#if RF_USE_VAL3DITY
        if (!handler.cfg_.a_val3dity_lod12.empty())
          attrow.insert_optional(handler.cfg_.a_val3dity_lod12,
                                  building.val3dity_lod12);
#endif
      }
      if (handler.cfg_.lod_13) {
        ms13 = &building.multisolids_lod13;
        if (!handler.cfg_.a_rmse_lod13.empty())
          attrow.insert_optional(handler.cfg_.a_rmse_lod13,
                                  building.rmse_lod13);
        if (!handler.cfg_.a_volume_lod13.empty())
          attrow.insert_optional(handler.cfg_.a_volume_lod13,
                                  building.volume_lod13);
#if RF_USE_VAL3DITY
        if (!handler.cfg_.a_val3dity_lod13.empty())
          attrow.insert_optional(handler.cfg_.a_val3dity_lod13,
                                  building.val3dity_lod13);
#endif
      }
      if (handler.cfg_.lod_22) {
        ms22 = &building.multisolids_lod22;
        if (!handler.cfg_.a_rmse_lod22.empty())
          attrow.insert_optional(handler.cfg_.a_rmse_lod22,
                                  building.rmse_lod22);
        if (!handler.cfg_.a_volume_lod22.empty())
          attrow.insert_optional(handler.cfg_.a_volume_lod22,
                                  building.volume_lod22);
#if RF_USE_VAL3DITY
        if (!handler.cfg_.a_val3dity_lod22.empty())
          attrow.insert_optional(handler.cfg_.a_val3dity_lod22,
                                  building.val3dity_lod22);
#endif
      }
      // lift lod 0 footprint to h_ground
      building.footprint.set_z(building.h_ground);
      CityJsonWriter->write_feature(ofs, building.footprint, ms12, ms13, ms22, attrow);
      if (handler.cfg_.split_cjseq) {
        ofs.close();
      }
    } catch (const std::exception& e) {
      logger.error("[serializer] Failed to serialize {}. {}", building.jsonl_path.string(), e.what());
    }
  }

}
