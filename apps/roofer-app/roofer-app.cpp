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

enum ExtrusionMode { STANDARD, LOD11_FALLBACK, SKIP, FAIL };

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
  std::vector<Progress> buildings_progresses;
  std::size_t buildings_cnt = 0;
  // offset
  std::unique_ptr<roofer::misc::projHelperInterface> proj_helper;
  // extent
  roofer::TBox<double> extent;

  std::vector<std::pair<Progress, size_t>> count_progresses() const;
};

/**
 * @brief Count of the current `buildings_progresses` items by type.
 * @return A count of each progress type as a vector of pairs.
 */
std::vector<std::pair<Progress, size_t>> BuildingTile::count_progresses()
    const {
  auto counts = std::vector<std::pair<Progress, size_t>>();
  for (std::uint8_t p = CROP_NOT_STARTED; p != SERIALIZATION_SUCCEEDED + 1;
       p++) {
    counts.emplace_back(static_cast<Progress>(p), 0);
  }
  for (auto& bp : buildings_progresses) {
    counts.at(bp).second++;
  }
  return counts;
}

template <>
struct fmt::formatter<BuildingTile> {
  static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context>
  constexpr auto format(BuildingTile const& tile, Context& ctx) const {
    auto nonzero = [](std::pair<Progress, size_t> c) { return c.second > 0; };
    auto pc_ = tile.count_progresses();
    std::vector<std::pair<Progress, size_t>> progress_counts{};
    std::copy_if(pc_.begin(), pc_.end(), std::back_inserter(progress_counts),
                 nonzero);
    // This doesn't work on GCC11 on gilfoyle, because it doesn't recognize the
    // pipe operator '|', but it would be more elegant than the current
    // implementation. auto progress_counts =
    //     tile.count_progresses() |
    //     std::views::filter(nonzero);
    bool data_offset_has_value = false;
    if (tile.proj_helper) {
      data_offset_has_value = tile.proj_helper->data_offset.has_value();
    }
    return fmt::format_to(
        ctx.out(),
        "BuildingTile(id={}, buildings.size={}, attributes.has_attributes={}, "
        "buildings_progresses={}, buildings_cnt={}, "
        "proj_helper.data_offset.has_value={}, extent='{}')",
        tile.id, tile.buildings.size(), tile.attributes.has_attributes(),
        progress_counts, tile.buildings_cnt, data_offset_has_value,
        tile.extent.wkt());
  }
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

// rerun
#ifdef RF_USE_RERUN
// if (handler.cfg_.use_rerun) {
//   // Create a new `RecordingStream` which sends data over TCP to the viewer
//   // process.
//   const auto rec = rerun::RecordingStream("Roofer");
//   // Try to spawn a new viewer instance.
//   rec.spawn().exit_on_failure();
//   rec.set_global();
//   roofer::vec3f testpts;
//   testpts.push_back({1.0f, 2.0f, 3.0f});
//   rec.log("test", rerun::Points3D(testpts));
// }
#endif

  // precomputation for tiling
  std::deque<BuildingTile> initial_tiles;
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

  // Compute batch tile regions
  {
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
    // logger.info("region_of_interest.has_value()? {}",
    //             handler.cfg_.region_of_interest.has_value());

    roofer::TBox<double> roi;
    if (handler.cfg_.region_of_interest.has_value()) {
      // VectorReader->region_of_interest = *handler.cfg_.region_of_interest;
      roi = *handler.cfg_.region_of_interest;
    } else {
      roi = VectorReader->layer_extent;
    }

    logger.info("Region of interest: {:.3f} {:.3f}, {:.3f} {:.3f}", roi.pmin[0],
                roi.pmin[1], roi.pmax[0], roi.pmax[1]);
    logger.info("Number of source footprints: {}",
                VectorReader->get_feature_count());

    // actual tiling
    if (!handler._tiling) {
      auto& building_tile = initial_tiles.emplace_back();
      building_tile.id = 0;
      building_tile.extent = roi;
      building_tile.proj_helper = roofer::misc::createProjHelper();
    } else {
      auto tile_extents =
          create_tiles(roi, handler.cfg_.tilesize[0], handler.cfg_.tilesize[1]);

      for (std::size_t tid = 0; tid < tile_extents.size(); tid++) {
        // intersect with roi, to avoid creating buildings outside of the roi
        auto& building_tile = initial_tiles.emplace_back();
        building_tile.id = tid;
        auto tile = roi.intersect(tile_extents[tid]);
        if (tile.has_value()) {
          building_tile.extent = *tile;
        } else {
          logger.warning(
              "Tile is outside of the region of interest: \n{}, ROI: \n{}",
              tile_extents[tid].wkt(), roi.wkt());
          return EXIT_FAILURE;
        }
        building_tile.proj_helper = roofer::misc::createProjHelper();
      }
    }
  }
  logger.debug("Created {} batch tile regions", initial_tiles.size());

