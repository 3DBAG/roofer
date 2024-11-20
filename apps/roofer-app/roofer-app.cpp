// Copyright (c) 2018-2024 TU Delft 3D geoinformation group, Ravi Peters (3DGI),
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

#if defined(IS_LINUX) || defined(IS_MACOS)
#include <new>
#include <mimalloc-override.h>
#else
#undef RF_ENABLE_HEAP_TRACING
#endif

#include "config.hpp"

/**
 * @brief A single building object
 *
 * Contains the footprint polygon, the point cloud, the reconstructed model and
 * some attributes that are set during the reconstruction.
 */
struct BuildingObject {
  roofer::PointCollection pointcloud;
  roofer::LinearRing footprint;

  std::unordered_map<int, roofer::Mesh> multisolids_lod12;
  std::unordered_map<int, roofer::Mesh> multisolids_lod13;
  std::unordered_map<int, roofer::Mesh> multisolids_lod22;

  size_t attribute_index;
  bool reconstruction_success = false;
  int reconstruction_time = 0;

  // set in crop
  fs::path jsonl_path;
  float h_ground;
  bool force_lod11;  // force_lod11 / fallback_lod11

  // set in reconstruction
  std::string roof_type;
  float roof_elevation_50p;
  float roof_elevation_70p;
  float roof_elevation_min;
  float roof_elevation_max;
  int roof_n_planes;
  float rmse_lod12;
  float rmse_lod13;
  float rmse_lod22;
  std::string val3dity_lod12;
  std::string val3dity_lod13;
  std::string val3dity_lod22;
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

#ifdef RF_ENABLE_HEAP_TRACING
// Overrides for heap allocation counting
// Ref.: https://www.youtube.com/watch?v=sLlGEUO_EGE
namespace {
  struct HeapAllocationCounter {
    std::atomic<size_t> total_allocated = 0;
    std::atomic<size_t> total_freed = 0;
    [[nodiscard]] size_t current_usage() const {
      return total_allocated - total_freed;
    };
  };
  HeapAllocationCounter heap_allocation_counter;
}  // namespace
#endif

/*
 * Code snippet below is taken from
 * https://github.com/microsoft/mimalloc/blob/dev/include/mimalloc-new-delete.h
 * and modified to work with the roofer trace feature for heap memory usage.
 */
#if defined(IS_LINUX) || defined(IS_MACOS)
#if defined(_MSC_VER) && defined(_Ret_notnull_) && \
    defined(_Post_writable_byte_size_)
   // stay consistent with VCRT definitions
#define mi_decl_new(n) \
  mi_decl_nodiscard mi_decl_restrict _Ret_notnull_ _Post_writable_byte_size_(n)
#define mi_decl_new_nothrow(n)                                                 \
  mi_decl_nodiscard mi_decl_restrict _Ret_maybenull_ _Success_(return != NULL) \
      _Post_writable_byte_size_(n)
#else
#define mi_decl_new(n) mi_decl_nodiscard mi_decl_restrict
#define mi_decl_new_nothrow(n) mi_decl_nodiscard mi_decl_restrict
#endif

void operator delete(void* p) noexcept { mi_free(p); };
void operator delete[](void* p) noexcept { mi_free(p); };

void operator delete(void* p, const std::nothrow_t&) noexcept { mi_free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { mi_free(p); }

mi_decl_new(n) void* operator new(std::size_t n) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new(n);
}
mi_decl_new(n) void* operator new[](std::size_t n) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new(n);
}

mi_decl_new_nothrow(n) void* operator new(std::size_t n,
                                          const std::nothrow_t& tag) noexcept {
  (void)(tag);
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_nothrow(n);
}
mi_decl_new_nothrow(n) void* operator new[](
    std::size_t n, const std::nothrow_t& tag) noexcept {
  (void)(tag);
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_nothrow(n);
}

#if (__cplusplus >= 201402L || _MSC_VER >= 1916)
void operator delete(void* p, std::size_t n) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size(p, n);
};
void operator delete[](void* p, std::size_t n) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size(p, n);
};
#endif

#if (__cplusplus > 201402L || defined(__cpp_aligned_new))
void operator delete(void* p, std::align_val_t al) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}
void operator delete[](void* p, std::align_val_t al) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}
void operator delete(void* p, std::size_t n, std::align_val_t al) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size_aligned(p, n, static_cast<size_t>(al));
};
void operator delete[](void* p, std::size_t n, std::align_val_t al) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_freed += n;
#endif
  mi_free_size_aligned(p, n, static_cast<size_t>(al));
};
void operator delete(void* p, std::align_val_t al,
                     const std::nothrow_t&) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}
