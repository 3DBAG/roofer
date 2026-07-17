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
// Balázs Dukai
#pragma once

#include <functional>
#include <concepts>
#include <thread>
#include <algorithm>
#include <array>
#include "toml.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"

#include <roofer/common/common.hpp>
#include <roofer/common/formatters.hpp>
#include <roofer/logger/logger.h>
#include <roofer/misc/Vector2DOps.hpp>
#include <roofer/reconstruction/ReconstructionConfig.hpp>
#include <stdexcept>
#include <string>
#include <list>
#include <filesystem>
#include <utility>
#include "version.hpp"

#include "validators.hpp"
#include "parameter.hpp"

namespace fs = std::filesystem;
namespace check = roofer::validators;

using fileExtent = std::pair<std::string, roofer::TBox<double>>;

struct InputPointcloud {
  std::vector<std::string> paths;
  std::string name;
  int quality = 0;
  int date = 0;
  int bld_class = 6;
  int grnd_class = 2;
  bool force_lod11 = false;
  bool select_only_for_date = false;
  std::optional<float> min_ground_elevation;

  roofer::vec1f nodata_radii;
  roofer::vec1f nodata_fractions;
  roofer::vec1f pt_densities;
  roofer::vec1b is_glass_roof;
  roofer::vec1b lod11_forced;
  roofer::vec1b pointcloud_insufficient;
  std::vector<roofer::LinearRing> nodata_circles;
  std::vector<roofer::PointCollection> building_clouds;
  std::vector<roofer::ImageMap> building_rasters;
  roofer::veco1f ground_elevations;
  roofer::veco1f terrain_grid_elevations;
  roofer::vec1f roof_elevations;
  roofer::vec1i acquisition_years;

  std::unique_ptr<roofer::misc::RTreeInterface> rtree;
  std::vector<fileExtent> file_extents;
};

inline std::optional<size_t> select_terrain_pointcloud(
    const std::vector<InputPointcloud>& input_pointclouds) {
  std::optional<size_t> selected;
  for (size_t i = 0; i < input_pointclouds.size(); ++i) {
    const auto& candidate = input_pointclouds[i];
    if (candidate.select_only_for_date) continue;
    if (!selected.has_value()) {
      selected = i;
      continue;
    }

    const auto& current = input_pointclouds[*selected];
    if (candidate.quality < current.quality ||
        (candidate.quality == current.quality &&
         candidate.date > current.date)) {
      selected = i;
    }
  }
  return selected;
}

struct RooferConfigHandler;

struct RooferConfig {
  // footprint source parameters
  std::string source_footprints;
  std::string id_attribute;           // -> attr_building_id
  std::string force_lod11_attribute;  // -> attr_force_blockmodel
  std::string yoc_attribute;          // -> attr_year_of_construction
  std::string h_terrain_attribute;    // -> attr_h_terrain
  std::string h_roof_attribute;       // -> attr_h_roof
  std::string layer_name;
  int layer_id = 0;
  std::string attribute_filter;

  int bld_class = 6;
  int grnd_class = 2;

  // crop parameters
  float ceil_point_density = 20;
  float cellsize = 0.5;
  // Minimum building point density (points/m²); roofprints below
  // this are flagged pointcloud-insufficient. Deterministic per-building.
  float min_building_density = 1.0;
  float max_nodata_fraction = 1.0;
  float terrain_grid_cellsize = 10.0;
  int terrain_grid_search_radius = 3;
  std::string terrain_nodata_mode = "fill_small_gaps";
  int lod11_fallback_area = 69000;
  float lod11_fallback_density = 5;
  roofer::arr2f tilesize = {1000, 1000};
  bool clear_if_insufficient = true;
  bool compute_pc_98p = false;
  bool simplify = true;

  bool write_crop_outputs = false;
  bool output_all = false;
  bool write_rasters = false;
  bool write_index = false;

  // general parameters
  std::optional<roofer::TBox<double>> region_of_interest;
  std::string srs_override;
#ifdef RF_USE_RERUN
  bool use_rerun = false;
#endif

  // crop output
  bool split_cjseq = false;
  bool omit_metadata = false;
  bool output_terrain = false;
  roofer::arr3d cj_scale = {0.001, 0.001, 0.001};
  std::optional<roofer::arr3d> cj_translate;
  std::string building_toml_file_spec =
      "{path}/objects/{bid}/config_{pc_name}.toml";
  std::string building_las_file_spec =
      "{path}/objects/{bid}/crop/{bid}_{pc_name}.las";
  std::string building_gpkg_file_spec = "{path}/objects/{bid}/crop/{bid}.gpkg";
  std::string building_raster_file_spec =
      "{path}/objects/{bid}/crop/{bid}_{pc_name}.tif";
  std::string building_jsonl_file_spec =
      "{path}/objects/{bid}/reconstruct/{bid}.city.jsonl";
  std::string jsonl_list_file_spec = "{path}/features.txt";
  std::string index_file_spec = "{path}/index.gpkg";
  std::string metadata_json_file_spec = "{path}/metadata.json";
  std::string output_path;

  // reconstruct: defaults and low-level options are owned by the shared
  // descriptor-backed aggregate.
  roofer::reconstruction::ReconstructionConfig reconstruction;

  // output attribute names
  std::string a_success = "rf_success";
  std::string a_reconstruction_time = "rf_t_run";
  std::string a_val3dity_lod12 = "rf_val3dity_lod12";
  std::string a_val3dity_lod13 = "rf_val3dity_lod13";
  std::string a_val3dity_lod22 = "rf_val3dity_lod22";
  std::string a_is_glass_roof = "rf_is_glass_roof";
  std::string a_nodata_frac = "rf_nodata_frac";
  std::string a_nodata_r = "rf_nodata_r";
  std::string a_pt_density = "rf_pt_density";
  std::string a_is_mutated = "rf_is_mutated";
  std::string a_pc_select = "rf_pc_select";
  std::string a_pc_source = "rf_pc_source";
  std::string a_pc_year = "rf_pc_year";
  std::string a_force_lod11 = "rf_force_lod11";
  std::string a_roof_type = "rf_roof_type";
  std::string a_h_roof_50p = "rf_h_roof_50p";
  std::string a_h_roof_70p = "rf_h_roof_70p";
  std::string a_h_roof_min = "rf_h_roof_min";
  std::string a_h_roof_max = "rf_h_roof_max";
  std::string a_h_roof_ridge = "rf_h_roof_ridge";
  std::string a_h_pc_98p = "rf_h_pc_98p";
  std::string a_roof_n_planes = "rf_roof_planes";
  std::string a_roof_n_ridgelines = "rf_ridgelines";
  std::string a_rmse_lod12 = "rf_rmse_lod12";
  std::string a_rmse_lod13 = "rf_rmse_lod13";
  std::string a_rmse_lod22 = "rf_rmse_lod22";
  std::string a_volume_lod12 = "rf_volume_lod12";
  std::string a_volume_lod13 = "rf_volume_lod13";
  std::string a_volume_lod22 = "rf_volume_lod22";
  std::string a_h_ground = "rf_h_ground";
  std::string a_slope = "rf_slope";
  std::string a_azimuth = "rf_azimuth";
  std::string a_extrusion_mode = "rf_extrusion_mode";
  std::string a_pointcloud_unusable = "rf_pointcloud_unusable";
};

