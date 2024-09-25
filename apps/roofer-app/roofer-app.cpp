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
#include <roofer/ReconstructionConfig.hpp>

// serialisation
#include <roofer/io/CityJsonWriter.hpp>

#include "BS_thread_pool.hpp"
#include "argh.h"
#include "git.h"
#include "toml.hpp"

using fileExtent = std::pair<std::string, roofer::TBox<double>>;

struct InputPointcloud {
  std::string path;
  std::string name;
  int quality;
  int date = 0;
  int bld_class = 6;
  int grnd_class = 2;
  bool force_lod11 = false;
  bool select_only_for_date = false;

  roofer::vec1f nodata_radii;
  roofer::vec1f nodata_fractions;
  roofer::vec1f pt_densities;
  roofer::vec1b is_mutated;
  roofer::vec1b is_glass_roof;
  roofer::vec1b lod11_forced;
  std::vector<roofer::LinearRing> nodata_circles;
  std::vector<roofer::PointCollection> building_clouds;
  std::vector<roofer::ImageMap> building_rasters;
  roofer::vec1f ground_elevations;
  roofer::vec1i acquisition_years;

  std::unique_ptr<roofer::misc::RTreeInterface> rtree;
  std::vector<fileExtent> file_extents;
};

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
  float rmse_lod12;
  float rmse_lod13;
  float rmse_lod22;
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

struct RooferConfig {
  // footprint source parameters
  std::string source_footprints;
  std::string id_attribute;                // -> attr_building_id
  std::string force_lod11_attribute = "";  // -> attr_force_blockmodel
  std::string yoc_attribute;               // -> attr_year_of_construction

  // crop parameters
  float ceil_point_density = 20;
  float cellsize = 0.5;
  int lod11_fallback_area = 69000;
  float lod11_fallback_density = 5;
  float tilesize_x = 1000;
  float tilesize_y = 1000;

  bool write_crop_outputs = false;
  bool output_all = false;
  bool write_rasters = false;
  bool write_metadata = false;
  bool write_index = false;

  // general parameters
  std::optional<roofer::TBox<double>> region_of_interest;
  std::string srs_override;

  // crop output
  bool split_cjseq = false;
  std::string building_toml_file_spec;
  std::string building_las_file_spec;
  std::string building_gpkg_file_spec;
  std::string building_raster_file_spec;
  std::string building_jsonl_file_spec;
  std::string jsonl_list_file_spec;
  std::string index_file_spec;
  std::string metadata_json_file_spec;
  std::string crop_output_path;

  // reconstruct
  roofer::ReconstructionConfig rec;
};

#include "crop_tile.hpp"
#include "reconstruct_building.hpp"

void print_help(std::string program_name) {
  // see http://docopt.org/
  std::cout << "Usage:" << "\n";
  std::cout << "   " << program_name;
  std::cout << " -c <file>" << "\n";
  std::cout << "Options:" << "\n";
  std::cout << "   -h, --help                   Show this help message."
            << "\n";
  std::cout << "   -V, --version                Show version." << "\n";
  std::cout << "   -v, --verbose                Be more verbose." << "\n";
  std::cout << "   -t, --trace                  Trace the progress. Implies "
               "--verbose."
            << "\n";
  std::cout << "   --trace-interval <s>         Trace interval in "
               "seconds [default: 10]."
            << "\n";
  std::cout << "   -c <file>, --config <file>   Config file." << "\n";
  std::cout << "   -r, --rasters                Output rasterised building "
               "pointclouds."
            << "\n";
  std::cout << "   -m, --metadata               Output metadata.json file."
            << "\n";
  std::cout << "   -i, --index                  Output index.gpkg file."
            << "\n";
  std::cout << "   -a, --all                    Output files for each "
               "candidate point cloud instead of only the optimal candidate."
            << "\n";
  std::cout << "   -j <n>, --jobs <n>           Limit the number of threads "
               "used by roofer. Default is to use all available resources."
            << "\n";
}

void print_version() {
  std::cout << fmt::format(
      "roofer {} ({}{}{})\n", git_Describe(),
      std::strcmp(git_Branch(), "main") ? ""
                                        : fmt::format("{}, ", git_Branch()),
      git_AnyUncommittedChanges() ? "dirty, " : "", git_CommitDate());
}