  // Multithreading setup
  // -5, because we need one thread for crop, reconstruct, sort, serialize,
  // plus logger. We don't count with the main thread and tracer thread, because
  // all the work is offloaded to the worker threads and the main is not doing
  // much work, tracer either.
  size_t nthreads_reserved = 5;
  size_t nthreads = nthreads_reserved + 1;
  if (nthreads < std::thread::hardware_concurrency()) {
    nthreads = std::thread::hardware_concurrency();
  }

  // Limit the parallelism
  if (handler._jobs < nthreads_reserved + 1) {
    // Only one thread will be available for the reconstructor pool
    nthreads = nthreads_reserved + 1;
  } else {
    nthreads = handler._jobs;
  }

  size_t nthreads_reconstructor_pool = nthreads - nthreads_reserved;
  logger.info(
      "Using {} threads for the reconstructor pool, {} threads in total "
      "(system offers {})",
      nthreads_reconstructor_pool, nthreads,
      std::thread::hardware_concurrency());

  std::atomic crop_running{true};
  std::deque<BuildingTile> cropped_tiles;
  std::mutex cropped_tiles_mutex;
  std::condition_variable cropped_pending;

  std::atomic reconstruction_running{true};
  std::deque<BuildingObjectRef> cropped_buildings;
  std::deque<BuildingObjectRef> reconstructed_buildings;
  std::mutex reconstructed_buildings_mutex;
  std::deque<BuildingTile> reconstructed_tiles;
  std::condition_variable reconstructed_pending;

  std::atomic sorting_running{true};
  std::deque<BuildingTile> sorted_tiles;
  std::mutex sorted_tiles_mutex;
  std::condition_variable sorted_pending;

  std::atomic serialization_running{true};

  // Counters for tracing
  std::atomic<size_t> cropped_buildings_cnt = 0;
  std::atomic<size_t> reconstructed_buildings_cnt = 0;
  std::atomic<size_t> reconstructed_started_cnt = 0;
  std::atomic<size_t> sorted_buildings_cnt = 0;
  std::atomic<size_t> serialized_buildings_cnt = 0;
  std::optional<std::thread> tracer_thread;

  std::thread reconstructor_thread;
  std::thread serializer_thread;
  std::thread sorter_thread;

  if (do_tracing) {
    tracer_thread.emplace([&] {
      while (crop_running.load() || reconstruction_running.load() ||
             serialization_running.load()) {
#ifdef RF_ENABLE_HEAP_TRACING
        logger.trace("heap", heap_allocation_counter.current_usage());
#endif
        logger.trace("rss", GetCurrentRSS());
        logger.trace("crop", cropped_buildings_cnt);
        logger.trace("reconstruct", reconstructed_buildings_cnt);
        logger.trace("sort", sorted_buildings_cnt);
        logger.trace("serialize", serialized_buildings_cnt);
        // logger.debug(
        //     "[reconstructor] reconstructor_pool nr. tasks waiting in the
        //     queue "
        //     "== {}",
        //     reconstructor_pool.get_tasks_queued());
        std::this_thread::sleep_for(trace_interval);
      }
// We log once more after all threads have finished, to measure the finaly
// memory use
#ifdef RF_ENABLE_HEAP_TRACING
      logger.trace("heap", heap_allocation_counter.current_usage());
#endif
      logger.trace("rss", GetCurrentRSS());
      logger.trace("crop", cropped_buildings_cnt);
      logger.trace("reconstruct", reconstructed_buildings_cnt);
      logger.trace("sort", sorted_buildings_cnt);
      logger.trace("serialize", serialized_buildings_cnt);
    });
  }

