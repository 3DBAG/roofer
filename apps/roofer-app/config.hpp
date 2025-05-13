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
// Balázs Dukai
#pragma once

#include <functional>
#include <concepts>
#include <thread>
#include <algorithm>
#include "toml.hpp"
#include "fmt/format.h"
#include "fmt/ranges.h"

#include <roofer/common/common.hpp>
#include <roofer/logger/logger.h>
#include <roofer/misc/Vector2DOps.hpp>
#include <stdexcept>
#include <string>
#include <list>
#include <filesystem>
#include <utility>
#include "git.h"

namespace roofer::enums {
  enum TerrainStrategy {
    BUFFER_TILE = 0,
    BUFFER_USER = 1,
    USER = 2
    // POLYGON_Z = 3
  };
}

#include "validators.hpp"
#include "parameter.hpp"

namespace fs = std::filesystem;
namespace check = roofer::validators;

using fileExtent = std::pair<std::string, roofer::TBox<double>>;

struct InputPointcloud {
  std::vector<std::string> paths;
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
  roofer::vec1b is_glass_roof;
  roofer::vec1b lod11_forced;
  roofer::vec1b pointcloud_insufficient;
  std::vector<roofer::LinearRing> nodata_circles;
  std::vector<roofer::PointCollection> building_clouds;
  std::vector<roofer::ImageMap> building_rasters;
  roofer::veco1f ground_elevations;
  roofer::vec1f roof_elevations;
  roofer::vec1i acquisition_years;

  std::unique_ptr<roofer::misc::RTreeInterface> rtree;
  std::vector<fileExtent> file_extents;
};

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
  int lod11_fallback_area = 69000;
  float lod11_fallback_density = 5;
  roofer::arr2f tilesize = {1000, 1000};
  bool clear_if_insufficient = true;
  bool compute_pc_98p = false;

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

  // reconstruct
  roofer::enums::TerrainStrategy h_terrain_strategy =
      roofer::enums::TerrainStrategy::BUFFER_TILE;
  int lod11_fallback_planes = 900;
  int lod11_fallback_time = 1800000;
  float complexity_factor = 0.888;

  bool clip_ground = true;
  bool lod_12 = false;
  bool lod_13 = false;
  bool lod_22 = true;
  float lod13_step_height = 3.;
  float floor_elevation = 0.;
  int plane_detect_k = 15;
  int plane_detect_min_points = 15;
  float plane_detect_epsilon = 0.300000;
  float plane_detect_normal_angle = 0.750000;
  float line_detect_epsilon = 1.000000;
  float thres_alpha = 0.250000;
  float thres_reg_line_dist = 0.800000;
  float thres_reg_line_ext = 3.000000;

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

struct RooferConfigHandler {
  RooferConfig cfg_;

  using param_group_map = std::vector<std::pair<std::string, ParameterVector>>;

  std::vector<InputPointcloud> input_pointclouds_;
  param_group_map app_param_groups_;
  param_group_map param_groups_;
  DocAttribMap output_attr_;
  std::unordered_map<std::string, ConfigParameter*> param_index_;
  std::unordered_map<std::string, ConfigParameter*> app_param_index_;

  // flags
  bool _print_help = false;
  bool _print_version = false;
  bool _crop_only = false;
  bool _no_tiling = false;
  bool _skip_pc_check = false;
  roofer::logger::LogLevel _loglevel = roofer::logger::LogLevel::info;
  int _trace_interval = 10;
  std::string _config_path;
  int _jobs = std::thread::hardware_concurrency();