void read_config(const std::string& config_path, RooferConfig& cfg,
                 std::vector<InputPointcloud>& input_pointclouds) {
  auto& logger = roofer::logger::Logger::get_logger();
  toml::table config;
  config = toml::parse_file(config_path);

  auto tml_source_footprints =
      config["input"]["footprint"]["path"].value<std::string>();
  if (tml_source_footprints.has_value())
    cfg.source_footprints = *tml_source_footprints;

  auto id_attribute_ =
      config["input"]["footprint"]["id_attribute"].value<std::string>();
  if (id_attribute_.has_value()) cfg.id_attribute = *id_attribute_;

  auto force_lod11_attribute_ =
      config["input"]["force_lod11_attribute"].value<std::string>();
  if (force_lod11_attribute_.has_value())
    cfg.force_lod11_attribute = *force_lod11_attribute_;

  auto yoc_attribute_ = config["input"]["yoc_attribute"].value<std::string>();
  if (yoc_attribute_.has_value()) cfg.yoc_attribute = *yoc_attribute_;

  auto tml_pointclouds = config["input"]["pointclouds"];
  if (toml::array* arr = tml_pointclouds.as_array()) {
    // visitation with for_each() helps deal with heterogeneous data
    for (auto& el : *arr) {
      toml::table* tb = el.as_table();
      auto& pc = input_pointclouds.emplace_back();

      if (auto n = (*tb)["name"].value<std::string>(); n.has_value()) {
        pc.name = *n;
      }
      if (auto n = (*tb)["quality"].value<int>(); n.has_value()) {
        pc.quality = *n;
      }
      if (auto n = (*tb)["date"].value<int>(); n.has_value()) {
        pc.date = *n;
      }
      if (auto n = (*tb)["force_lod11"].value<bool>(); n.has_value()) {
        pc.force_lod11 = *n;
      }
      if (auto n = (*tb)["select_only_for_date"].value<bool>(); n.has_value()) {
        pc.select_only_for_date = *n;
      }

      if (auto n = (*tb)["building_class"].value<int>(); n.has_value()) {
        pc.bld_class = *n;
      }
      if (auto n = (*tb)["ground_class"].value<int>(); n.has_value()) {
        pc.grnd_class = *n;
      }

      auto tml_path = (*tb)["path"].value<std::string>();
      if (tml_path.has_value()) {
        pc.path = *tml_path;
      }
    };
  }

  // paramaters
  auto ceil_point_density_ =
      config["parameters"]["ceil_point_density"].value<float>();
  if (ceil_point_density_.has_value())
    cfg.ceil_point_density = *ceil_point_density_;

  auto tilesize_x_ = config["parameters"]["tilesize_x"].value<float>();
  if (tilesize_x_.has_value()) cfg.tilesize_x = *tilesize_x_;
  auto tilesize_y_ = config["parameters"]["tilesize_y"].value<float>();
  if (tilesize_y_.has_value()) cfg.tilesize_y = *tilesize_y_;

  auto cellsize_ = config["parameters"]["cellsize"].value<float>();
  if (cellsize_.has_value()) cfg.cellsize = *cellsize_;

  auto lod11_fallback_area_ =
      config["parameters"]["lod11_fallback_area"].value<int>();
  if (lod11_fallback_area_.has_value())
    cfg.lod11_fallback_area = *lod11_fallback_area_;

  if (toml::array* region_of_interest_ =
          config["parameters"]["region_of_interest"].as_array()) {
    if (region_of_interest_->size() == 4 &&
        (region_of_interest_->is_homogeneous(toml::node_type::floating_point) ||
         region_of_interest_->is_homogeneous(toml::node_type::integer))) {
      cfg.region_of_interest =
          roofer::TBox<double>{*region_of_interest_->get(0)->value<double>(),
                               *region_of_interest_->get(1)->value<double>(),
                               0,
                               *region_of_interest_->get(2)->value<double>(),
                               *region_of_interest_->get(3)->value<double>(),
                               0};
    } else {
      logger.error("Failed to read parameter.region_of_interest");
    }
  }

  // reconstruction parameters
  auto complexity_factor_ =
      config["reconstruction"]["complexity_factor"].value<int>();
  if (complexity_factor_.has_value())
    cfg.rec.complexity_factor = *complexity_factor_;
  auto clip_ground_ = config["reconstruction"]["clip_ground"].value<bool>();
  if (clip_ground_.has_value()) cfg.rec.clip_ground = *clip_ground_;
  auto lod_ = config["reconstruction"]["lod"].value<bool>();
  if (lod_.has_value()) cfg.rec.lod = *lod_;
  auto lod13_step_height_ =
      config["reconstruction"]["lod13_step_height"].value<float>();
  if (lod13_step_height_.has_value())
    cfg.rec.lod13_step_height = *lod13_step_height_;

  auto plane_detect_k_ =
      config["reconstruction"]["plane_detect_k"].value<int>();
  if (plane_detect_k_.has_value()) cfg.rec.plane_detect_k = *plane_detect_k_;
  auto plane_detect_min_points_ =
      config["reconstruction"]["plane_detect_min_points"].value<int>();
  if (plane_detect_min_points_.has_value())
    cfg.rec.plane_detect_min_points = *plane_detect_min_points_;
  auto plane_detect_epsilon_ =
      config["reconstruction"]["plane_detect_epsilon"].value<float>();
  if (plane_detect_epsilon_.has_value())
    cfg.rec.plane_detect_epsilon = *plane_detect_epsilon_;
  auto plane_detect_normal_angle_ =
      config["reconstruction"]["plane_detect_normal_angle"].value<float>();
  if (plane_detect_normal_angle_.has_value())
    cfg.rec.plane_detect_normal_angle = *plane_detect_normal_angle_;
  auto line_detect_epsilon_ =
      config["reconstruction"]["line_detect_epsilon"].value<float>();
  if (line_detect_epsilon_.has_value())
    cfg.rec.line_detect_epsilon = *line_detect_epsilon_;
  auto thres_alpha_ = config["reconstruction"]["thres_alpha"].value<float>();
  if (thres_alpha_.has_value()) cfg.rec.thres_alpha = *thres_alpha_;
  auto thres_reg_line_dist_ =
      config["reconstruction"]["thres_reg_line_dist"].value<float>();
  if (thres_reg_line_dist_.has_value())
    cfg.rec.thres_reg_line_dist = *thres_reg_line_dist_;
  auto thres_reg_line_ext_ =
      config["reconstruction"]["thres_reg_line_ext"].value<float>();
  if (thres_reg_line_ext_.has_value())
    cfg.rec.thres_reg_line_ext = *thres_reg_line_ext_;

  // output
  auto split_cjseq_ = config["output"]["split_cjseq"].value<bool>();
  if (split_cjseq_.has_value()) cfg.split_cjseq = *split_cjseq_;
  auto building_toml_file_spec_ =
      config["output"]["building_toml_file"].value<std::string>();
  if (building_toml_file_spec_.has_value())
    cfg.building_toml_file_spec = *building_toml_file_spec_;

  auto building_las_file_spec_ =
      config["output"]["building_las_file"].value<std::string>();
  if (building_las_file_spec_.has_value())
    cfg.building_las_file_spec = *building_las_file_spec_;

  auto building_gpkg_file_spec_ =
      config["output"]["building_gpkg_file"].value<std::string>();
  if (building_gpkg_file_spec_.has_value())
    cfg.building_gpkg_file_spec = *building_gpkg_file_spec_;

  auto building_raster_file_spec_ =
      config["output"]["building_raster_file"].value<std::string>();
  if (building_raster_file_spec_.has_value())
    cfg.building_raster_file_spec = *building_raster_file_spec_;

  if (cfg.write_metadata) {
    auto metadata_json_file_spec_ =
        config["output"]["metadata_json_file"].value<std::string>();
    if (metadata_json_file_spec_.has_value())
      cfg.metadata_json_file_spec = *metadata_json_file_spec_;
  }

  auto building_jsonl_file_spec_ =
      config["output"]["building_jsonl_file"].value<std::string>();
  if (building_jsonl_file_spec_.has_value())
    cfg.building_jsonl_file_spec = *building_jsonl_file_spec_;

  auto index_file_spec_ = config["output"]["index_file"].value<std::string>();
  if (index_file_spec_.has_value()) cfg.index_file_spec = *index_file_spec_;

  auto jsonl_list_file_spec_ =
      config["output"]["jsonl_list_file"].value<std::string>();
  if (jsonl_list_file_spec_.has_value())
    cfg.jsonl_list_file_spec = *jsonl_list_file_spec_;

  auto output_path_ = config["output"]["path"].value<std::string>();
  if (output_path_.has_value()) cfg.crop_output_path = *output_path_;

  auto srs_override_ = config["output"]["crs"].value<std::string>();
  if (srs_override_.has_value()) cfg.srs_override = *srs_override_;
}

