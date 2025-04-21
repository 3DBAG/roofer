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
#include <roofer/ReconstructionConfig.hpp>
#include <roofer/misc/Vector2DOps.hpp>
#include <stdexcept>
#include <string>
#include <list>
#include <filesystem>
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
  std::optional<roofer::arr3d> cj_scale;
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
  std::string output_stem = "tile";

  // reconstruct
  roofer::enums::TerrainStrategy h_terrain_strategy =
      roofer::enums::TerrainStrategy::BUFFER_TILE;
  int lod11_fallback_planes = 900;
  int lod11_fallback_time = 1800000;
  roofer::ReconstructionConfig rec;

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
  // parametermap
  RooferConfig cfg_;

  using param_group_map =
      std::map<std::string, std::vector<std::unique_ptr<ConfigParameter>>>;
  ;
  std::vector<InputPointcloud> input_pointclouds_;
  param_group_map params_;
  std::unordered_map<std::string, ConfigParameter*> lookup_index_;

  // flags
  bool _print_help = false;
  bool _print_version = false;
  bool _crop_only = false;
  bool _no_tiling = false;
  bool _skip_pc_check = false;
  std::string _loglevel = "info";
  size_t _trace_interval = 10;
  std::string _config_path;
  size_t _jobs = std::thread::hardware_concurrency();

  // methods
  RooferConfigHandler() {
    // add parameters
    auto input = add_group("Input options");
    auto crop = add_group("Crop options");
    auto reconstruction = add_group("Reconstruction options");
    auto output = add_group("Output options");
    auto output_attr = add_group("Output attribute names");
    auto empty = add_group("Empty group");
    // params_.emplace("Input",
    // std::vector<std::unique_ptr<ConfigParameter>>{}); params_.emplace("Crop",
    // std::vector<std::unique_ptr<ConfigParameter>>{});
    // params_.emplace("Reconstruction",
    //                 std::vector<std::unique_ptr<ConfigParameter>>{});
    // params_.emplace("Output",
    // std::vector<std::unique_ptr<ConfigParameter>>{}); params_.emplace("Output
    // attribute names", std::vector<std::unique_ptr<ConfigParameter>>{});
    // params_.emplace("", std::vector<std::unique_ptr<ConfigParameter>>{});

    p(input, "id-attribute", "Building ID attribute", cfg_.id_attribute);
    p(input, "force-lod11-attribute", "Building attribute for forcing lod11",
      cfg_.force_lod11_attribute);
    p(input, "yoc-attribute",
      "Attribute containing building year of construction", cfg_.yoc_attribute);
    p(input, "h-terrain-attribute",
      "Attribute containing (fallback) terrain height for buildings",
      cfg_.h_terrain_attribute);
    p(input, "h-roof-attribute",
      "Attribute containing fallback roof height for buildings",
      cfg_.h_roof_attribute);
    p(input, "polygon-source-layer",
      "Load this layer from <polygon-source> [default: first layer]",
      cfg_.layer_name);
    p(input, "filter",
      "Specify WHERE clause in OGR SQL to select specfic features from "
      "<polygon-source>",
      cfg_.attribute_filter);
    p(input, "box",
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
      }});
    p(input, "srs", "Manually set Spatial Reference System for input data",
      cfg_.srs_override);
    // important pointcloud parameters, that are not related to pointcloud
    // selection
    p(input, "bld-class", "LAS classification code for building points",
      cfg_.bld_class, {check::HigherOrEqualTo<int>(0)});
    p(input, "grnd-class", "LAS classification code for ground points",
      cfg_.grnd_class, {check::HigherOrEqualTo<int>(0)});

    p(crop, "ceil-point-density",
      "Enfore this point density ceiling on each building pointcloud.",
      cfg_.ceil_point_density, {check::HigherThan<float>(0)});
    p(crop, "cellsize", "Cellsize used for quick pointcloud analysis",
      cfg_.cellsize, {check::HigherThan<float>(0)});
    p(crop, "lod11-fallback-area", "lod11 fallback area",
      cfg_.lod11_fallback_area, {check::HigherThan<int>(0)});
    p(crop, "reconstruct-insufficient",
      "reconstruct buildings with insufficient pointcloud data",
      cfg_.clear_if_insufficient);
    p(crop, "compute-pc-98p",
      "compute 98th percentile of pointcloud height for each building",
      cfg_.compute_pc_98p);

    p(reconstruction, "lod",
      "Which LoDs to generate, possible values: 12, 13, 22 [default: "
      "all]",
      cfg_.rec.lod, {check::OneOf<int>({0, 12, 13, 22})});
    p(reconstruction, "complexity-factor",
      "Complexity factor building reconstruction", cfg_.rec.complexity_factor,
      {check::InRange<float>(0, 1)});
    p(reconstruction, "no-clip", "Do not clip terrain parts from roofprint",
      cfg_.rec.clip_ground, {});
    p(reconstruction, "lod13-step-height",
      "Step height used for LoD1.3 generation", cfg_.rec.lod13_step_height,
      {check::HigherThan<float>(0)});
    p(reconstruction, "plane-detect-k", "plane detect k",
      cfg_.rec.plane_detect_k, {check::HigherThan<int>(0)});
    p(reconstruction, "plane-detect-min-points", "plane detect min points",
      cfg_.rec.plane_detect_min_points, {check::HigherThan<int>(2)});
    p(reconstruction, "plane-detect-epsilon", "plane detect epsilon",
      cfg_.rec.plane_detect_epsilon, {check::HigherThan<float>(0)});
    p(reconstruction, "h-terrain-strategy", "Terrain height strategy",
      cfg_.h_terrain_strategy, {check::OneOf<int>({0, 1, 2})});
    p(reconstruction, "lod11-fallback-planes",
      "Number of planes for LOD11 fallback", cfg_.lod11_fallback_planes,
      {check::HigherThan<int>(0)});
    p(reconstruction, "lod11-fallback-time", "Time for LOD11 fallback",
      cfg_.lod11_fallback_time, {check::HigherThan<int>(0)}),

        p(output, "tilesize", "Tilesize used for output tiles", cfg_.tilesize,
          {check::HigherThan<roofer::arr2f>({0, 0})});
    p(output, "output-stem", "Filename stem for output tiles,",
      cfg_.output_stem);
    p(output, "split-cjseq",
      "Output CityJSONSequence file for each building instead of one "
      "file per tile",
      cfg_.split_cjseq);
    p(output, "omit-metadata", "Omit metadata in CityJSON output",
      cfg_.omit_metadata);
    p(output, "cj-scale", "Scaling applied to CityJSON output vertices",
      cfg_.cj_scale);
    p(output, "cj-translate", "Translation applied to CityJSON output vertices",
      cfg_.cj_translate);
    p(output, "a_success",
      "Name of output attribute that indicates if processing completed without "
      "unexpected errors",
      cfg_.a_success);
    p(output_attr, "a_reconstruction_time",
      "Indicates reconstruction time in ms", cfg_.a_reconstruction_time);
    p(output_attr, "a_val3dity_lod12",
      "Lists val3dity codes for LOD12 geometry", cfg_.a_val3dity_lod12);
    p(output_attr, "a_val3dity_lod13",
      "Lists val3dity codes for LOD13 geometry", cfg_.a_val3dity_lod13);
    p(output_attr, "a_val3dity_lod22",
      "Lists val3dity codes for LOD22 geometry", cfg_.a_val3dity_lod22);
    p(output_attr, "a_is_glass_roof", "Indicates if a glass roof was detected",
      cfg_.a_is_glass_roof);
    p(output_attr, "a_nodata_frac",
      "Indicates the fraction of the roofprint "
      "that is not covered by pointcloud data",
      cfg_.a_nodata_frac);
    p(output_attr, "a_nodata_r",
      "Indicates the radius of the largest "
      "circle in the roofprint that is not covered by pointcloud data",
      cfg_.a_nodata_r);
    p(output_attr, "a_pt_density",
      "Indicates the point density inside the "
      "roofprint",
      cfg_.a_pt_density);
    p(output_attr, "a_is_mutated",
      "Indicates if the building was mutated "
      "between multiple input pointclouds (if multiple input pointclouds were "
      "provided)",
      cfg_.a_is_mutated);
    p(output_attr, "a_pc_select",
      "Indicates why the input pointcloud was "
      "selected for reconstruction",
      cfg_.a_pc_select);
    p(output_attr, "a_pc_source",
      "Indicates which input pointcloud was used "
      "for reconstruction",
      cfg_.a_pc_source);
    p(output_attr, "a_pc_year",
      "Indicates the acquisition year of the "
      "selected input pointcloud",
      cfg_.a_pc_year);
    p(output_attr, "a_force_lod11",
      "Indicates if LOD11 was forced for the "
      "building",
      cfg_.a_force_lod11);
    p(output_attr, "a_roof_type", "Indicates the roof type", cfg_.a_roof_type);
    p(output_attr, "a_h_roof_50p", "Indicates the 50th percentile roof height",
      cfg_.a_h_roof_50p);
    p(output_attr, "a_h_roof_70p", "Indicates the 70th percentile roof height",
      cfg_.a_h_roof_70p);
    p(output_attr, "a_h_roof_min", "Indicates the minimum roof height",
      cfg_.a_h_roof_min);
    p(output_attr, "a_h_roof_max", "Indicates the maximum roof height",
      cfg_.a_h_roof_max);
    p(output_attr, "a_h_roof_ridge", "Indicates the main ridge height",
      cfg_.a_h_roof_ridge);
    p(output_attr, "a_h_pc_98p",
      "Indicates the 98th percentile height of "
      "the pointcloud",
      cfg_.a_h_pc_98p);
    p(output_attr, "a_roof_n_planes",
      "Indicates the number of roofplanes "
      "detected in the pointcloud (could be different from the generated mesh "
      "model)",
      cfg_.a_roof_n_planes);
    p(output_attr, "a_roof_n_ridgelines",
      "Indicates the number of ridgelines "
      "detected in the pointcloud (could be different from the generated mesh "
      "model)",
      cfg_.a_roof_n_ridgelines);
    p(output_attr, "a_rmse_lod12",
      "Indicates the Root Mean Square Erorr of "
      "the LOD12 geometry",
      cfg_.a_rmse_lod12);
    p(output_attr, "a_rmse_lod13",
      "Indicates the Root Mean Square Erorr of "
      "the LOD13 geometry",
      cfg_.a_rmse_lod13);
    p(output_attr, "a_rmse_lod22",
      "Indicates the Root Mean Square Erorr of "
      "the LOD22 geometry",
      cfg_.a_rmse_lod22);
    p(output_attr, "a_volume_lod12",
      "Indicates the volume of the LOD12 "
      "geometry",
      cfg_.a_volume_lod12);
    p(output_attr, "a_volume_lod13",
      "Indicates the volume of the LOD13 "
      "geometry",
      cfg_.a_volume_lod13);
    p(output_attr, "a_volume_lod22",
      "Indicates the volume of the LOD22 "
      "geometry",
      cfg_.a_volume_lod22);
    p(output_attr, "a_h_ground",
      "Indicates the height of floor of the "
      "building",
      cfg_.a_h_ground);
    p(output_attr, "a_slope", "Indicates the slope of a roofpart",
      cfg_.a_slope);
    p(output_attr, "a_azimuth", "Indicates the azimuth of a roofpart",
      cfg_.a_azimuth);
    p(output_attr, "a_extrusion_mode",
      "Indicates what extrusion mode was used "
      "for the building",
      cfg_.a_extrusion_mode);
    p(output_attr, "a_pointcloud_unusable",
      "Indicates if the pointcloud was found "
      "unusable for reconstruction",
      cfg_.a_pointcloud_unusable);