std::vector<std::string> find_filepaths(
    const std::list<std::string>& filepath_parts,
    std::initializer_list<std::string> extensions,
    bool no_throw_on_missing = false) {
  std::vector<std::string> files;
  for (const auto& filepath_part : filepath_parts) {
    if (fs::is_directory(filepath_part)) {
      for (auto& p : fs::directory_iterator(filepath_part)) {
        auto ext = p.path().extension();
        for (auto& filter_ext : extensions) {
          if (filter_ext == ext) {
            files.push_back(p.path().string());
          }
        }
      }
    } else {
      if (fs::exists(filepath_part)) {
        files.push_back(filepath_part);
      } else if (!no_throw_on_missing) {
        throw std::runtime_error("File not found: " + filepath_part + ".");
      }
    }
  }
  return files;
}

namespace roofer::validators {
  // Path exists validator
  auto PathExists = [](const std::string& path) -> std::optional<std::string> {
    if (!std::filesystem::exists(path)) {
      return std::format("Path {} does not exist.", path);
    }
    return std::nullopt;
  };

  // Create a validator for file path writeability
  auto DirIsWritable =
      [](const std::string& path) -> std::optional<std::string> {
    std::filesystem::path fs_path(path);

    // convert to absolute path
    auto abs_path = std::filesystem::absolute(fs_path);

    // find the first parent folders that already exists
    auto parent = abs_path;
    while (!std::filesystem::exists(parent)) {
      parent = parent.parent_path();
    }

    // check if parent is a directory
    if (!std::filesystem::is_directory(parent)) {
      return std::format("Path {} is not a directory.", parent.string());
    }

    // Try to create a temporary file in parent
    auto testPath = parent / "write_test_tmp";
    try {
      std::ofstream test_file(testPath);
      if (test_file) {
        test_file.close();
        std::filesystem::remove(testPath);
        return std::nullopt;
      }
    } catch (...) {
      if (std::filesystem::exists(testPath)) {
        std::filesystem::remove(testPath);
      }
    }
    return std::format("Could not write to directory {}.", parent.string());
    ;
  };
}  // namespace roofer::validators

struct CLIArgs {
  std::string program_name;
  std::list<std::string> args;

  CLIArgs(int argc, const char* argv[]) {
    program_name = argv[0];
    // get the name of the bindary
    auto pos = program_name.find_last_of("/\\");
    if (pos != std::string::npos) {
      program_name = program_name.substr(pos + 1);
    }
    for (int i = 1; i < argc; i++) {
      args.push_back(argv[i]);
    }
  }
};

struct LegacyReconstructionDestination {
  std::string_view legacy_key;
  std::string_view section;
  std::string_view nested_key;
  bool cli_deprecated = true;
  bool ignored = false;
  bool cli_supported = true;
};

#define ROOFER_LEGACY_RECONSTRUCTION_ALIASES(X)                                \
  X("lod12", "reconstruction", "lod12", false, false, true, FIELD, "",         \
    cfg_.reconstruction, Reconstruction, lod12)                                \
  X("lod13", "reconstruction", "lod13", false, false, true, FIELD, "",         \
    cfg_.reconstruction, Reconstruction, lod13)                                \
  X("lod22", "reconstruction", "lod22", false, false, true, FIELD, "",         \
    cfg_.reconstruction, Reconstruction, lod22)                                \
  X("complexity-factor", "reconstruction.arrangement-optimiser",               \
    "complexity-factor", false, false, true, FIELD, "",                        \
    cfg_.reconstruction.arrangement_optimiser, Optimiser, complexity_factor)   \
  X("clip-terrain", "reconstruction", "clip-terrain", false, false, true,      \
    FIELD, "", cfg_.reconstruction, Reconstruction, clip_terrain)              \
  X("lod13-step-height", "reconstruction", "lod13-step-height", true, false,   \
    true, FIELD, "", cfg_.reconstruction, Reconstruction, lod13_step_height)   \
  X("plane-detect-k", "reconstruction.plane-detector",                         \
    "plane-neighbour-count", true, false, true, FIELD, "",                     \
    cfg_.reconstruction.plane_detector, PlaneDetector, plane_neighbour_count)  \
  X("plane-detect-min-points", "reconstruction.plane-detector",                \
    "min-plane-points", true, false, true, FIELD, "",                          \
    cfg_.reconstruction.plane_detector, PlaneDetector, min_plane_points)       \
  X("plane-detect-epsilon", "reconstruction.plane-detector", "plane-epsilon",  \
    true, false, true, FIELD, "", cfg_.reconstruction.plane_detector,          \
    PlaneDetector, plane_epsilon)                                              \
  X("plane-detect-normal-angle", "reconstruction.plane-detector",              \
    "normal-angle-threshold", true, false, false, NORMAL_DOT_PRODUCT, "",      \
    _legacy_plane_detect_normal_angle, void, unused)                           \
  X("line-detect-epsilon", "reconstruction.line-detector",                     \
    "distance-threshold", true, false, false, FIELD, "",                       \
    cfg_.reconstruction.line_detector, LineDetector, distance_threshold)       \
  X("thres-alpha", "reconstruction.alpha-shaper", "alpha", true, false, false, \
    FIELD, "", cfg_.reconstruction.alpha_shaper, AlphaShaper, alpha)           \
  X("thres-reg-line-dist", "reconstruction.line-regulariser",                  \
    "distance-threshold", true, false, false, FIELD, "",                       \
    cfg_.reconstruction.line_regulariser, LineRegulariser, distance_threshold) \
  X("thres-reg-line-ext", "reconstruction.line-regulariser", "extension",      \
    true, false, false, FIELD, "", cfg_.reconstruction.line_regulariser,       \
    LineRegulariser, extension)                                                \
  X("h-terrain-strategy", "reconstruction", "h-terrain-strategy", true, false, \
    true, FIELD, "\"buffer_tile\"", cfg_.reconstruction, Reconstruction,       \
    h_terrain_strategy)                                                        \
  X("lod11-fallback-planes", "reconstruction.plane-detector",                  \
    "max-plane-count", true, false, true, FIELD, "",                           \
    cfg_.reconstruction.plane_detector, PlaneDetector, max_plane_count)        \
  X("lod11-fallback-time", "reconstruction.plane-detector", "max-plane-count", \
    true, true, true, IGNORED, "", _deprecated_lod11_fallback_time, void,      \
    unused)