  // Process tiles
  std::thread cropper_thread([&]() {
    logger.debug("[cropper] Starting cropper");
    while (!initial_tiles.empty()) {
      auto& building_tile = initial_tiles.front();
      try {
        // crop each tile
        logger.debug("[cropper] Cropping tile {}", building_tile);
        // crop_tile returns true if at least one building was cropped
        if (!crop_tile(building_tile.extent,        // tile extent
                       handler.input_pointclouds_,  // input pointclouds
                       building_tile,               // output building data
                       handler.cfg_,                // configuration parameters
                       project_srs.get())) {
          logger.info("No footprints found in tile {}, skipping...",
                      building_tile.id);
        } else {
          building_tile.buildings_cnt = building_tile.buildings.size();
          building_tile.buildings_progresses.resize(
              building_tile.buildings_cnt);
          std::ranges::fill(building_tile.buildings_progresses, CROP_SUCCEEDED);
          {
            std::scoped_lock lock{cropped_tiles_mutex};
            cropped_buildings_cnt += building_tile.buildings_cnt;
            cropped_tiles.push_back(std::move(building_tile));
          }
          logger.debug(
              "[cropper] Finished cropping tile, notifying reconstructor {}",
              building_tile);
          cropped_pending.notify_one();
        }
      } catch (...) {
        logger.error("[cropper] Failed to crop tile {}", building_tile);
      }
      initial_tiles.pop_front();
    }
    crop_running.store(false);
    logger.debug("[cropper] Finished cropper");
    cropped_pending.notify_one();
  });