  // methods
  RooferConfigHandler() {
    ParameterVector input, crop, reconstruction, output;
    ParameterVector general;

    general.add("help", 'h', "Show help message", _print_help);
    general.add("version", 'v', "Show version", _print_version);
    general.add("jobs", 'j', "Number of threads to use", _jobs);
    general.add("config", 'c', "Configuration file", _config_path,
                {check::PathExists, check::DirIsWritable});
    general.add("trace-interval", "Interval for tracing (in seconds)",
                _trace_interval, {check::HigherThan<int>(0)});
    general.add("loglevel",
                "Specify loglevel. Can be "
                "info, warning, debug, trace",
                _loglevel);
#ifdef RF_USE_RERUN
    general.add("rerun", "Log intermediate results to rerun", cfg_.use_rerun);
#endif

    input.add("id-attribute", "Building ID attribute", cfg_.id_attribute);
    input.add("force-lod11-attribute", "Building attribute for forcing lod11",
              cfg_.force_lod11_attribute);
    input.add("yoc-attribute",
              "Attribute containing building year of construction",
              cfg_.yoc_attribute);
    input.add("h-terrain-attribute",
              "Attribute containing (fallback) terrain height for buildings",
              cfg_.h_terrain_attribute);
    input.add("h-roof-attribute",
              "Attribute containing fallback roof height for buildings",
              cfg_.h_roof_attribute);
    input.add("polygon-source-layer",
              "Load this layer from <polygon-source> [default: first layer]",
              cfg_.layer_name);
    input.add("filter",
              "Specify WHERE clause in OGR SQL to select specfic features from "
              "<polygon-source>",
              cfg_.attribute_filter);
    input
        .add("box",
             "Region of interest. Data outside of this region will be ignored",
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
        .add("srs", "Manually set Spatial Reference System for input data",
             cfg_.srs_override)
        .example_ = "\"EPSG:7415\"";
    // important pointcloud parameters, that are not related to pointcloud
    // selection
    input.add("bld-class", "LAS classification code for building points",
              cfg_.bld_class, {check::HigherOrEqualTo<int>(0)});
    input.add("grnd-class", "LAS classification code for ground points",
              cfg_.grnd_class, {check::HigherOrEqualTo<int>(0)});
    input.add("skip-pc-check",
              "Disable/enable check if all supplied pointcloud files exist",
              _skip_pc_check);

    crop.add("ceil-point-density",
             "Enforce this point density ceiling on each building pointcloud.",
             cfg_.ceil_point_density, {check::HigherThan<float>(0)});
    crop.add("cellsize", "Cellsize used for quick pointcloud analysis",
             cfg_.cellsize, {check::HigherThan<float>(0)});
    crop.add("lod11-fallback-area", "lod11 fallback area",
             cfg_.lod11_fallback_area, {check::HigherThan<int>(0)});
    crop.add("clear-insufficient",
             "reconstruct buildings with insufficient pointcloud data",
             cfg_.clear_if_insufficient);
    crop.add("compute-pc-98p",
             "compute 98th percentile of pointcloud height for each building",
             cfg_.compute_pc_98p);
    crop.add("crop-only", "Crop only, no reconstruction", _crop_only);
    crop.add("crop-output", "Output cropped building pointclouds",
             cfg_.write_crop_outputs);
    crop.add("crop-output-all",
             "Output files for each "
             "candidate pointcloud instead of only the optimal candidate. "
             "Implies --crop-output",
             cfg_.output_all);
    crop.add("crop-rasters",
             "Output rasterised crop "
             "pointclouds. Implies --crop-output",
             cfg_.write_rasters);
    crop.add("index",
             "Output index.gpkg file with "
             "crop analytics.",
             cfg_.write_index);

    reconstruction.add("lod12", "Generate LoD 1.2 geometry", cfg_.lod_12);
    reconstruction.add("lod13", "Generate LoD 1.3 geometry", cfg_.lod_13);
    reconstruction.add("lod22", "Generate LoD 2.2 geometry", cfg_.lod_22);
    reconstruction.add("complexity-factor",
                       "Complexity factor building reconstruction",
                       cfg_.complexity_factor, {check::InRange<float>(0, 1)});
    reconstruction.add("clip-terrain", "Clip terrain parts from roofprint",
                       cfg_.clip_ground, {});
    reconstruction.add("lod13-step-height",
                       "Step height used for LoD 1.3 generation",
                       cfg_.lod13_step_height, {check::HigherThan<float>(0)});
    reconstruction.add("plane-detect-k", "plane detect k", cfg_.plane_detect_k,
                       {check::HigherThan<int>(0)});
    reconstruction.add("plane-detect-min-points", "plane detect min points",
                       cfg_.plane_detect_min_points,
                       {check::HigherThan<int>(2)});
    reconstruction.add("plane-detect-epsilon", "plane detect epsilon",
                       cfg_.plane_detect_epsilon,
                       {check::HigherThan<float>(0)});
    reconstruction
        .add("h-terrain-strategy", "Terrain height strategy",
             cfg_.h_terrain_strategy)
        .example_ = "\"buffer_tile\"";
    reconstruction.add("lod11-fallback-planes",
                       "Number of planes for LOD11 fallback",
                       cfg_.lod11_fallback_planes, {check::HigherThan<int>(0)});
    reconstruction.add("lod11-fallback-time", "Time for LOD11 fallback",
                       cfg_.lod11_fallback_time, {check::HigherThan<int>(0)}),

        output.add("no-tiling", "Disable/enable tiling", _no_tiling);
    output.add("tilesize", "Tilesize used for output tiles", cfg_.tilesize,
               {check::HigherThan<roofer::arr2f>({0, 0})});
    output.add("split-cjseq",
               "Output CityJSONSequence file for each building instead of one "
               "file per tile",
               cfg_.split_cjseq);
    output.add("omit-metadata", "Omit metadata in CityJSON output",
               cfg_.omit_metadata);
    output.add("cj-scale", "Scaling applied to CityJSON output vertices",
               cfg_.cj_scale);
    output
        .add("cj-translate",
             "Translation applied to CityJSON output vertices. Uses tile "
             "center by "
             "default.",
             cfg_.cj_translate)
        .example_ = "[100000, 200000, 0]";

    output_attr_.emplace("success",
                         DocAttrib(&cfg_.a_success,
                                   "indicates if processing completed without "
                                   "unexpected errors"));
    output_attr_.emplace("reconstruction_time",
                         DocAttrib(&cfg_.a_reconstruction_time,
                                   "Indicates reconstruction time in ms"));
    output_attr_.emplace("val3dity_lod12",
                         DocAttrib(&cfg_.a_val3dity_lod12,
                                   "Lists val3dity codes for LOD12 geometry"));
    output_attr_.emplace("val3dity_lod13",
                         DocAttrib(&cfg_.a_val3dity_lod13,
                                   "Lists val3dity codes for LOD13 geometry"));
    output_attr_.emplace("val3dity_lod22",
                         DocAttrib(&cfg_.a_val3dity_lod22,
                                   "Lists val3dity codes for LOD22 geometry"));
    output_attr_.emplace("is_glass_roof",
                         DocAttrib(&cfg_.a_is_glass_roof,
                                   "Indicates if a glass roof was detected"));
    output_attr_.emplace("nodata_frac",
                         DocAttrib(&cfg_.a_nodata_frac,
                                   "Indicates the fraction of the roofprint "
                                   "that is not covered by pointcloud data"));
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
    output_attr_.emplace("pc_select",
                         DocAttrib(&cfg_.a_pc_select,
                                   "Indicates why the input pointcloud was "
                                   "selected for reconstruction"));
    output_attr_.emplace("pc_source",
                         DocAttrib(&cfg_.a_pc_source,
                                   "Indicates which input pointcloud was used "
                                   "for reconstruction"));
    output_attr_.emplace("pc_year",
                         DocAttrib(&cfg_.a_pc_year,
                                   "Indicates the acquisition year of the "
                                   "selected input pointcloud"));
    output_attr_.emplace("force_lod11",
                         DocAttrib(&cfg_.a_force_lod11,
                                   "Indicates if LOD11 was forced for the "
                                   "building"));
    output_attr_.emplace(
        "roof_type", DocAttrib(&cfg_.a_roof_type, "Indicates the roof type"));
    output_attr_.emplace(
        "h_roof_50p", DocAttrib(&cfg_.a_h_roof_50p,
                                "Indicates the 50th percentile roof height"));
    output_attr_.emplace(
        "h_roof_70p", DocAttrib(&cfg_.a_h_roof_70p,
                                "Indicates the 70th percentile roof height"));
    output_attr_.emplace(
        "h_roof_min",
        DocAttrib(&cfg_.a_h_roof_min, "Indicates the minimum roof height"));
    output_attr_.emplace(
        "h_roof_max",
        DocAttrib(&cfg_.a_h_roof_max, "Indicates the maximum roof height"));
    output_attr_.emplace(
        "h_roof_ridge",
        DocAttrib(&cfg_.a_h_roof_ridge, "Indicates the main ridge height"));
    output_attr_.emplace("h_pc_98p",
                         DocAttrib(&cfg_.a_h_pc_98p,
                                   "Indicates the 98th percentile height of "
                                   "the pointcloud"));
    output_attr_.emplace(
        "roof_n_planes",
        DocAttrib(&cfg_.a_roof_n_planes,
                  "Indicates the number of roofplanes "
                  "detected in the pointcloud (could be different from the "
                  "generated mesh "
                  "model)"));
    output_attr_.emplace(
        "roof_n_ridgelines",
        DocAttrib(&cfg_.a_roof_n_ridgelines,
                  "Indicates the number of ridgelines "
                  "detected in the pointcloud (could be different from the "
                  "generated mesh "
                  "model)"));
    output_attr_.emplace("rmse_lod12",
                         DocAttrib(&cfg_.a_rmse_lod12,
                                   "Indicates the Root Mean Square Erorr of "
                                   "the LOD12 geometry"));
    output_attr_.emplace("rmse_lod13",
                         DocAttrib(&cfg_.a_rmse_lod13,
                                   "Indicates the Root Mean Square Erorr of "
                                   "the LOD13 geometry"));
    output_attr_.emplace("rmse_lod22",
                         DocAttrib(&cfg_.a_rmse_lod22,
                                   "Indicates the Root Mean Square Erorr of "
                                   "the LOD22 geometry"));
    output_attr_.emplace("volume_lod12",
                         DocAttrib(&cfg_.a_volume_lod12,
                                   "Indicates the volume of the LOD12 "
                                   "geometry"));
    output_attr_.emplace("volume_lod13",
                         DocAttrib(&cfg_.a_volume_lod13,
                                   "Indicates the volume of the LOD13 "
                                   "geometry"));
    output_attr_.emplace("volume_lod22",
                         DocAttrib(&cfg_.a_volume_lod22,
                                   "Indicates the volume of the LOD22 "
                                   "geometry"));
    output_attr_.emplace("h_ground",
                         DocAttrib(&cfg_.a_h_ground,
                                   "Indicates the height of floor of the "
                                   "building"));
    output_attr_.emplace(
        "slope", DocAttrib(&cfg_.a_slope, "Indicates the slope of a roofpart"));
    output_attr_.emplace(
        "azimuth",
        DocAttrib(&cfg_.a_azimuth, "Indicates the azimuth of a roofpart"));
    output_attr_.emplace("extrusion_mode",
                         DocAttrib(&cfg_.a_extrusion_mode,
                                   "Indicates what extrusion mode was used "
                                   "for the building"));
    output_attr_.emplace("pointcloud_unusable",
                         DocAttrib(&cfg_.a_pointcloud_unusable,
                                   "Indicates if the pointcloud was found "
                                   "unusable for reconstruction"));
    output.add("attribute-rename",
               "Rename output attributes. Use 'old_name=new_name' format. "
               "If the new name is empty, the attribute will be removed.",
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
  };

  void validate() {
    for (auto& [group_name, group] : param_groups_) {
      for (auto& param : group) {
        if (auto error_msg = param->validate()) {
          throw std::runtime_error(
              fmt::format("Validation error for {} parameter {}. {}",
                          group_name, param->longname_, *error_msg));
        }
      }
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
    //   throw std::runtime_error(fmt::format(
    //       "Footprint source does not exist: {}.", cfg_.source_footprints));
    // }
    if (auto error_msg = check::DirIsWritable(cfg_.output_path)) {
      throw std::runtime_error(
          fmt::format("Can't write to output directory: {}", *error_msg));
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
      throw std::runtime_error(fmt::format(
          "Failed to read value for {} from config file. {}", key, e.what()));
    }
  }

  void print_help(std::string program_name) {
    // see http://docopt.org/
    std::cout << "Automatic LoD 2.2 building reconstruction from "
                 "a pointclouds\n\n";
    std::cout << "\033[1mUsage\033[0m:" << "\n";
    std::cout << "  " << program_name;
    std::cout << " [options] <pointcloud-path>... <polygon-source> "
                 "<output-directory>"
              << "\n";
    std::cout << "  " << program_name;
    std::cout
        << " [options] (-c | --config) <config-file> [(<pointcloud-path>... "
           "<polygon-source>)] <output-directory>"
        << "\n";
    std::cout << "  " << program_name;
    std::cout << " -h | --help" << "\n";
    std::cout << "  " << program_name;
    std::cout << " -v | --version" << "\n";
    std::cout << "\n";
    std::cout << "\033[1mPositional arguments:\033[0m" << "\n";
    std::cout << "  <pointcloud-path>            Path to pointcloud file "
                 "(.LAS or .LAZ) or folder that contains pointcloud files.\n";
    std::cout << "  <polygon-source>             Path to roofprint polygons "
                 "source. "
                 "Can be "
                 "an OGR supported file (eg. GPKG) or database connection "
                 "string.\n";
    std::cout << "  <output-directory>           Output directory.\n";

    print_params(app_param_groups_);
    print_params(param_groups_);
  }

  void print_params(param_group_map& params) {
    for (auto& [group_name, group] : params) {
      if (group.empty()) continue;
      std::cout << "\n";
      std::cout << "\033[1m" << group_name << " options:\033[0m\n";
      for (auto& param : group) {
        auto tdesc = param->type_description();
        auto desc = param->description();
        if (param->longname_.size() + tdesc.size() + 1 <= 33) {
          std::cout << "  " << std::setw(35) << std::left
                    << (param->cli_flag() + " " + tdesc) << desc << "\n";
          std::cout << "    " << std::setw(33) << std::left
                    << ""  // 5 characters of padding
                    << "\033[34mDefault: " << param->default_to_string()
                    << "\033[0m" << "\n";
        } else {
          std::cout << "  " << std::setw(35) << std::left << (param->cli_flag())
                    << desc << "\n";
          std::cout << "    " << std::setw(33) << std::left
                    << tdesc  // 5 characters of padding
                    << "\033[34mDefault: " << param->default_to_string()
                    << "\033[0m" << "\n";
        }
      }
    }
  }

  void print_version() {
    std::cout << fmt::format(
        "roofer {} ({}{}{})\n", git_Describe(),
        std::strcmp(git_Branch(), "main") ? ""
                                          : fmt::format("{}, ", git_Branch()),
        git_AnyUncommittedChanges() ? "dirty, " : "", git_CommitDate());
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
          throw std::runtime_error(fmt::format("Unknown argument: {}.", arg));
        }
      } else if (arg.starts_with("-")) {
        argname = arg.substr(1);
        if (auto p = app_param_index_.find(argname);
            p != app_param_index_.end()) {
          it = c.args.erase(it);
          it = p->second->set(c.args, it);
        } else {
          throw std::runtime_error(fmt::format("Unknown argument: {}.", arg));
        }
      }

      if (argname == "t" || argname == "trace-interval") {
        _loglevel = roofer::logger::LogLevel::trace;
      }
      if (argname == "-c" || argname == "--config") {
        if (auto error_msg = check::PathExists(_config_path)) {
          throw std::runtime_error(fmt::format(
              "Invalid argument for -c or --config. {}", *error_msg));
        }
      }

      if (argname == "crop-output-all" || argname == "crop-rasters") {
        cfg_.write_crop_outputs = true;
      }
    }
  }