#ifdef RF_USE_RERUN
    p("", "use-rerun", "Use rerun", cfg_.use_rerun),
#endif
  };

  template <typename T>
  void p(param_group_map::iterator& group, const std::string& longname,
         const std::string& help, T& value,
         std::vector<Validator<T>> validators = {}) {
    group->second.emplace_back(std::make_unique<ConfigParameterByReference<T>>(
        longname, help, value, std::move(validators)));
  }
  template <typename T>
  void p(param_group_map::iterator& group, const std::string& longname,
         const char shortname, const std::string& help, T& value,
         std::vector<Validator<T>> validators = {}) {
    group->second.emplace_back(std::make_unique<ConfigParameterByReference<T>>(
        longname, shortname, help, value, std::move(validators)));
  }

  param_group_map::iterator add_group(const std::string& group_title) {
    auto res = params_.emplace(group_title,
                               std::vector<std::unique_ptr<ConfigParameter>>{});
    return res.first;
  }

  void build_lookup_index() {
    for (const auto& [group_name, param_list] : params_) {
      for (const auto& param : param_list) {
        lookup_index_[param->longname_] = param.get();
        if (param->shortname_.has_value()) {
          lookup_index_[std::string(1, param->shortname_.value())] =
              param.get();
        }
      }
    }
  }

  void validate() {
    for (auto& [group_name, group] : params_) {
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
    std::cout << "Usage:" << "\n";
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
    std::cout << "  <polygon-source>             Path to footprint polygon "
                 "source. "
                 "Can be "
                 "an OGR supported file (eg. GPKG) or database connection "
                 "string.\n";
    std::cout << "  <output-directory>           Output directory.\n";
    std::cout << "\n";
    std::cout << "\033[1mGeneral options:\033[0m\n";
    std::cout << "  -h, --help                   Show this help message.\n";
    std::cout << "  -v, --version                Show version." << "\n";
    std::cout << "  -l, --loglevel <level>       Specify loglevel. Can be "
                 "trace, debug, info, warning [default: info]"
              << "\n";
    std::cout << "  --trace-interval <s>         Trace interval in "
                 "seconds. Implies --loglevel trace [default: 10].\n";
    std::cout << "  -c <file>, --config <file>   TOML configuration file."
              << "\n";
    std::cout << "  -j <n>, --jobs <n>           Number of threads to use. "
                 "[default: number of cores]\n";
    // std::cout << "   --crop-only                  Only crop pointclouds.
    // Skip reconstruction.\n";
    std::cout << "  --no-tiling                  Do not use tiling.\n";
    std::cout << "  --crop-output                Output cropped building "
                 "pointclouds.\n";
    std::cout << "  --crop-output-all            Output files for each "
                 "candidate pointcloud instead of only the optimal candidate. "
                 "Implies --crop-output.\n";
    std::cout << "  --crop-rasters               Output rasterised crop "
                 "pointclouds. Implies --crop-output.\n";
    std::cout << "  --index                      Output index.gpkg file with "
                 "crop analytics.\n";
    std::cout << "  --skip-pc-check              Do not check if pointcloud "
                 "files exist\n";

    for (auto& [group_name, group] : params_) {
      if (group.empty()) continue;
      std::cout << "\n";
      std::cout << "\033[1m" << group_name << ":\033[0m\n";
      for (auto& param : group) {
        auto tdesc = param->type_description();
        auto desc = param->description();
        if (param->longname_.size() + tdesc.size() + 1 <= 33) {
          std::cout << "  --" << std::setw(33) << std::left
                    << (param->longname_ + " " + tdesc) << desc << "\n";
          std::cout << "    " << std::setw(33) << std::left
                    << ""  // 5 characters of padding
                    << "\033[34mDefault: " << param->default_to_string()
                    << "\033[0m" << "\n";
        } else {
          std::cout << "  --" << std::setw(33) << std::left
                    << (param->longname_) << desc << "\n";
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

      if (arg == "-h" || arg == "--help") {
        _print_help = true;
        it = c.args.erase(it);  // Remove the processed argument
      } else if (arg == "-v" || arg == "--version") {
        _print_version = true;
        it = c.args.erase(it);
      } else if (arg == "-l" || arg == "--loglevel") {
        auto next_it = std::next(it);
        if (next_it != c.args.end() && !next_it->starts_with("-")) {
          _loglevel = *next_it;
          if (auto error_msg = check::OneOf<std::string>(
                  {"trace", "debug", "info", "warning"})(_loglevel)) {
            throw std::runtime_error(
                fmt::format("Invalid argument for --loglevel. {}", *error_msg));
          }
          // Erase the option and its argument
          it = c.args.erase(it);  // Erase the option
          it = c.args.erase(it);  // Erase the argument
        } else {
          throw std::runtime_error("Missing argument for --loglevel.");
        }
      } else if (arg == "-j" || arg == "--jobs") {
        auto next_it = std::next(it);
        if (next_it != c.args.end() && !next_it->starts_with("-")) {
          _jobs = std::stoi(*next_it);
          // Erase the option and its argument
          it = c.args.erase(it);
          it = c.args.erase(it);
        } else {
          throw std::runtime_error("Missing argument for --jobs");
        }
      } else if (arg == "-t" || arg == "--trace-interval") {
        auto next_it = std::next(it);
        if (next_it != c.args.end() && !next_it->starts_with("-")) {
          _trace_interval = std::stoi(*next_it);
          _loglevel = "trace";
          // Erase the option and its argument
          it = c.args.erase(it);
          it = c.args.erase(it);
        } else {
          throw std::runtime_error("Missing argument for --trace-interval.");
        }
      } else if (arg == "-c" || arg == "--config") {
        auto next_it = std::next(it);
        if (next_it != c.args.end() && !next_it->starts_with("-")) {
          _config_path = *next_it;
          if (auto error_msg = check::PathExists(_config_path)) {
            throw std::runtime_error(fmt::format(
                "Invalid argument for -c or --config. {}", *error_msg));
          }
          // Erase the option and its argument
          it = c.args.erase(it);
          it = c.args.erase(it);
        } else {
          throw std::runtime_error("Missing argument for --config.");
        }
      } else if (arg == "--no-tiling") {
        _no_tiling = true;
        it = c.args.erase(it);
      } else if (arg == "--skip-pc-check") {
        _skip_pc_check = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-only") {
        _crop_only = true;
        cfg_.write_crop_outputs = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-output") {
        cfg_.write_crop_outputs = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-output-all") {
        cfg_.write_crop_outputs = true;
        cfg_.output_all = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-rasters") {
        cfg_.write_crop_outputs = true;
        cfg_.write_rasters = true;
        it = c.args.erase(it);
      } else if (arg == "--index") {
        cfg_.write_index = true;
        it = c.args.erase(it);
      } else {
        ++it;  // Unrecognized argument; proceed to the next
      }
    }
  }

  void parse_cli_second_pass(CLIArgs& c) {
    auto it = c.args.begin();
    while (it != c.args.end()) {
      std::string arg = *it;

      try {
        if (arg.starts_with("--")) {
          auto argname = arg.substr(2);
          if (auto p = lookup_index_.find(argname); p != lookup_index_.end()) {
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
        } else if (auto p = lookup_index_.find(key.data());
                   p != lookup_index_.end()) {
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
    fmt::format_to(ctx.out(), "RooferConfig(source_footprints={}",
                   cfgh.cfg_.source_footprints);

    // Add all parameters
    for (const auto& [groupname, param_list] : cfgh.params_) {
      for (const auto& param : param_list) {
        fmt::format_to(ctx.out(), ", {}={}", param->longname_,
                       param->to_string());
      }
    }

    return fmt::format_to(ctx.out(), ")");
  }
};