inline std::optional<LegacyReconstructionDestination>
legacy_reconstruction_destination(std::string_view key) {
#define ROOFER_LEGACY_DESTINATION(key, section, nested_key, deprecated,  \
                                  ignored, cli_supported, kind, example, \
                                  owner, type, member)                   \
  LegacyReconstructionDestination{key,        section, nested_key,       \
                                  deprecated, ignored, cli_supported},
  static constexpr std::array destinations{
      ROOFER_LEGACY_RECONSTRUCTION_ALIASES(ROOFER_LEGACY_DESTINATION)};
#undef ROOFER_LEGACY_DESTINATION

  const auto destination =
      std::find_if(destinations.begin(), destinations.end(),
                   [key](const auto& item) { return item.legacy_key == key; });
  if (destination == destinations.end()) return std::nullopt;
  return *destination;
}

struct RooferConfigHandler {
  RooferConfig cfg_;

  using param_group_map = std::vector<std::pair<std::string, ParameterVector>>;

  std::vector<InputPointcloud> input_pointclouds_;
  param_group_map app_param_groups_;
  param_group_map param_groups_;
  DocAttribMap output_attr_;
  std::unordered_map<std::string, ConfigParameter*> param_index_;
  std::unordered_map<std::string, ConfigParameter*> app_param_index_;
  ParameterVector root_only_reconstruction_aliases_;
  std::unordered_map<std::string, ConfigParameter*>
      root_only_reconstruction_alias_index_;

  static int default_jobs() {
    auto system_threads = std::thread::hardware_concurrency();
    return system_threads == 0 ? 1 : static_cast<int>(system_threads);
  }

  // flags
  bool _print_help = false;
  bool _print_help_all = false;
  bool _print_attributes = false;
  bool _print_version = false;
  bool _crop_only = false;
  bool _tiling = false;
  bool _skip_pc_check = false;
  roofer::logger::LogLevel _loglevel = roofer::logger::LogLevel::info;
  int _trace_interval = 10;
  std::string _config_path;
  int _jobs = default_jobs();
  int _deprecated_lod11_fallback_time = 1800000;
  float _legacy_plane_detect_normal_angle = 0.75F;

  // methods
  RooferConfigHandler() {
    ParameterVector input, crop, reconstruction, output;
    ParameterVector general;

    general.add("help", 'h', "Show help message", _print_help);
    general.add("help-all", 'H', "Show full help message", _print_help_all);
    general.add("attributes", 'a', "List output attributes", _print_attributes);
    general.add("version", 'v', "Show version", _print_version);
    general.add("jobs", 'j',
                "Number of worker jobs to use. Reconstruction uses roughly "
                "jobs - 1 threads.",
                _jobs, {roofer::config::greater_than(0)});
    general.add("config", 'c', "Configuration file", _config_path,
                {check::PathExists, check::DirIsWritable});
    general.add("trace-interval", "Interval for tracing in seconds",
                _trace_interval, {roofer::config::greater_than(0)});
    general.add("loglevel", "Specify loglevel", _loglevel);
#ifdef RF_USE_RERUN
    general.add("rerun", "Log intermediate results to rerun", cfg_.use_rerun);
#endif

    input.add(
        "id-attribute",
        "Building ID attribute to be used as identifier in CityJSONSeq output.",
        cfg_.id_attribute);
    input.add(
        "force-lod11-attribute",
        "Input attribute (boolean) to force individual buildings to always be "
        "reconstructed using simple extrusion (LoD 1.1).",
        cfg_.force_lod11_attribute);
    input.add("yoc-attribute",
              "Input attribute (integer) containing the building's year of "
              "construction."
              " Only relevant when multiple pointclouds are provided.",
              cfg_.yoc_attribute);
    input.add(
        "h-terrain-attribute",
        "Input attribute (float) with fallback terrain elevation for each "
        "building. Used in case no terrain elevation can be derived from "
        "the pointcloud. See also --h-terrain-strategy",
        cfg_.h_terrain_attribute);
    input.add(
        "h-roof-attribute",
        "Input attribute (float) containing fallback roof height for buildings "
        "in case no roof height can be derived from the pointcloud.",
        cfg_.h_roof_attribute);
    input.add("polygon-source-layer",
              "Select this layer name from `<polygon-source>`. By default the "
              "first layer is used.",
              cfg_.layer_name);
    input.add("filter",
              "Specify WHERE clause in OGR SQL to select specfic features from "
              "`<polygon-source>`. See "
              "https://gdal.org/en/stable/user/ogr_sql_dialect.html#where",
              cfg_.attribute_filter);
    input
        .add(
            "box",
            "Axis aligned bounding box specifying the region of interest. Data "
            "outside of this region will be ignored.",
            cfg_.region_of_interest,
            {[](const std::optional<roofer::TBox<double>>& box)
                 -> std::optional<std::string> {
              if (box.has_value()) {
                auto error_msg = check::ValidBox(*box);
                if (error_msg) {
                  return error_msg;
                }
              }
              return std::nullopt;
            }})
        .example_ = "[100, 100, 200, 200]";
    input
        .add(
            "srs",
            "Manually set or override Spatial Reference System for input data.",
            cfg_.srs_override)
        .example_ = "\"EPSG:7415\"";
    // important pointcloud parameters, that are not related to pointcloud
    // selection
    input.add("bld-class",
              "LAS classification code that contains the building points.",
              cfg_.bld_class, {roofer::config::at_least(0)});
    input.add("grnd-class",
              "LAS classification code that constains the ground points.",
              cfg_.grnd_class, {roofer::config::at_least(0)});
    input.add("skip-pc-check",
              "Disable/enable check if all supplied pointcloud files exist.",
              _skip_pc_check);

    crop.add(
        "simplify",
        "Simplify input rootprints to remove (nearly) duplicated vertices.",
        cfg_.simplify);
    crop.add("ceil-point-density",
             "Enforce this point density ceiling on each building pointcloud.",
             cfg_.ceil_point_density, {roofer::config::greater_than(0.0F)});
    crop.add("cellsize",
             "Cellsize used for quick pointcloud analysis (eg. point density"
             " and nodata regions).",
             cfg_.cellsize, {roofer::config::greater_than(0.0F)});
    crop.add("min-building-density",
             "Minimum building-class point density (points/m²) below "
             "which a rootprint's pointcloud is flagged insufficient.",
             cfg_.min_building_density, {roofer::config::at_least(0.0F)});
    crop.add("max-nodata-fraction",
             "Maximum fraction of the roofprint area without pointcloud data. "
             "Above this threshold, a rootprint's pointcloud is flagged "
             "insufficient.",
             cfg_.max_nodata_fraction, {roofer::config::in_range(0.0F, 1.0F)});
    crop.add("terrain-grid-cellsize",
             "Cellsize used for the crop phase terrain fallback grid.",
             cfg_.terrain_grid_cellsize, {roofer::config::greater_than(0.0F)});
    crop.add("terrain-grid-search-radius",
             "Number of terrain grid cells to search around a building when "
             "its local fallback cells do not contain terrain points.",
             cfg_.terrain_grid_search_radius, {roofer::config::at_least(0)});
    crop.add("terrain-nodata-mode",
             "How missing terrain grid samples are handled during output "
             "triangulation: `complete_quads`, `local_triangles`, or "
             "`fill_small_gaps`.",
             cfg_.terrain_nodata_mode,
             {check::OneOf<std::string>(
                 {"complete_quads", "local_triangles", "fill_small_gaps"})});
    crop.add(
        "lod11-fallback-area",
        "LoD 1.1 fallback threshold area in square meters. If the area of the "
        "roofprint is larger than this value, the building will be always be "
        "reconstructed using a LoD 1.1 extrusion.",
        cfg_.lod11_fallback_area, {roofer::config::greater_than(0)});
    crop.add("clear-insufficient",
             "Do not attempt to reconstruct buildings with insufficient "
             "pointcloud data."
             " If `--h-roof-attribute` is set, an LoD 1.1 extrusion will be "
             "performed, "
             "otherwise no 3D model will be generated.",
             cfg_.clear_if_insufficient);
    crop.add("compute-pc-98p",
             "Compute and output the 98th percentile of pointcloud height for "
             "each building.",
             cfg_.compute_pc_98p);
    crop.add("crop-only",
             "Only perform the crop phase, do not perform reconstruction,",
             _crop_only);
    crop.add("crop-output",
             "Output building pointclouds from crop phase as LAS files.",
             cfg_.write_crop_outputs);
    crop.add("crop-output-all",
             "Output building pointclouds for each pointcloud. "
             "Only relevant when multiple pointclouds are provided."
             "Implies `--crop-output`",
             cfg_.output_all);
    crop.add("crop-rasters",
             "Output rasterised pointcloud analytics from crop phase as "
             "GeoTIFF files. "
             "Implies `--crop-output`",
             cfg_.write_rasters);
    crop.add("index",
             "Output index.gpkg file with quick pointcloud analystics from "
             "crop phase.",
             cfg_.write_index);

    using Reconstruction = roofer::reconstruction::ReconstructionConfig;
    using Optimiser = roofer::reconstruction::ArrangementOptimiserConfig;
    using PlaneDetector = roofer::reconstruction::PlaneDetectorConfig;
    using AlphaShaper = roofer::reconstruction::AlphaShaperConfig;
    using LineDetector = roofer::reconstruction::LineDetectorConfig;
    using LineRegulariser = roofer::reconstruction::LineRegulariserConfig;

#define ROOFER_REGISTER_LEGACY_FIELD(key, cli_supported, example, owner, type, \
                                     member)                                   \
  do {                                                                         \
    auto& parameter =                                                          \
        (cli_supported)                                                        \
            ? reconstruction.add_from_field(key, owner, &type::member)         \
            : root_only_reconstruction_aliases_.add_from_field(key, owner,     \
                                                               &type::member); \
    parameter.example_ = example;                                              \
  } while (false);
#define ROOFER_REGISTER_LEGACY_IGNORED(key, cli_supported, example, owner, \
                                       type, member)                       \
  do {                                                                     \
    static_assert(cli_supported);                                          \
    auto& parameter = reconstruction.add(                                  \
        key,                                                               \
        "Deprecated and ignored. Wall-clock reconstruction limits were "   \
        "removed because they are nondeterministic; use max-plane-count "  \
        "under [reconstruction.plane-detector] instead.",                  \
        owner);                                                            \
    parameter.example_ = example;                                          \
  } while (false);