  if (!handler._crop_only) {
    BS::thread_pool reconstructor_pool(nthreads_reconstructor_pool);
    reconstructor_thread = std::thread([&]() {
      while (crop_running.load() || !cropped_tiles.empty()) {
        logger.debug("[reconstructor] before lock cropped_tiles_mutex");
        std::unique_lock lock{cropped_tiles_mutex};
        logger.debug("[reconstructor] before wait(lock)");
        logger.debug(
            "[reconstructor] crop_running.load() == {}, !cropped_tiles.empty() "
            "== {}",
            crop_running.load(), !cropped_tiles.empty());
        cropped_pending.wait(lock, [&cropped_tiles, &crop_running] {
          return !cropped_tiles.empty() || !crop_running.load();
        });
        // Move the cropped buildings out of the tiles into their own queue, so
        // that the parallel workers do not stop at the tile boundary, until all
        // buildings are finished in the current tile.
        while (!cropped_tiles.empty()) {
          auto& building_tile = cropped_tiles.front();
          for (size_t building_idx = 0;
               building_idx < building_tile.buildings.size(); building_idx++) {
            cropped_buildings.emplace_back(
                building_tile.id, building_idx,
                std::move(building_tile.buildings[building_idx]),
                RECONSTRUCTION_IN_PROGRESS);
          }
          std::ranges::fill(building_tile.buildings_progresses,
                            RECONSTRUCTION_IN_PROGRESS);
          logger.debug(
              "[reconstructor] Submitted all buildings for reconstruction for "
              "tile {}",
              building_tile);
          // Here the building_tile.buildings is empty, because we moved off all
          // BuildingObject-s, so we might as well clear it.
          building_tile.buildings.clear();
          reconstructed_tiles.push_back(std::move(building_tile));
          cropped_tiles.pop_front();
          // This wakes up the serializer thread as soon as we submitted one
          // tile for reconstruction, but that doesn't mean that any building of
          // the tile has finished reconstruction. But at least the serializer
          // thread is suspended until this point.
          reconstructed_pending.notify_one();
        }
        cropped_tiles.clear();
        cropped_tiles.shrink_to_fit();
        lock.unlock();
        logger.debug("[reconstructor] after lock.unlock()");

        // Start one reconstruction task per building, running parallel
        while (!cropped_buildings.empty()) {
          auto building_ref = std::move(cropped_buildings.front());
          cropped_buildings.pop_front();
          ++reconstructed_started_cnt;

          reconstructor_pool.detach_task([bref = std::move(building_ref),
                                          cfg = &handler.cfg_,
                                          &reconstructed_buildings,
                                          &reconstructed_buildings_cnt,
                                          &reconstructed_buildings_mutex] {
            // TODO: It seems that I need to assign the moved 'building_ref' to
            // a
            //  new variable with an explicit type here, because 'bref' contains
            //  const references, but I don't understand why. The problem is
            //  that "Non-const lvalue reference to type BuildingObject cannot
            //  bind to lvalue of type const BuildingObject".
            BuildingObjectRef building_object_ref = bref;
            try {
              auto& logger = roofer::logger::Logger::get_logger();
              auto start = std::chrono::high_resolution_clock::now();
              logger.debug("[reconstructor] start: {}",
                           building_object_ref.building.jsonl_path.string());
              reconstruct_building(building_object_ref.building, cfg);
              logger.debug("[reconstructor] finish: {}",
                           building_object_ref.building.jsonl_path.string());
              // TODO: These two seem to be redundant
              building_object_ref.progress = RECONSTRUCTION_SUCCEEDED;
              building_object_ref.building.reconstruction_success = true;
              building_object_ref.building.reconstruction_time =
                  static_cast<int>(
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::high_resolution_clock::now() - start)
                          .count());
            } catch (const std::exception& e) {
              building_object_ref.building.multisolids_lod12.clear();
              building_object_ref.building.multisolids_lod13.clear();
              building_object_ref.building.multisolids_lod22.clear();
              building_object_ref.progress = RECONSTRUCTION_FAILED;
              building_object_ref.building.extrusion_mode = FAIL;
              auto& logger = roofer::logger::Logger::get_logger();
              logger.warning(
                  "[reconstructor] reconstruction failed for: {}. Exception: "
                  "{}",
                  building_object_ref.building.jsonl_path.string(), e.what());
            } catch (...) {
              building_object_ref.building.multisolids_lod12.clear();
              building_object_ref.building.multisolids_lod13.clear();
              building_object_ref.building.multisolids_lod22.clear();
              building_object_ref.progress = RECONSTRUCTION_FAILED;
              building_object_ref.building.extrusion_mode = FAIL;
              auto& logger = roofer::logger::Logger::get_logger();
              logger.warning(
                  "[reconstructor] reconstruction failed for: {}. Unknown "
                  "exception.",
                  building_object_ref.building.jsonl_path.string());
            }
            {
              std::scoped_lock lock_reconstructed{
                  reconstructed_buildings_mutex};
              ++reconstructed_buildings_cnt;
              reconstructed_buildings.push_back(std::move(building_object_ref));
            }
          });
        }
      }

      logger.debug(
          "[reconstructor] Waiting for all reconstructor threads to join...");
      reconstructor_pool.wait();

      if (!cropped_tiles.empty()) {
        logger.error(
            "[reconstructor] reconstructor is finished, but cropped_tiles is "
            "not empty, it still contains {} items",
            cropped_tiles.size());
      }
      logger.debug(
          "[reconstructor] All reconstructor threads have joined, sent {} "
          "buildings for reconstruction",
          reconstructed_started_cnt.load());
      reconstruction_running.store(false);
      reconstructed_pending.notify_one();
    });

