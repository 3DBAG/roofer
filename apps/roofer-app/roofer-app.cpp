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
#include <future>
#include <iostream>
#include <optional>
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
#include <roofer/io/UnderpassReader.hpp>
#include <roofer/io/LineIndex.hpp>
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

#if defined(IS_WINDOWS)
#undef RF_ENABLE_HEAP_TRACING
#endif
#include "allocators.hpp"

#include "config.hpp"

#include "cityjson_output.hpp"

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

  // Config parsing can itself emit warnings before the destination is known.
  logger.set_console_to_stderr(true);

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
  if (handler._print_help_all) {
    handler.print_help_all(cli_args.program_name);
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
  const bool stream_output = handler.cfg_.stream_to_stdout();
  logger.set_console_to_stderr(stream_output);
  logger.set_level(handler._loglevel);
  if (handler.cfg_.output_terrain) {
    logger.warning(
        "Terrain output is experimental. Its geometry, attributes, file "
        "naming, and configuration may change, and the feature may be removed "
        "in a future release.");
  }
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

  roofer::arr3d stream_translate{};

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

    if (stream_output) {
      const auto center = roi.center();
      stream_translate = handler.cfg_.cj_translate.value_or(
          roofer::arr3d{center[0], center[1], 0.});
    }

    // actual tiling
    if (!handler._tiling) {
      auto& building_tile = initial_tiles.emplace_back();
      building_tile.id = 0;
      building_tile.extent = roi;
      building_tile.proj_helper = roofer::misc::createProjHelper();
    } else {
      auto tile_extents = create_tiles(
          roi, handler.cfg_.metres_to_input_units(handler.cfg_.tilesize[0]),
          handler.cfg_.metres_to_input_units(handler.cfg_.tilesize[1]));

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
  const size_t initial_tiles_count = initial_tiles.size();
  logger.debug("Created {} batch tile regions", initial_tiles_count);

  roofer::io::UnderpassIndex underpass_index;
  if (!handler.cfg_.underpass_source.empty()) {
    const auto& cfg = handler.cfg_;
    try {
      underpass_index = roofer::io::readUnderpasses(
          cfg.underpass_source, cfg.underpass_layer,
          cfg.underpass_id_attribute.empty() ? cfg.id_attribute
                                             : cfg.underpass_id_attribute,
          cfg.underpass_height_attribute,
          project_srs->is_valid() ? project_srs->export_wkt() : std::string{},
          cfg.underpass_attributes,
          cfg.metres_to_input_units(cfg.polygon_snap_threshold));
      logger.info("Loaded underpasses for {} building IDs",
                  underpass_index.size());
    } catch (const std::exception& ex) {
      logger.error("Failed to load underpasses: {}", ex.what());
      return EXIT_FAILURE;
    }
  }

  std::optional<roofer::io::LineIndex> line_index;
  if (!handler.cfg_.line_source.empty()) {
    try {
      line_index.emplace(
          handler.cfg_.line_source, handler.cfg_.line_layer,
          project_srs->is_valid() ? project_srs->export_wkt() : std::string{});
      logger.info("Loaded {} roof line segments", line_index->size());
    } catch (const std::exception& ex) {
      logger.error("Failed to load roof lines: {}", ex.what());
      return EXIT_FAILURE;
    }
  }

  // Multithreading setup. The -j/--jobs value is the user-facing worker budget.
  // The cropper, sorter, serializer, logger, and optional tracer use additional
  // mostly-blocking pipeline threads, so keep one job aside for that overhead
  // and give the rest to the CPU-heavy reconstruction pool.
  const size_t requested_jobs = static_cast<size_t>(handler._jobs);
  const size_t nthreads_reconstructor_pool =
      requested_jobs > 1 ? requested_jobs - 1 : 1;
  const auto system_threads = std::thread::hardware_concurrency();
  logger.info(
      "Using {} threads for the reconstructor pool (-j/--jobs {}, system "
      "offers {})",
      nthreads_reconstructor_pool, requested_jobs, system_threads);

  const bool run_reconstruction = !handler._crop_only;
  std::atomic crop_running{true};
  std::deque<BuildingTile> cropped_tiles;
  std::mutex cropped_tiles_mutex;
  std::condition_variable cropped_pending;

  CompletedBuildings completed(nthreads_reconstructor_pool);
  std::atomic<bool> stream_failed = false;

  std::atomic sorting_running{run_reconstruction};
  std::deque<BuildingTile> sorted_tiles;
  std::mutex sorted_tiles_mutex;
  std::condition_variable sorted_pending;

  bool file_output_failed = false;

  // Counters for tracing
  std::atomic<size_t> cropped_tiles_cnt = 0;
  std::atomic<size_t> cropped_buildings_cnt = 0;
  std::atomic<size_t> reconstructed_buildings_cnt = 0;
  std::atomic<size_t> reconstructed_started_cnt = 0;
  std::atomic<size_t> sorted_buildings_cnt = 0;
  std::atomic<size_t> serialized_tiles_cnt = 0;
  std::atomic<size_t> serialized_buildings_cnt = 0;
  std::optional<std::thread> tracer_thread;
  std::promise<void> pipeline_done;

  std::thread reconstructor_thread;
  std::thread serializer_thread;
  std::thread sorter_thread;

  if (do_tracing) {
    tracer_thread.emplace([&] {
      auto done = pipeline_done.get_future();
      do {
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
      } while (done.wait_for(trace_interval) == std::future_status::timeout);
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
    while (!initial_tiles.empty() && !stream_failed) {
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
          for (auto& building : building_tile.buildings) {
            if (line_index)
              building.external_lines = line_index->clip(
                  building.footprint, *building_tile.proj_helper->data_offset);
            auto input = underpass_index.find(building.id);
            if (input == underpass_index.end() || !building.h_ground) continue;
            for (const auto& feature : input->second) {
              building.underpasses.push_back(feature.localize(
                  *building_tile.proj_helper->data_offset, *building.h_ground,
                  handler.cfg_.metres_to_input_units(
                      handler.cfg_.underpass_default_height)));
            }
          }
          building_tile.buildings_cnt = building_tile.buildings.size();
          if (!stream_output) {
            building_tile.buildings_progresses.assign(
                building_tile.buildings_cnt, CROP_SUCCEEDED);
          }
          const auto tile_id = building_tile.id;
          const auto buildings_cnt = building_tile.buildings_cnt;
          {
            std::scoped_lock lock{cropped_tiles_mutex};
            cropped_buildings_cnt += building_tile.buildings_cnt;
            cropped_tiles.push_back(std::move(building_tile));
          }
          ++cropped_tiles_cnt;
          logger.info("[cropper] Tile {}: cropped {} buildings", tile_id,
                      buildings_cnt);
          logger.debug(
              "[cropper] Finished cropping tile {}, notifying "
              "reconstructor",
              tile_id);
          cropped_pending.notify_one();
        }
      } catch (const std::exception& e) {
        logger.error("[cropper] Failed to crop tile {}. {}", building_tile.id,
                     e.what());
      } catch (...) {
        logger.error("[cropper] Failed to crop tile {}. Unknown exception.",
                     building_tile.id);
      }
      initial_tiles.pop_front();
    }
    {
      std::scoped_lock lock{cropped_tiles_mutex};
      crop_running.store(false);
    }
    logger.debug("[cropper] Finished cropper");
    cropped_pending.notify_one();
  });

  if (run_reconstruction) {
    BS::thread_pool reconstructor_pool(nthreads_reconstructor_pool);
    reconstructor_thread = std::thread([&]() {
      logger.info(
          "[reconstructor] Starting reconstruction with {} worker "
          "threads",
          nthreads_reconstructor_pool);
      while (true) {
        std::unique_lock lock{cropped_tiles_mutex};
        cropped_pending.wait(lock, [&] {
          return stream_failed || !cropped_tiles.empty() || !crop_running;
        });
        if (stream_failed || cropped_tiles.empty()) {
          logger.debug("[reconstructor] No cropped tiles remaining, stopping");
          break;
        }
        auto tile =
            std::make_shared<BuildingTile>(std::move(cropped_tiles.front()));
        cropped_tiles.pop_front();
        lock.unlock();

        auto buildings = std::move(tile->buildings);
        tile->remaining = buildings.size();
        if (!stream_output) {
          tile->buildings.resize(buildings.size());
          std::ranges::fill(tile->buildings_progresses,
                            RECONSTRUCTION_IN_PROGRESS);
        }
        logger.info(
            "[reconstructor] Tile {}: submitted {} buildings for "
            "reconstruction",
            tile->id, buildings.size());

        for (size_t index = 0; index < buildings.size() && !stream_failed;
             ++index) {
          BuildingTask task{tile, index, reconstructed_started_cnt.fetch_add(1),
                            std::move(buildings[index])};
          reconstructor_pool.detach_task([&, task = std::move(task)]() mutable {
            if (stream_failed) return;
            try {
              auto& logger = roofer::logger::Logger::get_logger();
              auto start = std::chrono::high_resolution_clock::now();
              logger.debug("[reconstructor] start: {}",
                           task.building.output_label());
              reconstruct_building(task.building, &handler.cfg_);
              logger.debug("[reconstructor] finish: {}",
                           task.building.output_label());
              task.building.reconstruction_success = true;
              task.building.reconstruction_time = static_cast<int>(
                  std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::high_resolution_clock::now() - start)
                      .count());
            } catch (const std::exception& e) {
              task.building.multisolids_lod12.clear();
              task.building.multisolids_lod13.clear();
              task.building.multisolids_lod22.clear();
              task.building.extrusion_mode = FAIL;
              auto& logger = roofer::logger::Logger::get_logger();
              logger.warning(
                  "[reconstructor] reconstruction failed for: {}. Exception: "
                  "{}",
                  task.building.output_label(), e.what());
            } catch (...) {
              task.building.multisolids_lod12.clear();
              task.building.multisolids_lod13.clear();
              task.building.multisolids_lod22.clear();
              task.building.extrusion_mode = FAIL;
              auto& logger = roofer::logger::Logger::get_logger();
              logger.warning(
                  "[reconstructor] reconstruction failed for: {}. Unknown "
                  "exception.",
                  task.building.output_label());
            }
            const auto processed_count = ++reconstructed_buildings_cnt;
            completed.push(std::move(task));
            if (processed_count % 500 == 0) {
              auto& logger = roofer::logger::Logger::get_logger();
              logger.info("[reconstructor] Processed {} buildings",
                          processed_count);
            }
          });
        }
      }

      logger.debug(
          "[reconstructor] Waiting for all reconstructor threads to join...");
      reconstructor_pool.wait();

      logger.debug(
          "[reconstructor] All reconstructor threads have joined, sent {} "
          "buildings for reconstruction",
          reconstructed_started_cnt.load());
      logger.info(
          "[reconstructor] Finished reconstruction: processed {} of {} "
          "submitted buildings",
          reconstructed_buildings_cnt.load(), reconstructed_started_cnt.load());
      completed.close();
    });

    if (stream_output) {
      serializer_thread = std::thread([&] {
        try {
          stream_completed_buildings(
              completed, std::cout, handler.cfg_, stream_translate,
              *project_srs, serialized_buildings_cnt, serialized_tiles_cnt);
        } catch (const std::exception& e) {
          logger.error("Failed to stream CityJSON: {}", e.what());
          {
            std::lock_guard lock{cropped_tiles_mutex};
            stream_failed = true;
          }
          cropped_pending.notify_one();
          completed.close();
        }
      });
    } else {
      sorter_thread = std::thread([&] {
        while (auto result = completed.next()) {
          auto& tile = *result->tile;
          const auto index = result->building_idx;
          tile.buildings_progresses[index] =
              result->building.reconstruction_success ? RECONSTRUCTION_SUCCEEDED
                                                      : RECONSTRUCTION_FAILED;
          tile.buildings[index] = std::move(result->building);
          ++sorted_buildings_cnt;
          if (--tile.remaining == 0) {
            logger.info("[sorter] Tile {}: sorted {} buildings", tile.id,
                        tile.buildings_cnt);
            {
              std::scoped_lock lock{sorted_tiles_mutex};
              sorted_tiles.push_back(std::move(tile));
            }
            sorted_pending.notify_one();
          }
        }
        {
          std::scoped_lock lock{sorted_tiles_mutex};
          sorting_running.store(false);
        }
        sorted_pending.notify_one();
        logger.debug("[sorter] Finished sorter");
      });

      serializer_thread = std::thread([&]() {
        logger.info("[serializer] Output directory: {}",
                    handler.cfg_.output_path);
        while (true) {
          std::unique_lock lock{sorted_tiles_mutex};
          sorted_pending.wait(lock, [&sorted_tiles, &sorting_running] {
            return !sorted_tiles.empty() || !sorting_running.load();
          });
          if (!sorting_running.load() && sorted_tiles.empty()) {
            logger.debug("[serializer] No sorted tiles remaining, stopping");
            break;
          }
          auto pending_serialized{std::move(sorted_tiles)};
          sorted_tiles.clear();
          lock.unlock();

          while (!pending_serialized.empty()) {
            auto& building_tile = pending_serialized.front();
            logger.info("[serializer] Tile {}: writing {} buildings",
                        building_tile.id, building_tile.buildings_cnt);
            logger.debug("[serializer] Serializing tile {}", building_tile);

            try {
              write_tile_files(handler.cfg_, building_tile, *project_srs,
                               serialized_buildings_cnt);
              ++serialized_tiles_cnt;
              logger.info("[serializer] Tile {}: wrote {} buildings",
                          building_tile.id, building_tile.buildings_cnt);
            } catch (const std::exception& e) {
              file_output_failed = true;
              logger.error("Failed to write tile {}: {}", building_tile.id,
                           e.what());
            }
            pending_serialized.pop_front();
          }
        }
        logger.info(
            "[serializer] Finished serialization: wrote {} buildings in "
            "{} tiles",
            serialized_buildings_cnt.load(), serialized_tiles_cnt.load());
        logger.debug("[serializer] Finished serializer");
      });
    }
    reconstructor_thread.join();
    if (sorter_thread.joinable()) sorter_thread.join();
    serializer_thread.join();
  }

  cropper_thread.join();

  if (tracer_thread.has_value()) {
    pipeline_done.set_value();
    tracer_thread->join();
  }

  if (!stream_failed && run_reconstruction && !cropped_tiles.empty()) {
    logger.error(
        "all threads have been joined, but cropped_tiles is "
        "not empty, it still contains {} items",
        cropped_tiles.size());
  }
  return stream_failed || file_output_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