#define ROOFER_REGISTER_LEGACY_NORMAL_DOT_PRODUCT(key, cli_supported, example, \
                                                  owner, type, member)         \
  do {                                                                         \
    static_assert(!cli_supported);                                             \
    auto& parameter = root_only_reconstruction_aliases_.add(                   \
        key,                                                                   \
        "Minimum normal dot-product similarity within a plane "                \
        "(unitless).",                                                         \
        owner, {roofer::config::in_range(0.0F, 1.0F)});                        \
    parameter.example_ = example;                                              \
  } while (false);
#define ROOFER_REGISTER_LEGACY(key, section, nested_key, deprecated, ignored, \
                               cli_supported, kind, example, owner, type,     \
                               member)                                        \
  ROOFER_REGISTER_LEGACY_##kind(key, cli_supported, example, owner, type,     \
                                member)
    ROOFER_LEGACY_RECONSTRUCTION_ALIASES(ROOFER_REGISTER_LEGACY)
#undef ROOFER_REGISTER_LEGACY
#undef ROOFER_REGISTER_LEGACY_NORMAL_DOT_PRODUCT
#undef ROOFER_REGISTER_LEGACY_IGNORED
#undef ROOFER_REGISTER_LEGACY_FIELD

    output.add("tiling", "Enable or disable output tiling.", _tiling);
    output.add("tilesize", "Tilesize for rectangular output tiles in meters.",
               cfg_.tilesize, {check::AllHigherThan({0, 0})});
    output.add("split-cjseq",
               "Output CityJSONSequence file for each building instead of one "
               "file per tile.",
               cfg_.split_cjseq);
    output.add("omit-metadata",
               "Omit metadata line in CityJSONSequence output.",
               cfg_.omit_metadata);
    output.add("terrain",
               "Write one triangulated TINRelief feature per tile from the "
               "highest-quality pointcloud source.",
               cfg_.output_terrain);
    output.add("cj-scale", "Scaling applied to CityJSON output vertices",
               cfg_.cj_scale);
    output
        .add("cj-translate",
             "Translation applied to CityJSON output vertices. Uses dataset "
             "center by default.",
             cfg_.cj_translate)
        .example_ = "[100000, 200000, 0]";

    output_attr_.emplace("success",
                         DocAttrib(&cfg_.a_success,
                                   "Indicates if processing completed without "
                                   "unexpected errors"));
    output_attr_.emplace("reconstruction_time",
                         DocAttrib(&cfg_.a_reconstruction_time,
                                   "Reconstruction time in milliseconds"));
    output_attr_.emplace(
        "val3dity_lod12",
        DocAttrib(&cfg_.a_val3dity_lod12,
                  "Lists val3dity codes for LoD 1.2 geometry"));
    output_attr_.emplace(
        "val3dity_lod13",
        DocAttrib(&cfg_.a_val3dity_lod13,
                  "Lists val3dity codes for LoD 1.3 geometry"));
    output_attr_.emplace(
        "val3dity_lod22",
        DocAttrib(&cfg_.a_val3dity_lod22,
                  "Lists val3dity codes for LoD 2.2 geometry"));
    output_attr_.emplace("is_glass_roof",
                         DocAttrib(&cfg_.a_is_glass_roof,
                                   "Indicates if a glass roof was detected"));
    output_attr_.emplace(
        "nodata_frac", DocAttrib(&cfg_.a_nodata_frac,
                                 "Indicates fraction (in the range [0,1]) "
                                 "of the roofprint area that is not covered by"
                                 " pointcloud data"));
    output_attr_.emplace(
        "nodata_r",
        DocAttrib(
            &cfg_.a_nodata_r,
            "Indicates the radius of the largest "
            "circle in the roofprint that is not covered by pointcloud data"));
    output_attr_.emplace("pt_density",
                         DocAttrib(&cfg_.a_pt_density,
                                   "Indicates the point density inside the "
                                   "roofprint"));
    output_attr_.emplace(
        "is_mutated",
        DocAttrib(&cfg_.a_is_mutated,
                  "Indicates if the building was mutated "
                  "between multiple input pointclouds (if multiple input "
                  "pointclouds were "
                  "provided)"));
    output_attr_.emplace(
        "pc_select", DocAttrib(&cfg_.a_pc_select,
                               "Indicates why the input pointcloud was "
                               "selected for reconstruction. Only relevant "
                               "if multiple input pointclouds were provided"));
    output_attr_.emplace("pc_source",
                         DocAttrib(&cfg_.a_pc_source,
                                   "Indicates which input pointcloud was used "
                                   "for reconstruction"));
    output_attr_.emplace("pc_year",
                         DocAttrib(&cfg_.a_pc_year,
                                   "Indicates the acquisition year of the "
                                   "selected input pointcloud"));
    output_attr_.emplace(
        "force_lod11",
        DocAttrib(&cfg_.a_force_lod11,
                  "Indicates if LoD 1.1 extrusion was forced for "
                  "the building"));
    output_attr_.emplace(
        "roof_type",
        DocAttrib(&cfg_.a_roof_type,
                  "Roof type. Can be `no points`, `no planes`, `horizontal`, "
                  "`multiple horizontal`, or `slanted`"));
    output_attr_.emplace(
        "h_roof_50p",
        DocAttrib(&cfg_.a_h_roof_50p, "The 50th percentile roof elevation"));
    output_attr_.emplace(
        "h_roof_70p",
        DocAttrib(&cfg_.a_h_roof_70p, "The 70th percentile roof elevation"));
    output_attr_.emplace("h_roof_min", DocAttrib(&cfg_.a_h_roof_min,
                                                 "The minimum roof elevation"));
    output_attr_.emplace("h_roof_max", DocAttrib(&cfg_.a_h_roof_max,
                                                 "The maximum roof elevation"));
    output_attr_.emplace("h_roof_ridge", DocAttrib(&cfg_.a_h_roof_ridge,
                                                   "The main ridge elevation"));
    output_attr_.emplace("h_pc_98p",
                         DocAttrib(&cfg_.a_h_pc_98p,
                                   "The 98th percentile elevation of "
                                   "the building pointcloud"));
    output_attr_.emplace(
        "roof_n_planes",
        DocAttrib(&cfg_.a_roof_n_planes,
                  "The number of roofplanes "
                  "detected in the pointcloud (could be different from the "
                  "generated mesh model)"));
    output_attr_.emplace(
        "roof_n_ridgelines",
        DocAttrib(&cfg_.a_roof_n_ridgelines,
                  "The number of ridgelines "
                  "detected in the pointcloud (could be different from the "
                  "generated mesh model)"));
    output_attr_.emplace("rmse_lod12",
                         DocAttrib(&cfg_.a_rmse_lod12,
                                   "The Root Mean Square Erorr of "
                                   "the LOD12 geometry"));
    output_attr_.emplace("rmse_lod13",
                         DocAttrib(&cfg_.a_rmse_lod13,
                                   "The Root Mean Square Erorr of "
                                   "the LOD13 geometry"));
    output_attr_.emplace("rmse_lod22",
                         DocAttrib(&cfg_.a_rmse_lod22,
                                   "The Root Mean Square Erorr of "
                                   "the LOD22 geometry"));
    output_attr_.emplace("volume_lod12",
                         DocAttrib(&cfg_.a_volume_lod12,
                                   "The volume in cubic meters of the LoD 1.2 "
                                   "geometry"));
    output_attr_.emplace("volume_lod13",
                         DocAttrib(&cfg_.a_volume_lod13,
                                   "The volume in cubic meters of the LoD 1.3 "
                                   "geometry"));
    output_attr_.emplace("volume_lod22",
                         DocAttrib(&cfg_.a_volume_lod22,
                                   "The volume in cubic meters of the LoD 2.2 "
                                   "geometry"));
    output_attr_.emplace("h_ground",
                         DocAttrib(&cfg_.a_h_ground,
                                   "The elevation of the floor of the "
                                   "building"));
    output_attr_.emplace(
        "slope",
        DocAttrib(&cfg_.a_slope, "The slope of a roofpart in degrees"));
    output_attr_.emplace(
        "azimuth",
        DocAttrib(&cfg_.a_azimuth, "The azimuth of a roofpart in degrees"));
    output_attr_.emplace(
        "extrusion_mode",
        DocAttrib(
            &cfg_.a_extrusion_mode,
            "Indicates what extrusion mode was used "
            "for the building. `standard`: the regular LoD 1.2, 1.3 or 2.2 "
            "extrusion. `lod11_fallback`: all geometry was substituted with an "
            "LoD 1.1 extrusion. `skip`: no 3D geometry was generated"));
    output_attr_.emplace("pointcloud_unusable",
                         DocAttrib(&cfg_.a_pointcloud_unusable,
                                   "Indicates if the pointcloud was found "
                                   "to be insufficient for reconstruction"));
    output.add("attribute-rename",
               "Rename output attributes. "
               "If no value is provided, the attribute will not be written."
               " See the list of available attributes with `--attributes`."
               " By default attribute names are prefixed with `rf_`.",
               output_attr_);
    // Move groups into param_group_map
    param_groups_.emplace_back("Input", std::move(input));
    param_groups_.emplace_back("Crop", std::move(crop));
    param_groups_.emplace_back("Reconstruction", std::move(reconstruction));
    param_groups_.emplace_back("Output", std::move(output));
    app_param_groups_.emplace_back("General", std::move(general));

    // Add to index
    for (auto& [group_name, group] : param_groups_) {
      group.add_to_index(param_index_);
    }
    for (auto& [group_name, group] : app_param_groups_) {
      group.add_to_index(app_param_index_);
    }
    root_only_reconstruction_aliases_.add_to_index(
        root_only_reconstruction_alias_index_);
  };

  void validate() {
    if (_jobs < 1) {
      throw std::runtime_error(
          "Validation error for jobs parameter. Value must be higher "
          "than 0.");
    }

    for (auto& [group_name, group] : param_groups_) {
      for (auto& param : group) {
        if (auto error_msg = param->validate()) {
          throw std::runtime_error(
              std::format("Validation error for {} parameter {}. {}",
                          group_name, param->longname_, *error_msg));
        }
      }
    }
    if (auto error = cfg_.reconstruction.validate()) {
      throw std::runtime_error("Validation error for reconstruction." + *error);
    }

    if (input_pointclouds_.empty()) {
      throw std::runtime_error("No input pointclouds specified.");
    }
    if (!_skip_pc_check) {
      for (auto& ipc : input_pointclouds_) {
        if (ipc.paths.empty()) {
          throw std::runtime_error(
              "No files found for one of the input pointclouds.");
        }
      }
    }
    // if (auto error_msg = check::PathExists(cfg_.source_footprints)) {
    //   throw std::runtime_error(std::format(
    //       "Footprint source does not exist: {}.", cfg_.source_footprints));
    // }
    if (auto error_msg = check::DirIsWritable(cfg_.output_path)) {
      throw std::runtime_error(
          std::format("Can't write to output directory: {}", *error_msg));
    }
  }

  template <typename T, typename node>
  void get_toml_value(const node& config, const std::string& key, T& result) {
    try {
      if (auto tml_value = config[key].template value<T>();
          tml_value.has_value()) {
        result = *tml_value;
      }
    } catch (const std::exception& e) {
      throw std::runtime_error(std::format(
          "Failed to read value for {} from config file. {}", key, e.what()));
    }
  }

  template <typename Config>
  bool assign_reconstruction_field(const toml::key& key, const toml::node& node,
                                   const std::string& parent_path,
                                   Config& config) {
    bool found = false;
    roofer::config::for_each_field(config, [&](auto field, auto& field_value) {
      if (found || key != field.toml_name()) return;
      found = true;
      const auto path = parent_path + "." + field.toml_name();
      if (!assign_toml_value(node, field_value)) {
        throw std::runtime_error("Invalid value type or value for " + path +
                                 ".");
      }
      if (field.validator) {
        if (auto error = field.validator(field_value)) {
          throw std::runtime_error("Validation error for " + path + ": " +
                                   *error + ".");
        }
      }
    });
    return found;
  }

  template <typename Component>
  void parse_reconstruction_component(const toml::table& table,
                                      const std::string& path,
                                      Component& component) {
    for (const auto& [key, node] : table) {
      if (!assign_reconstruction_field(key, node, path, component)) {
        throw std::runtime_error("Unknown reconstruction parameter: " + path +
                                 "." + std::string(key.str()) + ".");
      }
    }
  }

  void parse_reconstruction_table(const toml::table& table) {
    for (const auto& [key, node] : table) {
      if (assign_reconstruction_field(key, node, "reconstruction",
                                      cfg_.reconstruction)) {
        continue;
      }

      bool found = false;
      cfg_.reconstruction.visit_components(
          [&](std::string_view component_name, auto& component) {
            if (!found && key == component_name) {
              found = true;
              const auto* component_table = node.as_table();
              const auto path = "reconstruction." + std::string(component_name);
              if (!component_table) {
                throw std::runtime_error(path + " must be a table.");
              }
              parse_reconstruction_component(*component_table, path, component);
            }
          });
      if (!found) {
        throw std::runtime_error(
            "Unknown reconstruction parameter: reconstruction." +
            std::string(key.str()) + ".");
      }
    }
  }

  void print_help_header(const std::string& program_name, bool compact) {
    // see http://docopt.org/
    std::cout << "Automatic LoD 2.2 building reconstruction from "
                 "airborne lidar pointclouds\n\n";
    std::cout << "\033[1mUsage\033[0m:" << "\n";
    std::cout << "  " << program_name;
    std::cout << " [options] <pointcloud-path>... <polygon-source> "
                 "<output-directory>"
              << "\n";
    std::cout << "  " << program_name;
    if (compact) {
      std::cout
          << " -c <config-file> [<pointcloud-path>... <polygon-source>]\n";
      std::cout << "         <output-directory>\n";
    } else {
      std::cout
          << " [options] (-c | --config) <config-file> [(<pointcloud-path>... "
             "<polygon-source>)] <output-directory>"
          << "\n";
    }
    std::cout << "  " << program_name;
    std::cout << " -h | --help" << "\n";
    std::cout << "  " << program_name;
    std::cout << " -H | --help-all" << "\n";
    std::cout << "  " << program_name;
    std::cout << " -a | --attributes" << "\n";
    std::cout << "  " << program_name;
    std::cout << " -v | --version" << "\n";
    std::cout << "\n";
  }

  void print_positional_arguments(bool compact) {
    std::cout << "\033[1mPositional arguments:\033[0m" << "\n";
    if (compact) {
      std::cout
          << "  <pointcloud-path>            LAS/LAZ file or directory.\n";
      std::cout << "  <polygon-source>             Roofprint polygons, for "
                   "example GPKG\n";
      std::cout << "                               or OGR connection string.\n";
    } else {
      std::cout << "  <pointcloud-path>            Path to pointcloud file "
                   "(.LAS or .LAZ) or folder that contains pointcloud files.\n";
      std::cout << "  <polygon-source>             Path to roofprint polygons "
                   "source. "
                   "Can be "
                   "an OGR supported file (eg. GPKG) or database connection "
                   "string.\n";
    }
    std::cout << "  <output-directory>           Output directory.\n";
  }

  void print_help(std::string program_name) {
    print_help_header(program_name, true);

    std::cout << "\033[1mExamples:\033[0m" << "\n";
    std::cout << "  " << program_name;
    std::cout << " pointcloud.laz roofprints.gpkg output-dir\n";
    std::cout << "  " << program_name;
    std::cout << " --lod12 --lod22 pointcloud.laz roofprints.gpkg output-dir\n";
    std::cout << "  " << program_name;
    std::cout << " --filter 'identificatie=1980100000265200'\n";
    std::cout << "         pointcloud.laz roofprints.gpkg output-dir\n";
    std::cout << "  " << program_name;
    std::cout << " -c config.toml output-dir\n";
    std::cout << "\n";
    print_positional_arguments(true);
    std::cout << "\n";
    std::cout << "\033[1mCommon options:\033[0m" << "\n";
    std::cout << "  -c, --config <file>          Read options from a TOML "
                 "configuration file.\n";
    std::cout
        << "  -j, --jobs <int>             Number of worker jobs to use.\n";
    std::cout
        << "  --lod12                      Generate LoD 1.2 geometries.\n";
    std::cout
        << "  --lod13                      Generate LoD 1.3 geometries.\n";
    std::cout
        << "  --lod22                      Generate LoD 2.2 geometries.\n";
    std::cout << "  -a, --attributes             List output attributes.\n";
    std::cout << "  -v, --version                Show version.\n";
    std::cout
        << "  -H, --help-all               Show all options and defaults.\n";
    std::cout << "\n";
    std::cout << "Use '" << program_name
              << " --help-all' to show all options and defaults.\n";
  }

  void print_help_all(std::string program_name) {
    print_help_header(program_name, false);
    print_positional_arguments(false);

    print_params(app_param_groups_);
    print_params(param_groups_);
  }

  void print_attributes() {
    const size_t name_column_width =
        24;  // Fixed width for attribute name column
    const size_t desc_column_width =
        66;  // Fixed width for description column (total ~80 chars)

    std::cout << "\033[1mOutput attributes:\033[0m\n";
    for (const auto& [name, attrib] : output_attr_) {
      // Wrap the description text
      auto wrapped_desc =
          wrap_text(attrib.description, name_column_width + desc_column_width,
                    name_column_width + 2);

      // Print attribute name and first line of description
      std::cout << " " << std::setw(name_column_width) << std::left << name;
      if (!wrapped_desc.empty()) {
        std::cout << wrapped_desc[0].substr(name_column_width + 2) << "\n";
      } else {
        std::cout << "\n";
      }

      // Print remaining wrapped description lines
      for (size_t i = 1; i < wrapped_desc.size(); ++i) {
        std::cout << wrapped_desc[i] << "\n";
      }
    }
  }

  // Utility function to wrap text to a specified width with proper indentation
  std::vector<std::string> wrap_text(const std::string& text, size_t max_width,
                                     size_t indent = 0) {
    std::vector<std::string> lines;
    std::string indent_str(indent, ' ');
    std::string current_line = indent_str;
    size_t current_width = indent;

    std::istringstream iss(text);
    std::string word;

    while (iss >> word) {
      // Check if adding the word exceeds max_width
      if (current_width + word.length() + (current_line.empty() ? 0 : 1) >
          max_width) {
        if (!current_line.empty() && current_line != indent_str) {
          lines.push_back(current_line);
          current_line = indent_str;
          current_width = indent;
        }
      }
      if (!current_line.empty() && current_line != indent_str) {
        current_line += " ";
        current_width += 1;
      }
      current_line += word;
      current_width += word.length();
    }
    if (!current_line.empty() && current_line != indent_str) {
      lines.push_back(current_line);
    }
    return lines;
  }

  void print_params(param_group_map& params) {
    const size_t param_column_width = 35;  // Fixed width for parameter column
    const size_t desc_column_width = 65;   // Fixed width for description column

    for (auto& [group_name, group] : params) {
      if (group.empty()) continue;
      std::cout << "\n";
      std::cout << "\033[1m" << group_name << " options:\033[0m\n";
      for (auto& param : group) {
        std::string param_text =
            param->cli_flag() + " " + param->type_description();
        std::string desc = param->description();
        std::string default_text = "Default: " + param->default_to_string();

        // Wrap the description and default text
        auto wrapped_desc =
            wrap_text(desc, param_column_width + desc_column_width,
                      param_column_width + 2);
        auto wrapped_default =
            wrap_text(default_text, param_column_width + desc_column_width,
                      param_column_width + 2);

        // Print parameter and first line of description
        if (param_text.size() <= param_column_width - 2) {
          std::cout << "  " << std::setw(param_column_width) << std::left
                    << param_text;
          if (!wrapped_desc.empty()) {
            std::cout << wrapped_desc[0].substr(param_column_width + 2) << "\n";
          } else {
            std::cout << "\n";
          }
        } else {
          // If parameter text is too long, print it on its own line
          std::cout << "  " << param_text << "\n";
          if (!wrapped_desc.empty()) {
            std::cout << std::string(param_column_width + 2, ' ')
                      << wrapped_desc[0].substr(param_column_width + 2) << "\n";
          }
        }

        // Print remaining wrapped description lines
        for (size_t i = 1; i < wrapped_desc.size(); ++i) {
          std::cout << wrapped_desc[i] << "\n";
        }

        // Print default value lines in blue
        for (const auto& line : wrapped_default) {
          if (line.size() > param_column_width + desc_column_width - 3) {
            // trim the line to fit within the column width and add ellipsis
            std::string trimmed_line =
                line.substr(0, param_column_width + desc_column_width - 3) +
                "...";
            std::cout << "\033[34m" << trimmed_line << "\033[0m" << "\n";
          } else {
            // print the line as is
            std::cout << "\033[34m" << line << "\033[0m" << "\n";
          }
        }
      }
    }
  }

  void print_version() {
    std::cout << std::format("roofer {} ({})\n", RF_VERSION, RF_GIT_HASH);
  }

  void parse_cli_first_pass(CLIArgs& c) {
    // parse program control arguments (not in config file)
    auto it = c.args.begin();
    while (it != c.args.end()) {
      const std::string& arg = *it;
      std::string argname = "";
      if (arg.starts_with("--")) {
        argname = arg.substr(2);
        if (auto p = app_param_index_.find(argname);
            p != app_param_index_.end()) {
          it = c.args.erase(it);
          it = p->second->set(c.args, it);
        } else {
          ++it;
        }
      } else if (arg.starts_with("-")) {
        argname = arg.substr(1);
        if (auto p = app_param_index_.find(argname);
            p != app_param_index_.end()) {
          it = c.args.erase(it);
          it = p->second->set(c.args, it);
        } else {
          ++it;
        }
      } else {
        ++it;
      }

      if (argname == "t" || argname == "trace-interval") {
        _loglevel = roofer::logger::LogLevel::trace;
      }
      if (argname == "-c" || argname == "--config") {
        if (auto error_msg = check::PathExists(_config_path)) {
          throw std::runtime_error(std::format(
              "Invalid argument for -c or --config. {}", *error_msg));
        }
      }

      if (argname == "crop-output-all" || argname == "crop-rasters") {
        cfg_.write_crop_outputs = true;
      }
    }
  }

  void parse_cli_second_pass(CLIArgs& c) {
    auto& logger = roofer::logger::Logger::get_logger();
    const auto warn_if_legacy = [&](const std::string& name) {
      if (const auto destination = legacy_reconstruction_destination(name);
          destination && destination->cli_deprecated) {
        if (destination->ignored) {
          logger.warning(
              "Option --{} is deprecated and ignored; use {} under [{}] in "
              "the configuration file instead. It will be rejected in 2.0.",
              name, destination->nested_key, destination->section);
        } else {
          logger.warning(
              "Option --{} is deprecated; use {} under [{}] in the "
              "configuration file. It will be removed in 2.0.",
              name, destination->nested_key, destination->section);
        }
      }
    };
    auto it = c.args.begin();
    while (it != c.args.end()) {
      std::string arg = *it;

      try {
        if (arg.starts_with("--no")) {
          auto argname = arg.substr(5);
          if (auto p = param_index_.find(argname); p != param_index_.end()) {
            warn_if_legacy(argname);
            it = c.args.erase(it);
            p->second->unset();
          } else {
            throw std::runtime_error(std::format("Unknown argument: {}.", arg));
          }
        } else if (arg.starts_with("--")) {
          auto argname = arg.substr(2);
          if (auto p = param_index_.find(argname); p != param_index_.end()) {
            warn_if_legacy(argname);
            it = c.args.erase(it);
            it = p->second->set(c.args, it);
          } else {
            throw std::runtime_error(std::format("Unknown argument: {}.", arg));
          }
        } else if (arg.starts_with("-")) {
          auto argname = arg.substr(1);
          if (auto p = param_index_.find(argname); p != param_index_.end()) {
            it = c.args.erase(it);
            it = p->second->set(c.args, it);
          } else {
            throw std::runtime_error(std::format("Unknown argument: {}.", arg));
          }
        } else {
          ++it;
        }
      } catch (const std::exception& e) {
        throw std::runtime_error(
            std::format("Error parsing argument: {}. {}.", arg, e.what()));
      }
    }

    // now c.arg shoulg only contain positional arguments
    // we assume either only the output path given on CLI or also fp and pc
    // inputs
    bool fp_set = cfg_.source_footprints.size() > 0;
    bool pc_set = input_pointclouds_.size() > 0;
    bool output_set = cfg_.output_path.size() > 0;

    if (pc_set && fp_set && output_set && c.args.size() == 0) {
      // all set
    } else if (pc_set && fp_set && c.args.size() == 1) {
      cfg_.output_path = c.args.back();
    } else if (c.args.size() > 2) {
      cfg_.output_path = c.args.back();
      c.args.pop_back();
      cfg_.source_footprints = c.args.back();
      c.args.pop_back();

      input_pointclouds_.clear();
      input_pointclouds_.emplace_back(InputPointcloud{
          .paths = find_filepaths(c.args, {".las", ".LAS", ".laz", ".LAZ"},
                                  _skip_pc_check),
          .bld_class = cfg_.bld_class,
          .grnd_class = cfg_.grnd_class});
    } else {
      throw std::runtime_error(
          "Unable set all inputs and output. Need to provide at least <ouput "
          "path> and set input paths in config file or provide all of <point "
          "cloud sources> <polygon source> <ouput path>.");
    }
  };

  void parse_config_file() {
    auto& logger = roofer::logger::Logger::get_logger();
    toml::table config;
    try {
      config = toml::parse_file(_config_path);
    } catch (const std::exception& e) {
      throw std::runtime_error(std::format("Syntax error."));
    }

    // iterate config table
    for (const auto& [key, value] : config) {
      try {
        if (const auto destination =
                legacy_reconstruction_destination(key.str())) {
          if (destination->ignored) {
            logger.warning(
                "Root configuration key {} is deprecated and ignored; use {} "
                "under [{}] instead. It will be rejected in 2.0.",
                key.data(), destination->nested_key, destination->section);
          } else {
            logger.warning(
                "Root configuration key {} is deprecated; use {} under [{}] "
                "instead. It will be removed in 2.0.",
                key.data(), destination->nested_key, destination->section);
          }
        }
        if (key == "polygon-source") {
          get_toml_value(config, "polygon-source", cfg_.source_footprints);
        } else if (key == "output-directory") {
          get_toml_value(config, "output-directory", cfg_.output_path);
        } else if (key == "output-attributes") {
          if (toml::table* tb = config["output-attributes"].as_table()) {
            for (const auto& [key, value] : *tb) {
              if (auto p = output_attr_.find(key.data());
                  p != output_attr_.end()) {
                std::string name = "";
                get_toml_value(*tb, key.data(), *(p->second.value));
                // if (!name.empty()) {
                //   p->second = name;
                // }
              } else {
                throw std::runtime_error(
                    fmt::format("Unknown output attribute: {}.", key.data()));
              }
            }
          }
        } else if (key == "reconstruction") {
          // Parsed after all root-level compatibility aliases so nested values
          // deterministically take precedence, independent of table ordering.
        } else if (auto p =
                       root_only_reconstruction_alias_index_.find(key.data());
                   p != root_only_reconstruction_alias_index_.end()) {
          p->second->set_from_toml(config, key.data());
        } else if (auto p = param_index_.find(key.data());
                   p != param_index_.end()) {
          p->second->set_from_toml(config, key.data());
        } else if (key == "pointclouds") {
          if (toml::array* arr = config["pointclouds"].as_array()) {
            // visitation with for_each() helps deal with heterogeneous data
            for (auto& el : *arr) {
              toml::table* tb = el.as_table();
              auto& pc = input_pointclouds_.emplace_back();

              for (const auto& [key, value] : *tb) {
                if (key == "name") {
                  get_toml_value(*tb, "name", pc.name);
                } else if (key == "quality") {
                  get_toml_value(*tb, "quality", pc.quality);
                } else if (key == "date") {
                  get_toml_value(*tb, "date", pc.date);
                } else if (key == "force_lod11") {
                  get_toml_value(*tb, "force_lod11", pc.force_lod11);
                } else if (key == "select_only_for_date") {
                  get_toml_value(*tb, "select_only_for_date",
                                 pc.select_only_for_date);
                } else if (key == "building_class") {
                  get_toml_value(*tb, "building_class", pc.bld_class);
                } else if (key == "ground_class") {
                  get_toml_value(*tb, "ground_class", pc.grnd_class);
                } else if (key == "source") {
                  std::list<std::string> input_paths;
                  if (toml::array* pc_paths = tb->at("source").as_array()) {
                    for (auto& pc_path : *pc_paths) {
                      input_paths.push_back(*pc_path.value<std::string>());
                    }
                  } else {
                    throw std::runtime_error(
                        "Failed to read pointclouds.source. Make sure it is "
                        "a "
                        "list of "
                        "strings.");
                  }
                  pc.paths = find_filepaths(input_paths,
                                            {".las", ".LAS", ".laz", ".LAZ"},
                                            _skip_pc_check);
                } else {
                  throw std::runtime_error(std::format(
                      "Unknown parameter in [[pointcloud]] table in "
                      "config file: {}.",
                      key.data()));
                }
              }
            };
          }
        } else {
          throw std::runtime_error(
              std::format("Unknown parameter in config file: {}.", key.data()));
        }
      } catch (const std::exception& e) {
        throw std::runtime_error(
            std::format("Failed to read value for {} from config file. {}",
                        key.data(), e.what()));
      }
    }
    if (config.contains("plane-detect-normal-angle")) {
      cfg_.reconstruction.plane_detector.normal_angle_threshold =
          roofer::reconstruction::normal_angle_degrees_from_dot_product(
              _legacy_plane_detect_normal_angle);
    }
    if (const auto* reconstruction = config["reconstruction"].as_table()) {
      parse_reconstruction_table(*reconstruction);
    } else if (config.contains("reconstruction")) {
      throw std::runtime_error("reconstruction must be a table.");
    }
  }
};

#undef ROOFER_LEGACY_RECONSTRUCTION_ALIASES

template <>
struct fmt::formatter<RooferConfigHandler> {
  static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context>
  auto format(RooferConfigHandler const& cfgh, Context& ctx) const {
    fmt::format_to(ctx.out(), "RooferConfig(source_roofprints={}",
                   cfgh.cfg_.source_footprints);

    // Add all parameters
    for (const auto& [groupname, param_list] : cfgh.param_groups_) {
      for (const auto& param : param_list) {
        fmt::format_to(ctx.out(), ", {}={}", param->longname_,
                       param->to_string());
      }
    }

    return fmt::format_to(ctx.out(), ")");
  }
};