    sorter_thread = std::thread([&] {
      while (reconstruction_running.load() ||
             !reconstructed_buildings.empty()) {
        logger.debug("[sorter] before lock reconstructed_buildings_mutex");
        std::unique_lock lock{reconstructed_buildings_mutex};
        reconstructed_pending.wait(
            lock, [&reconstructed_buildings, &reconstruction_running] {
              return !reconstructed_buildings.empty() ||
                     !reconstruction_running.load();
            });
        auto pending_sorted{std::move(reconstructed_buildings)};
        reconstructed_buildings.clear();
        reconstructed_buildings.shrink_to_fit();
        lock.unlock();
        logger.debug("[sorter] after lock.unlock()");

        while (!pending_sorted.empty()) {
          auto& bref = pending_sorted.front();
          std::pair<bool, std::size_t> finished_idx{false, 0};
          // TODO: Maybe we could use a different data structure, one that has
          //  O(1) lookup for the tile ID, instead of looping the
          //  reconstructed_tiles. However, I expect that the size of
          //  reconstructed_tiles remains relatively low. But we need to do the
          //  loop on each building...
          for (size_t i = 0; i < reconstructed_tiles.size(); i++) {
            auto& building_tile = reconstructed_tiles[i];
            // The reconstructor moved off all items from the
            // building_tile.buildings container, which can mess up its size. We
            // need to make sure that the container is the right size, so that
            // we can insert the reconstructed building at the index.
            if (building_tile.buildings.size() != building_tile.buildings_cnt) {
              building_tile.buildings.resize(building_tile.buildings_cnt);
            }
            if (building_tile.id == bref.tile_id) {
              building_tile.buildings.at(bref.building_idx) =
                  std::move(bref.building);
              building_tile.buildings_progresses.at(bref.building_idx) =
                  bref.progress;
            }

            auto tile_finished = std::all_of(
                building_tile.buildings_progresses.begin(),
                building_tile.buildings_progresses.end(),
                [](Progress p) { return p > RECONSTRUCTION_IN_PROGRESS; });
            if (tile_finished) {
              finished_idx.first = true;
              finished_idx.second = i;
              logger.debug("[sorter] tile_finished=true: {}", building_tile);
              {
                std::scoped_lock lock_sorted{sorted_tiles_mutex};
                sorted_tiles.push_back(std::move(building_tile));
              }
              sorted_pending.notify_one();
            }
          }

          if (finished_idx.first) {
            reconstructed_tiles.erase(reconstructed_tiles.begin() +
                                      finished_idx.second);
          }
          ++sorted_buildings_cnt;
          pending_sorted.pop_front();
        }
      }
      sorting_running.store(false);
      sorted_pending.notify_one();
      logger.debug("[sorter] Finished sorter");
    });