void operator delete[](void* p, std::align_val_t al,
                       const std::nothrow_t&) noexcept {
  mi_free_aligned(p, static_cast<size_t>(al));
}

void* operator new(std::size_t n, std::align_val_t al) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned(n, static_cast<size_t>(al));
}
void* operator new[](std::size_t n, std::align_val_t al) noexcept(false) {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned(n, static_cast<size_t>(al));
}
void* operator new(std::size_t n, std::align_val_t al,
                   const std::nothrow_t&) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned_nothrow(n, static_cast<size_t>(al));
}
void* operator new[](std::size_t n, std::align_val_t al,
                     const std::nothrow_t&) noexcept {
#ifdef RF_ENABLE_HEAP_TRACING
  heap_allocation_counter.total_allocated += n;
#endif
  return mi_new_aligned_nothrow(n, static_cast<size_t>(al));
}
#endif
#endif
/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 * Ref.:
 * https://lemire.me/blog/2022/11/10/measuring-the-memory-usage-of-your-c-program/
 */
#if defined(IS_WINDOWS)
#include <windows.h>
#include <psapi.h>

#elif defined(IS_LINUX) || defined(IS_MACOS)
#include <sys/resource.h>
#include <unistd.h>

#if defined(IS_MACOS)
#include <mach/mach.h>
#elif defined(IS_LINUX)
#include <cstdio>
#endif

#endif

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
inline size_t GetCurrentRSS() {
#if defined(IS_WINDOWS)
  /* Windows -------------------------------------------------- */
  PROCESS_MEMORY_COUNTERS info;
  GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
  return (size_t)info.WorkingSetSize;

#elif defined(IS_MACOS)
  /* OSX ------------------------------------------------------ */
  struct mach_task_basic_info info;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info,
                &infoCount) != KERN_SUCCESS)
    return (size_t)0L; /* Can't access? */
  return (size_t)info.resident_size;

#elif defined(IS_LINUX)
  /* Linux ---------------------------------------------------- */
  long rss = 0L;
  FILE* fp = NULL;
  if ((fp = fopen("/proc/self/statm", "r")) == NULL)
    return (size_t)0L; /* Can't open? */
  if (fscanf(fp, "%*s%ld", &rss) != 1) {
    fclose(fp);
    return (size_t)0L; /* Can't read? */
  }
  fclose(fp);
  return (size_t)rss * (size_t)sysconf(_SC_PAGESIZE);

#else
  /* AIX, BSD, Solaris, and Unknown OS ------------------------ */
  return (size_t)0L; /* Unsupported. */
#endif
}