  void parse_cli_second_pass(CLIArgs& c) {
    auto it = c.args.begin();
    while (it != c.args.end()) {
      std::string arg = *it;

      try {
        if (arg.starts_with("--no")) {
          auto argname = arg.substr(4);
          if (auto p = param_index_.find(argname); p != param_index_.end()) {
            it = c.args.erase(it);
            p->second->unset();
          } else {
            throw std::runtime_error(fmt::format("Unknown argument: {}.", arg));
          }
        } else if (arg.starts_with("--")) {
          auto argname = arg.substr(2);
          if (auto p = param_index_.find(argname); p != param_index_.end()) {
            it = c.args.erase(it);
            it = p->second->set(c.args, it);
          } else {
            throw std::runtime_error(fmt::format("Unknown argument: {}.", arg));
          }
        } else if (arg.starts_with("-")) {
          auto argname = arg.substr(1);
          if (auto p = param_index_.find(argname); p != param_index_.end()) {
            it = c.args.erase(it);
            it = p->second->set(c.args, it);
          } else {
            throw std::runtime_error(fmt::format("Unknown argument: {}.", arg));
          }
        } else {
          ++it;
        }
      } catch (const std::exception& e) {
        throw std::runtime_error(
            fmt::format("Error parsing argument: {}. {}.", arg, e.what()));
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
      throw std::runtime_error(fmt::format("Syntax error."));
    }

    // iterate config table
    for (const auto& [key, value] : config) {
      try {
        if (key == "polygon-source") {
          get_toml_value(config, "polygon-source", cfg_.source_footprints);
        } else if (key == "output-directory") {
          get_toml_value(config, "output-directory", cfg_.output_path);
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
                  throw std::runtime_error(fmt::format(
                      "Unknown parameter in [[pointcloud]] table in "
                      "config file: {}.",
                      key.data()));
                }
              }
            };
          }
        } else {
          throw std::runtime_error(
              fmt::format("Unknown parameter in config file: {}.", key.data()));
        }
      } catch (const std::exception& e) {
        throw std::runtime_error(
            fmt::format("Failed to read value for {} from config file. {}",
                        key.data(), e.what()));
      }
    }
  }
};

template <>
struct fmt::formatter<RooferConfigHandler> {
  static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context>
  auto format(RooferConfigHandler const& cfgh, Context& ctx) const {
    fmt::format_to(ctx.out(), "RooferConfig(source_rootprints={}",
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