    serializer_thread = std::thread([&]() {
      logger.info("[serializer] Writing output to {}",
                  handler.cfg_.output_path);
      while (sorting_running.load() || !sorted_tiles.empty()) {
        logger.debug("[serializer] before lock sorted_tiles_mutex");
        std::unique_lock lock{sorted_tiles_mutex};
        sorted_pending.wait(lock, [&sorted_tiles, &sorting_running] {
          return !sorted_tiles.empty() || !sorting_running.load();
        });
        auto pending_serialized{std::move(sorted_tiles)};
        sorted_tiles.clear();
        sorted_tiles.shrink_to_fit();
        lock.unlock();
        logger.debug("[serializer] after lock.unlock()");

        while (!pending_serialized.empty()) {
          auto& building_tile = pending_serialized.front();
          logger.debug("[serializer] Serializing tile {}", building_tile);

          // create status attribute
          auto attr_success = building_tile.attributes.maybe_insert_vec<bool>(
              handler.cfg_.a_success);
          if (attr_success.has_value()) {
            for (auto& progress : building_tile.buildings_progresses) {
              attr_success->get().push_back(progress ==
                                            RECONSTRUCTION_SUCCEEDED);
            }
          }
          // create time attribute
          auto attr_time = building_tile.attributes.maybe_insert_vec<int>(
              handler.cfg_.a_reconstruction_time);
          if (attr_time.has_value()) {
            for (auto& building : building_tile.buildings) {
              attr_time->get().push_back(building.reconstruction_time);
            }
          }
          // output reconstructed buildings
          auto CityJsonWriter =
              roofer::io::createCityJsonWriter(*building_tile.proj_helper);
          CityJsonWriter->written_features_count = serialized_buildings_cnt;
          CityJsonWriter->identifier_attribute = handler.cfg_.id_attribute;
          // user provided offset
          if (handler.cfg_.cj_translate.has_value()) {
            CityJsonWriter->translate_x_ = (*handler.cfg_.cj_translate)[0];
            CityJsonWriter->translate_y_ = (*handler.cfg_.cj_translate)[1];
            CityJsonWriter->translate_z_ = (*handler.cfg_.cj_translate)[2];
            // auto offset from data
          } else if (building_tile.proj_helper->data_offset.has_value()) {
            CityJsonWriter->translate_x_ =
                (*building_tile.proj_helper->data_offset)[0];
            CityJsonWriter->translate_y_ =
                (*building_tile.proj_helper->data_offset)[1];
            CityJsonWriter->translate_z_ =
                (*building_tile.proj_helper->data_offset)[2];
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
            auto jsonl_tile_path =
                fs::path(handler.cfg_.output_path) /
                fmt::format("{:06d}_{:06d}.city.jsonl", minx, miny);
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
                attrow.insert(handler.cfg_.a_is_glass_roof,
                              building.is_glass_roof);
              if (!handler.cfg_.a_pointcloud_unusable.empty())
                attrow.insert(handler.cfg_.a_pointcloud_unusable,
                              building.pointcloud_insufficient);
              if (!handler.cfg_.a_roof_type.empty())
                attrow.insert(handler.cfg_.a_roof_type, building.roof_type);
              if (!handler.cfg_.a_h_roof_50p.empty())
                attrow.insert_optional(handler.cfg_.a_h_roof_50p,
                                       building.roof_elevation_50p);
              if (!handler.cfg_.a_h_roof_70p.empty())
                attrow.insert_optional(handler.cfg_.a_h_roof_70p,
                                       building.roof_elevation_70p);
              if (!handler.cfg_.a_h_roof_min.empty())
                attrow.insert_optional(handler.cfg_.a_h_roof_min,
                                       building.roof_elevation_min);
              if (!handler.cfg_.a_h_roof_max.empty())
                attrow.insert_optional(handler.cfg_.a_h_roof_max,
                                       building.roof_elevation_max);
              if (!handler.cfg_.a_h_roof_ridge.empty())
                attrow.insert_optional(handler.cfg_.a_h_roof_ridge,
                                       building.roof_elevation_ridge);
              if (!handler.cfg_.a_roof_n_planes.empty())
                attrow.insert_optional(handler.cfg_.a_roof_n_planes,
                                       building.roof_n_planes);
              if (!handler.cfg_.a_roof_n_ridgelines.empty())
                attrow.insert_optional(handler.cfg_.a_roof_n_ridgelines,
                                       building.roof_n_ridgelines);
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
                  case FAIL:
                    extrusion_mode_str = "fail";
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
                  attrow.insert_optional(handler.cfg_.a_rmse_lod12,
                                         building.rmse_lod12);
                if (!handler.cfg_.a_volume_lod12.empty())
                  attrow.insert_optional(handler.cfg_.a_volume_lod12,
                                         building.volume_lod12);
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
              CityJsonWriter->write_feature(ofs, building.footprint, ms12, ms13,
                                            ms22, attrow);
              if (handler.cfg_.split_cjseq) {
                ofs.close();
              }
              ++serialized_buildings_cnt;
            } catch (const std::exception& e) {
              logger.error("[serializer] Failed to serialize {}. {}",
                           building.jsonl_path.string(), e.what());
            }
          }
          if (!handler.cfg_.split_cjseq) {
            ofs.close();
          }
          pending_serialized.pop_front();
        }
      }
      serialization_running.store(false);
      logger.debug("[serializer] Finished serializer");
    });
    reconstructor_thread.join();
    sorter_thread.join();
    serializer_thread.join();
  }

  cropper_thread.join();

  if (tracer_thread.has_value()) {
    tracer_thread->join();
  }

  if (!cropped_tiles.empty() && !handler._crop_only) {
    logger.error(
        "all threads have been joined, but cropped_tiles is "
        "not empty, it still contains {} items",
        cropped_tiles.size());
  }
  if (!reconstructed_buildings.empty() && !handler._crop_only) {
    logger.error(
        "all threads have been joined, but reconstructed_buildings is "
        "not empty, it still contains {} items",
        reconstructed_buildings.size());
  }

  // logger.info("Finished roofer");
}