int main(int argc, const char* argv[]) {
  // auto cmdl = argh::parser({"-c", "--config"});
  // cmdl.add_params({"trace-interval", "-j", "--jobs"});

  // cmdl.parse(argc, argv);
  auto& logger = roofer::logger::Logger::get_logger();

  // read cmdl options
  RooferConfig roofer_cfg;
  std::vector<InputPointcloud> input_pointclouds;
  CLIArgs cli_args(argc, argv);
  RooferConfigHandler roofer_cfg_handler(roofer_cfg, input_pointclouds);
  roofer_cfg.rec.lod = 0;

  // Parse basic command line arguments (not yet the configuration parameters)
  try {
    roofer_cfg_handler.parse_cli_first_pass(cli_args);
  } catch (const std::exception& e) {
    logger.error("Failed to parse command line arguments.");
    logger.error("{} Use '-h' to print usage information.", e.what());
    // roofer_cfg_handler.print_help(cli_args.program_name);
    return EXIT_FAILURE;
  }
  if (roofer_cfg_handler._print_help) {
    roofer_cfg_handler.print_help(cli_args.program_name);
    return EXIT_SUCCESS;
  }
  if (roofer_cfg_handler._print_version) {
    roofer_cfg_handler.print_version();
    return EXIT_SUCCESS;
  }

  // Read configuration file, config path has already been checked for existence
  if (roofer_cfg_handler._config_path.size()) {
    logger.info("Reading configuration from file {}",
                roofer_cfg_handler._config_path);
    try {
      roofer_cfg_handler.parse_config_file();
    } catch (const std::exception& e) {
      logger.error(
          "Unable to parse config file {}. {} Use '-h' to print usage "
          "information.",
          roofer_cfg_handler._config_path, e.what());
      return EXIT_FAILURE;
    }
  }

  // Parse further command line arguments, those will override values from
  // config file
  try {
    roofer_cfg_handler.parse_cli_second_pass(cli_args);
  } catch (const std::exception& e) {
    logger.error(
        "Failed to parse command line arguments. {} Use '-h' to print usage "
        "information.",
        e.what());
    // roofer_cfg_handler.print_help(cli_args.program_name);
    return EXIT_FAILURE;
  }

  // validate configuration parameters
  try {
    roofer_cfg_handler.validate();
  } catch (const std::exception& e) {
    logger.error(
        "Failed to validate parameter values. {} Use '-h' to print usage "
        "information.",
        e.what());
    // roofer_cfg_handler.print_help(cli_args.program_name);
    return EXIT_FAILURE;
  }

  bool do_tracing = false;
  auto trace_interval =
      std::chrono::seconds(roofer_cfg_handler._trace_interval);
  if (roofer_cfg_handler._loglevel == "info") {
    logger.set_level(roofer::logger::LogLevel::info);
  } else if (roofer_cfg_handler._loglevel == "debug") {
    logger.set_level(roofer::logger::LogLevel::debug);
  } else if (roofer_cfg_handler._loglevel == "trace") {
    logger.set_level(roofer::logger::LogLevel::trace);
    trace_interval = std::chrono::seconds(roofer_cfg_handler._trace_interval);
    do_tracing = true;
    logger.debug("trace interval is set to {} seconds", trace_interval.count());
  } else {
    logger.set_level(roofer::logger::LogLevel::warning);
  }

  // precomputation for tiling
  std::deque<BuildingTile> initial_tiles;
  auto project_srs = roofer::io::createSpatialReferenceSystemOGR();

  if (!roofer_cfg.srs_override.empty()) {
    project_srs->import(roofer_cfg.srs_override);
    if (!project_srs->is_valid()) {
      logger.error("Invalid user override SRS: {}", roofer_cfg.srs_override);
      return EXIT_FAILURE;
    } else {
      logger.info("Using user override SRS: {}", roofer_cfg.srs_override);
    }
  }

  for (auto& ipc : input_pointclouds) {
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
    VectorReader->layer_name = roofer_cfg.layer_name;
    VectorReader->layer_id = roofer_cfg.layer_id;
    VectorReader->attribute_filter = roofer_cfg.attribute_filter;
    try {
      VectorReader->open(roofer_cfg.source_footprints);
    } catch (const std::exception& e) {
      logger.error("{}", e.what());
      return EXIT_FAILURE;
    }
    if (!project_srs->is_valid()) {
      VectorReader->get_crs(project_srs.get());
    }
    // logger.info("region_of_interest.has_value()? {}",
    //             roofer_cfg.region_of_interest.has_value());

    roofer::TBox<double> roi;
    if (roofer_cfg.region_of_interest.has_value()) {
      // VectorReader->region_of_interest = *roofer_cfg.region_of_interest;
      roi = *roofer_cfg.region_of_interest;
    } else {
      roi = VectorReader->layer_extent;
    }

    logger.info("Region of interest: {} {}, {} {}", roi.pmin[0], roi.pmin[1],
                roi.pmax[0], roi.pmax[1]);

    // actual tiling
    if (roofer_cfg_handler._no_tiling) {
      auto& building_tile = initial_tiles.emplace_back();
      building_tile.id = 0;
      building_tile.extent = roi;
      building_tile.proj_helper = roofer::misc::createProjHelper();
    } else {
      auto tile_extents =
          create_tiles(roi, roofer_cfg.tilesize[0], roofer_cfg.tilesize[1]);

      for (std::size_t tid = 0; tid < tile_extents.size(); tid++) {
        auto& tile = tile_extents[tid];
        auto& building_tile = initial_tiles.emplace_back();
        building_tile.id = tid;
        building_tile.extent = tile;
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
  if (roofer_cfg_handler._jobs < nthreads_reserved + 1) {
    // Only one thread will be available for the reconstructor pool
    nthreads = nthreads_reserved + 1;
  } else {
    nthreads = roofer_cfg_handler._jobs;
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
        logger.debug("[cropper] Cropping tile {}", building_tile.id);
        logger.debug("[cropper] Tile extent: {} {}, {} {}",
                     building_tile.extent.pmin[0], building_tile.extent.pmin[1],
                     building_tile.extent.pmax[0],
                     building_tile.extent.pmax[1]);
        // crop_tile returns true if at least one building was cropped
        if (!crop_tile(building_tile.extent,  // tile extent
                       input_pointclouds,     // input pointclouds
                       building_tile,         // output building data
                       roofer_cfg,            // configuration parameters
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
              "[cropper] Finished cropping tile {}, notifying reconstructor",
              building_tile.id);
          cropped_pending.notify_one();
        }
      } catch (...) {
        logger.error("[cropper] Failed to crop tile {}", building_tile.id);
      }
      initial_tiles.pop_front();
    }
    crop_running.store(false);
    logger.debug("[cropper] Finished cropper");
    cropped_pending.notify_one();
  });

  if (!roofer_cfg_handler._crop_only) {
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
          reconstructed_tiles.push_back(std::move(building_tile));
          cropped_tiles.pop_front();
          logger.debug(
              "[reconstructor] Submitted all buildings of tile {} for "
              "reconstruction",
              building_tile.id);
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
                                          &roofer_cfg, &reconstructed_buildings,
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
              logger.debug("[reconstructor] starting reconstruction for: {}",
                           building_object_ref.building.jsonl_path.string());
              reconstruct_building(building_object_ref.building,
                                   &roofer_cfg.rec);
              logger.debug("[reconstructor] finished reconstruction for: {}",
                           building_object_ref.building.jsonl_path.string());
              // TODO: These two seem to be redundant
              building_object_ref.progress = RECONSTRUCTION_SUCCEEDED;
              building_object_ref.building.reconstruction_success = true;
              building_object_ref.building.reconstruction_time =
                  static_cast<int>(
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::high_resolution_clock::now() - start)
                          .count());
            } catch (...) {
              building_object_ref.progress = RECONSTRUCTION_FAILED;
              auto& logger = roofer::logger::Logger::get_logger();
              logger.warning("[reconstructor] reconstruction failed for: {}",
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
        reconstructed_pending.wait(lock, [&reconstructed_buildings] {
          return !reconstructed_buildings.empty();
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
            auto& tile = reconstructed_tiles[i];
            // We expect that the tile.buildings container hasn't been cleared,
            // so that we can insert the reconstructed building at the index.
            assert(tile.buildings.size() == tile.buildings_cnt);
            if (tile.id == bref.tile_id) {
              tile.buildings[bref.building_idx] = std::move(bref.building);
              tile.buildings_progresses[bref.building_idx] = bref.progress;
            }

            auto tile_finished = std::all_of(
                tile.buildings_progresses.begin(),
                tile.buildings_progresses.end(),
                [](Progress p) { return p > RECONSTRUCTION_IN_PROGRESS; });
            if (tile_finished) {
              finished_idx.first = true;
              finished_idx.second = i;
              {
                std::scoped_lock lock_sorted{sorted_tiles_mutex};
                sorted_tiles.push_back(std::move(tile));
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
      logger.debug("[sorter] Finished sorter");
    });

    serializer_thread = std::thread([&]() {
      logger.info("[serializer] Writing output to {}", roofer_cfg.output_path);
      while (sorting_running.load() || !sorted_tiles.empty()) {
        logger.debug("[serializer] before lock sorted_tiles_mutex");
        std::unique_lock lock{sorted_tiles_mutex};
        sorted_pending.wait(lock,
                            [&sorted_tiles] { return !sorted_tiles.empty(); });
        auto pending_serialized{std::move(sorted_tiles)};
        sorted_tiles.clear();
        sorted_tiles.shrink_to_fit();
        lock.unlock();
        logger.debug("[serializer] after lock.unlock()");

        while (!pending_serialized.empty()) {
          auto& building_tile = pending_serialized.front();

          // create status attribute
          auto& attr_status = building_tile.attributes.insert_vec<std::string>(
              roofer_cfg.n["status"]);
          for (auto& progress : building_tile.buildings_progresses) {
            if (progress == RECONSTRUCTION_FAILED) {
              attr_status.push_back("reconstruction_failed");
            } else if (progress == RECONSTRUCTION_SUCCEEDED) {
              attr_status.push_back("reconstruction_succeeded");
            } else {
              attr_status.push_back("unknown");
            }
          }
          // create time attribute
          auto& attr_time = building_tile.attributes.insert_vec<int>(
              roofer_cfg.n["reconstruction_time"]);
          for (auto& building : building_tile.buildings) {
            attr_time.push_back(building.reconstruction_time);
          }
          // output reconstructed buildings
          auto CityJsonWriter =
              roofer::io::createCityJsonWriter(*building_tile.proj_helper);
          CityJsonWriter->identifier_attribute = roofer_cfg.id_attribute;
          if (building_tile.proj_helper->data_offset.has_value()) {
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
          CityJsonWriter->scale_x_ = 0.01;
          CityJsonWriter->scale_y_ = 0.01;
          CityJsonWriter->scale_z_ = 0.01;

          std::ofstream ofs;
          if (!roofer_cfg.split_cjseq) {
            auto jsonl_tile_path =
                fs::path(roofer_cfg.output_path) / "tiles" /
                fmt::format("tile_{:05d}.city.jsonl", building_tile.id);
            fs::create_directories(jsonl_tile_path.parent_path());
            ofs.open(jsonl_tile_path);
            CityJsonWriter->write_metadata(
                ofs, project_srs.get(), building_tile.extent,
                {.identifier = std::to_string(building_tile.id)});
          } else {
            std::string metadata_json_file =
                fmt::format(fmt::runtime(roofer_cfg.metadata_json_file_spec),
                            fmt::arg("path", roofer_cfg.output_path));
            fs::create_directories(fs::path(metadata_json_file).parent_path());
            ofs.open(metadata_json_file);
            CityJsonWriter->write_metadata(
                ofs, project_srs.get(), building_tile.extent,
                {.identifier = std::to_string(building_tile.id)});
            ofs.close();
          }

          for (auto& building : building_tile.buildings) {
            if (roofer_cfg.split_cjseq) {
              fs::create_directories(building.jsonl_path.parent_path());
              ofs.open(building.jsonl_path);
            }
            try {
              auto attrow = roofer::AttributeMapRow(building_tile.attributes,
                                                    building.attribute_index);

              attrow.insert(roofer_cfg.n["h_ground"], building.h_ground);
              attrow.insert(roofer_cfg.n["roof_type"], building.roof_type);
              attrow.insert(roofer_cfg.n["h_roof_50p"],
                            building.roof_elevation_50p);
              attrow.insert(roofer_cfg.n["h_roof_70p"],
                            building.roof_elevation_70p);
              attrow.insert(roofer_cfg.n["h_roof_min"],
                            building.roof_elevation_min);
              attrow.insert(roofer_cfg.n["h_roof_max"],
                            building.roof_elevation_max);
              attrow.insert(roofer_cfg.n["roof_n_planes"],
                            building.roof_n_planes);

              std::unordered_map<int, roofer::Mesh>* ms12 = nullptr;
              std::unordered_map<int, roofer::Mesh>* ms13 = nullptr;
              std::unordered_map<int, roofer::Mesh>* ms22 = nullptr;
              if (roofer_cfg.rec.lod == 0 || roofer_cfg.rec.lod == 12) {
                ms12 = &building.multisolids_lod12;
                attrow.insert(roofer_cfg.n["rmse_lod12"], building.rmse_lod12);
#if RF_USE_VAL3DITY
                attrow.insert(roofer_cfg.n["val3dity_lod12"],
                              building.val3dity_lod12);
#endif
              }
              if (roofer_cfg.rec.lod == 0 || roofer_cfg.rec.lod == 13) {
                ms13 = &building.multisolids_lod13;
                attrow.insert(roofer_cfg.n["rmse_lod13"], building.rmse_lod13);
#if RF_USE_VAL3DITY
                attrow.insert(roofer_cfg.n["val3dity_lod13"],
                              building.val3dity_lod13);
#endif
              }
              if (roofer_cfg.rec.lod == 0 || roofer_cfg.rec.lod == 22) {
                ms22 = &building.multisolids_lod22;
                attrow.insert(roofer_cfg.n["rmse_lod22"], building.rmse_lod22);
#if RF_USE_VAL3DITY
                attrow.insert(roofer_cfg.n["val3dity_lod22"],
                              building.val3dity_lod22);
#endif
              }
              CityJsonWriter->write_feature(ofs, building.footprint, ms12, ms13,
                                            ms22, attrow);
              if (roofer_cfg.split_cjseq) {
                ofs.close();
              }
              ++serialized_buildings_cnt;
            } catch (const std::exception& e) {
              logger.error("[serializer] Failed to serialize {}. {}",
                           building.jsonl_path.string(), e.what());
            }
          }
          if (!roofer_cfg.split_cjseq) {
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

  if (!cropped_tiles.empty() && !roofer_cfg_handler._crop_only) {
    logger.error(
        "all threads have been joined, but cropped_tiles is "
        "not empty, it still contains {} items",
        cropped_tiles.size());
  }
  if (!reconstructed_buildings.empty() && !roofer_cfg_handler._crop_only) {
    logger.error(
        "all threads have been joined, but reconstructed_buildings is "
        "not empty, it still contains {} items",
        reconstructed_buildings.size());
  }
}