void get_las_extents(InputPointcloud& ipc,
                     roofer::io::SpatialReferenceSystemInterface* srs) {
  auto pj = roofer::misc::createProjHelper();
  for (auto& fp :
       roofer::find_filepaths(ipc.path, {".las", ".LAS", ".laz", ".LAZ"})) {
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

void* operator new(size_t size) {
  heap_allocation_counter.total_allocated += size;
  return malloc(size);
}
void operator delete(void* memory, size_t size) noexcept {
  heap_allocation_counter.total_freed += size;
  free(memory);
};

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
  auto cmdl = argh::parser({"-c", "--config"});
  cmdl.add_params({"trace-interval", "-j", "--jobs"});

  cmdl.parse(argc, argv);
  std::string program_name = cmdl[0];
  auto& logger = roofer::logger::Logger::get_logger();

  // read cmdl options
  RooferConfig roofer_cfg;
  roofer_cfg.rec.lod = 0;

  roofer_cfg.output_all = cmdl[{"-a", "--all"}];
  roofer_cfg.write_rasters = cmdl[{"-r", "--rasters"}];
  roofer_cfg.write_metadata = cmdl[{"-m", "--metadata"}];
  roofer_cfg.write_index = cmdl[{"-i", "--index"}];

  if (cmdl[{"-h", "--help"}]) {
    print_help(program_name);
    return EXIT_SUCCESS;
  }
  if (cmdl[{"-V", "--version"}]) {
    print_version();
    return EXIT_SUCCESS;
  }
  if (cmdl[{"-v", "--verbose"}]) {
    logger.set_level(roofer::logger::LogLevel::debug);
  } else {
    logger.set_level(roofer::logger::LogLevel::warning);
  }
  // Enabling tracing overwrites the log level
  auto trace_interval = std::chrono::seconds(10);
  bool do_tracing = cmdl[{"-t", "--trace"}];
  if (do_tracing) {
    logger.set_level(roofer::logger::LogLevel::trace);
    size_t ti;
    if (cmdl("trace-interval") >> ti) {
      trace_interval = std::chrono::seconds(ti);
    }
    logger.debug("trace interval is set to {} seconds", trace_interval.count());
  }

  // Read configuration
  std::string config_path;
  std::vector<InputPointcloud> input_pointclouds;
  if (cmdl({"-c", "--config"}) >> config_path) {
    if (!fs::exists(config_path)) {
      logger.error("No such config file: {}", config_path);
      print_help(program_name);
      return EXIT_FAILURE;
    }
    logger.info("Reading configuration from file {}", config_path);
    try {
      read_config(config_path, roofer_cfg, input_pointclouds);
    } catch (const std::exception& e) {
      logger.error("Unable to parse config file {}.\n{}", config_path,
                   e.what());
      return EXIT_FAILURE;
    }
  } else {
    logger.error("No config file specified");
    return EXIT_FAILURE;
  }

  // precomputation for tiling
  std::deque<BuildingTile> initial_tiles;
  auto project_srs = roofer::io::createSpatialReferenceSystemOGR();

  if (!roofer_cfg.srs_override.empty()) {
    project_srs->import(roofer_cfg.srs_override);
    if (!project_srs->is_valid()) {
      logger.error("Invalid user override SRS: {}", roofer_cfg.srs_override);
      return EXIT_FAILURE;
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
    VectorReader->open(roofer_cfg.source_footprints);
    if (!project_srs->is_valid()) VectorReader->get_crs(project_srs.get());
    logger.info("region_of_interest.has_value()? {}",
                roofer_cfg.region_of_interest.has_value());
    logger.info("Reading footprints from {}", roofer_cfg.source_footprints);

    roofer::TBox<double> roi;
    if (roofer_cfg.region_of_interest.has_value()) {
      // VectorReader->region_of_interest = *roofer_cfg.region_of_interest;
      roi = *roofer_cfg.region_of_interest;
    } else {
      roi = VectorReader->layer_extent;
    }

    // actual tiling
    auto tile_extents =
        create_tiles(roi, roofer_cfg.tilesize_x, roofer_cfg.tilesize_y);

    for (std::size_t tid = 0; tid < tile_extents.size(); tid++) {
      auto& tile = tile_extents[tid];
      auto& building_tile = initial_tiles.emplace_back();
      building_tile.id = tid;
      building_tile.extent = tile;
      building_tile.proj_helper = roofer::misc::createProjHelper();
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
  size_t jobs_from_param = 0;
  if (cmdl({"-j", "--jobs"}) >> jobs_from_param) {
    // Limit the parallelism
    if (jobs_from_param < nthreads_reserved + 1) {
      // Only one thread will be available for the reconstructor pool
      nthreads = nthreads_reserved + 1;
    } else {
      nthreads = jobs_from_param;
    }
  }
  size_t nthreads_reconstructor_pool = nthreads - nthreads_reserved;
  logger.debug(
      "Using {} threads for the reconstructor pool, {} threads in total",
      nthreads_reconstructor_pool, nthreads);
  BS::thread_pool reconstructor_pool(nthreads_reconstructor_pool);

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

  if (do_tracing) {
    tracer_thread.emplace([&] {
      while (crop_running.load() || reconstruction_running.load() ||
             serialization_running.load()) {
        logger.trace("heap", heap_allocation_counter.current_usage());
        logger.trace("rss", GetCurrentRSS());
        logger.trace("crop", cropped_buildings_cnt);
        logger.trace("reconstruct", reconstructed_buildings_cnt);
        logger.trace("sort", sorted_buildings_cnt);
        logger.trace("serialize", serialized_buildings_cnt);
        logger.debug(
            "[reconstructor] reconstructor_pool nr. tasks waiting in the queue "
            "== {}",
            reconstructor_pool.get_tasks_queued());
        std::this_thread::sleep_for(trace_interval);
      }
      // We log once more after all threads have finished, to measure the finaly
      // memory use
      logger.trace("heap", heap_allocation_counter.current_usage());
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
        crop_tile(building_tile.extent,  // tile extent
                  input_pointclouds,     // input pointclouds
                  building_tile,         // output building data
                  roofer_cfg,
                  project_srs.get());  // configuration parameters
        building_tile.buildings_cnt = building_tile.buildings.size();
        building_tile.buildings_progresses.resize(building_tile.buildings_cnt);
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
      } catch (...) {
        logger.error("[cropper] Failed to crop tile {}", building_tile.id);
      }
      initial_tiles.pop_front();
    }
    crop_running.store(false);
    logger.debug("[cropper] Finished cropper");
    cropped_pending.notify_one();
  });

  std::thread reconstructor_thread([&]() {
    while (crop_running.load() || !cropped_tiles.empty()) {
      logger.debug("[reconstructor] before lock cropped_tiles_mutex");
      std::unique_lock lock{cropped_tiles_mutex};
      logger.debug("[reconstructor] before wait(lock)");
      logger.debug(
          "[reconstructor] crop_running.load() == {}, !cropped_tiles.empty() "
          "== {}",
          crop_running.load(), !cropped_tiles.empty());
      cropped_pending.wait(lock,
                           [&cropped_tiles] { return !cropped_tiles.empty(); });
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
        // This wakes up the serializer thread as soon as we submitted one tile
        // for reconstruction, but that doesn't mean that any building of the
        // tile has finished reconstruction. But at least the serializer thread
        // is suspended until this point.
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
          // TODO: It seems that I need to assign the moved 'building_ref' to a
          //  new variable with an explicit type here, because 'bref' contains
          //  const references, but I don't understand why. The problem is that
          //  "Non-const lvalue reference to type BuildingObject cannot bind to
          //  lvalue of type const BuildingObject".
          BuildingObjectRef building_object_ref = bref;
          try {
            auto start = std::chrono::high_resolution_clock::now();
            reconstruct_building(building_object_ref.building, &roofer_cfg.rec);
            // TODO: These two seem to be redundant
            building_object_ref.progress = RECONSTRUCTION_SUCCEEDED;
            building_object_ref.building.reconstruction_success = true;
            building_object_ref.building.reconstruction_time = static_cast<int>(
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
            std::scoped_lock lock_reconstructed{reconstructed_buildings_mutex};
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

  std::thread sorter_thread([&] {
    while (reconstruction_running.load() || !reconstructed_buildings.empty()) {
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

  std::thread serializer_thread([&]() {
    logger.info("[serializer] Writing output to {}",
                roofer_cfg.crop_output_path);
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
        auto& attr_status =
            building_tile.attributes.insert_vec<std::string>("b3_status");
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
        auto& attr_time =
            building_tile.attributes.insert_vec<int>("b3_reconstruction_time");
        for (auto& building : building_tile.buildings) {
          attr_time.push_back(building.reconstruction_time);
        }
        // output reconstructed buildings
        auto CityJsonWriter =
            roofer::io::createCityJsonWriter(*building_tile.proj_helper);
        CityJsonWriter->identifier_attribute = roofer_cfg.id_attribute;
        CityJsonWriter->translate_x_ =
            (*building_tile.proj_helper->data_offset)[0];
        CityJsonWriter->translate_y_ =
            (*building_tile.proj_helper->data_offset)[1];
        CityJsonWriter->translate_z_ =
            (*building_tile.proj_helper->data_offset)[2];
        CityJsonWriter->scale_x_ = 0.01;
        CityJsonWriter->scale_y_ = 0.01;
        CityJsonWriter->scale_z_ = 0.01;

        std::ofstream ofs;
        if (!roofer_cfg.split_cjseq) {
          auto jsonl_tile_path =
              fs::path(roofer_cfg.crop_output_path) / "tiles" /
              fmt::format("tile_{:04d}.city.jsonl", building_tile.id);
          fs::create_directories(jsonl_tile_path.parent_path());
          ofs.open(jsonl_tile_path);
          CityJsonWriter->write_metadata(
              ofs, project_srs.get(), building_tile.extent,
              {.identifier = std::to_string(building_tile.id)});
        }

        for (auto& building : building_tile.buildings) {
          if (roofer_cfg.split_cjseq) {
            fs::create_directories(building.jsonl_path.parent_path());
            ofs.open(building.jsonl_path);
          }
          try {
            std::unordered_map<int, roofer::Mesh>* ms12 = nullptr;
            std::unordered_map<int, roofer::Mesh>* ms13 = nullptr;
            std::unordered_map<int, roofer::Mesh>* ms22 = nullptr;
            if (roofer_cfg.rec.lod == 0 || roofer_cfg.rec.lod == 12) {
              ms12 = &building.multisolids_lod12;
            }
            if (roofer_cfg.rec.lod == 0 || roofer_cfg.rec.lod == 13) {
              ms13 = &building.multisolids_lod13;
            }
            if (roofer_cfg.rec.lod == 0 || roofer_cfg.rec.lod == 22) {
              ms22 = &building.multisolids_lod22;
            }
            CityJsonWriter->write_feature(
                ofs, building.footprint, ms12, ms13, ms22,
                roofer::AttributeMapRow(building_tile.attributes,
                                        building.attribute_index));
            if (roofer_cfg.split_cjseq) {
              ofs.close();
            }
            ++serialized_buildings_cnt;
          } catch (...) {
            logger.error("[serializer] Failed to serialize {}",
                         building.jsonl_path.string());
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

  cropper_thread.join();
  reconstructor_thread.join();
  sorter_thread.join();
  serializer_thread.join();
  if (tracer_thread.has_value()) {
    tracer_thread->join();
  }

  if (!cropped_tiles.empty()) {
    logger.error(
        "all threads have been joined, but cropped_tiles is "
        "not empty, it still contains {} items",
        cropped_tiles.size());
  }
  if (!reconstructed_buildings.empty()) {
    logger.error(
        "all threads have been joined, but reconstructed_buildings is "
        "not empty, it still contains {} items",
        reconstructed_buildings.size());
  }
}
