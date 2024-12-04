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

namespace fs = std::filesystem;

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
  std::vector<roofer::LinearRing> nodata_circles;
  std::vector<roofer::PointCollection> building_clouds;
  std::vector<roofer::ImageMap> building_rasters;
  roofer::vec1f ground_elevations;
  roofer::vec1f roof_elevations;
  roofer::vec1i acquisition_years;

  std::unique_ptr<roofer::misc::RTreeInterface> rtree;
  std::vector<fileExtent> file_extents;
};

struct RooferConfig {
  // footprint source parameters
  std::string source_footprints;
  std::string id_attribute;           // -> attr_building_id
  std::string force_lod11_attribute;  // -> attr_force_blockmodel
  std::string yoc_attribute;          // -> attr_year_of_construction
  std::string layer_name;
  int layer_id = 0;
  std::string attribute_filter;

  // crop parameters
  float ceil_point_density = 20;
  float cellsize = 0.5;
  int lod11_fallback_area = 69000;
  float lod11_fallback_density = 5;
  roofer::arr2f tilesize = {1000, 1000};
  bool clear_if_insufficient = true;

  bool write_crop_outputs = false;
  bool output_all = false;
  bool write_rasters = false;
  bool write_index = false;

  // general parameters
  std::optional<roofer::TBox<double>> region_of_interest;
  std::string srs_override;

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

  // reconstruct
  int lod11_fallback_planes = 100;
  int lod11_fallback_time = 90000;
  roofer::ReconstructionConfig rec;

  // output attribute names
  std::unordered_map<std::string, std::string> n = {
      {"status", "rf_status"},
      {"reconstruction_time", "rf_reconstruction_time"},
      {"val3dity_lod12", "rf_val3dity_lod12"},
      {"val3dity_lod13", "rf_val3dity_lod13"},
      {"val3dity_lod22", "rf_val3dity_lod22"},
      {"is_glass_roof", "rf_is_glass_roof"},
      {"nodata_frac", "rf_nodata_frac"},
      {"nodata_r", "rf_nodata_r"},
      {"pt_density", "rf_pt_density"},
      {"is_mutated", "rf_is_mutated"},
      {"pc_select", "rf_pc_select"},
      {"pc_source", "rf_pc_source"},
      {"pc_year", "rf_pc_year"},
      {"force_lod11", "rf_force_lod11"},
      {"roof_type", "rf_roof_type"},
      {"h_roof_50p", "rf_h_roof_50p"},
      {"h_roof_70p", "rf_h_roof_70p"},
      {"h_roof_min", "rf_h_roof_min"},
      {"h_roof_max", "rf_h_roof_max"},
      {"roof_n_planes", "rf_roof_n_planes"},
      {"rmse_lod12", "rf_rmse_lod12"},
      {"rmse_lod13", "rf_rmse_lod13"},
      {"rmse_lod22", "rf_rmse_lod22"},
      {"volume_lod12", "rf_volume_lod12"},
      {"volume_lod13", "rf_volume_lod13"},
      {"volume_lod22", "rf_volume_lod22"},
      {"h_ground", "rf_h_ground"},
      {"slope", "rf_slope"},
      {"azimuth", "rf_azimuth"},
      {"extrusion_mode", "rf_extrusion_mode"},
  };
};

template <>
struct fmt::formatter<roofer::ReconstructionConfig> {
  static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context>
  constexpr auto format(roofer::ReconstructionConfig const& cfg,
                        Context& ctx) const {
    return fmt::format_to(
        ctx.out(),
        "ReconstructionConfig(complexity_factor={}, clip_ground={}, lod={}, "
        "lod13_step_height={}, floor_elevation={}, "
        "override_with_floor_elevation={}, plane_detect_k={}, "
        "plane_detect_min_points={}, plane_detect_epsilon={}, "
        "plane_detect_normal_angle={}, line_detect_epsilon={}, thres_alpha={}, "
        "thres_reg_line_dist={}, thres_reg_line_ext={})",
        cfg.complexity_factor, cfg.clip_ground, cfg.lod, cfg.lod13_step_height,
        cfg.floor_elevation, cfg.override_with_floor_elevation,
        cfg.plane_detect_k, cfg.plane_detect_min_points,
        cfg.plane_detect_epsilon, cfg.plane_detect_normal_angle,
        cfg.line_detect_epsilon, cfg.thres_alpha, cfg.thres_reg_line_dist,
        cfg.thres_reg_line_ext);
  }
};

template <>
struct fmt::formatter<RooferConfig> {
  static constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context>
  constexpr auto format(RooferConfig const& cfg, Context& ctx) const {
    std::string region_of_interest = "novalue";
    if (cfg.region_of_interest.has_value()) {
      region_of_interest = cfg.region_of_interest.value().wkt();
    }
    return fmt::format_to(
        ctx.out(),
        "RooferConfig(source_footprints={}, id_attribute={}, "
        "force_lod11_attribute={}, yoc_attribute={}, layer_name={}, "
        "layer_id={}, attribute_filter={}, ceil_point_density={}, cellsize={}, "
        "lod11_fallback_area={}, lod11_fallback_density={}, tilesize={}, "
        "clear_if_insufficient={}, write_crop_outputs={}, output_all={}, "
        "write_rasters={}, write_index={}, region_of_interest={}, "
        "srs_override={}, split_cjseq={}, building_toml_file_spec={}, "
        "building_las_file_spec={}, building_gpkg_file_spec={}, "
        "building_raster_file_spec={}, building_jsonl_file_spec={}, "
        "jsonl_list_file_spec={}, index_file_spec={}, "
        "metadata_json_file_spec={}, output_path={}, rec={})",
        cfg.source_footprints, cfg.id_attribute, cfg.force_lod11_attribute,
        cfg.yoc_attribute, cfg.layer_name, cfg.layer_id, cfg.attribute_filter,
        cfg.ceil_point_density, cfg.cellsize, cfg.lod11_fallback_area,
        cfg.lod11_fallback_density, cfg.tilesize, cfg.clear_if_insufficient,
        cfg.write_crop_outputs, cfg.output_all, cfg.write_rasters,
        cfg.write_index, region_of_interest, cfg.srs_override, cfg.split_cjseq,
        cfg.building_toml_file_spec, cfg.building_las_file_spec,
        cfg.building_gpkg_file_spec, cfg.building_raster_file_spec,
        cfg.building_jsonl_file_spec, cfg.jsonl_list_file_spec,
        cfg.index_file_spec, cfg.metadata_json_file_spec, cfg.output_path,
        cfg.rec);
  }
};

template <typename T>
using Validator = std::function<std::optional<std::string>(const T&)>;

namespace roofer::v {
  // Concept to ensure types are comparable
  template <typename T>
  concept Comparable = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
    { a > b } -> std::convertible_to<bool>;
  };

  // Generator function for range validators
  template <typename T>
    requires Comparable<T>
  auto InRange(T min, T max) {
    return [min, max](const T& val) -> std::optional<std::string> {
      if (val < min || val > max) {
        return fmt::format("Value {} is out of range <{}, {}>.", val, min, max);
      }
      return std::nullopt;
    };
  };

  // Generator function for validator to check if a value is higher than a given
  // value
  template <typename T>
    requires Comparable<T>
  auto HigherThan(T min) {
    return [min](const T& val) -> std::optional<std::string> {
      if constexpr (std::is_same_v<T, roofer::arr2f>) {
        if (val[0] <= min[0] || val[1] <= min[1]) {
          return fmt::format(
              "One of the values of [{}, {}] is too low. Values must be higher "
              "than {} and {} respectively.",
              val[0], val[1], min[0], min[1]);
        }
      } else if (val <= min) {
        return fmt::format("Value must be higher than {}.", min);
      }
      return std::nullopt;
    };
  };

  // Generator function for validator to check if a value is higher than or
  // equal to a given value
  template <typename T>
    requires Comparable<T>
  auto HigherOrEqualTo(T min) {
    return [min](const T& val) -> std::optional<std::string> {
      if (val >= min) {
        return fmt::format("Value must be higher than or equal to {}.", min);
      }
      return std::nullopt;
    };
  };

  // Generator function for validator to check if the value is one of the given
  // values
  template <typename T>
  auto OneOf(std::vector<T> values) {
    return [values](const T& val) -> std::optional<std::string> {
      if (std::find(values.begin(), values.end(), val) == values.end()) {
        return fmt::format("Value {} is not one of the allowed values.", val);
      }
      return std::nullopt;
    };
  };

  // Box validator
  auto ValidBox =
      [](const roofer::TBox<double>& box) -> std::optional<std::string> {
    if (box.pmin[0] >= box.pmax[0] || box.pmin[1] >= box.pmax[1]) {
      return "Box is invalid.";
    }
    return std::nullopt;
  };

  // Path exists validator
  auto PathExists = [](const std::string& path) -> std::optional<std::string> {
    if (!std::filesystem::exists(path)) {
      return fmt::format("Path {} does not exist.", path);
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
      return fmt::format("Path {} is not a directory.", parent.string());
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
    return fmt::format("Could not write to directory {}.", parent.string());
    ;
  };
}  // namespace roofer::v

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

class ConfigParameter {
 public:
  std::string _help;
  ConfigParameter(std::string help) : _help(help){};
  virtual ~ConfigParameter() = default;

  virtual std::optional<std::string> validate() = 0;

  virtual std::list<std::string>::iterator set(
      std::list<std::string>& args, std::list<std::string>::iterator it) = 0;

  virtual void set_from_toml(const toml::table& table,
                             const std::string& name) = 0;

  std::string description() { return _help; }
  virtual std::string type_description() = 0;
  virtual std::string get_as_string() = 0;
};

// template <typename T>
// class ConfigParameterByValue : public ConfigParameter {
//     T _value;
//     std::vector<Validator<T>> _validators;

//   public:
//     ConfigParameterByValue(std::string help, T value,
//       std::vector<Validator<T>> validators) : ConfigParameter(help),
//       _value(value), _validators(validators) {
//     };

// };

template <typename T>
class ConfigParameterByReference : public ConfigParameter {
  T& _value;
  std::vector<Validator<T>> _validators;

 public:
  ConfigParameterByReference(std::string help, T& value,
                             std::vector<Validator<T>> validators)
      : ConfigParameter(help), _value(value), _validators(validators){};

  std::optional<std::string> validate() override {
    for (auto& validator : _validators) {
      ;
      if (auto error_msg = validator(_value)) {
        return error_msg;
      }
    }
    return std::nullopt;
  }

  std::string get_as_string() override {
    if constexpr (std::is_same_v<T, std::optional<roofer::TBox<double>>>) {
      if (!_value.has_value()) {
        return "[]";
      } else {
        return fmt::format("[{},{},{},{}]", _value->pmin[0], _value->pmin[1],
                           _value->pmax[0], _value->pmax[1]);
      }
    } else if constexpr (std::is_same_v<T, roofer::arr2f>) {
      return fmt::format("[{},{}]", _value[0], _value[1]);
    } else if constexpr (std::is_same_v<T, std::optional<roofer::arr3d>>) {
      if (!_value.has_value()) {
        return "[]";
      } else {
        return fmt::format("[{},{},{}]", (*_value)[0], (*_value)[1],
                           (*_value)[2]);
      }
    } else {
      return fmt::format("{}", _value);
    }
  }

  std::list<std::string>::iterator set(
      std::list<std::string>& args,
      std::list<std::string>::iterator it) override {
    if constexpr (std::is_same_v<T, bool>) {
      _value = !(_value);
      return it;
    } else if (it == args.end()) {
      throw std::runtime_error("Missing argument for parameter");
    } else if constexpr (std::is_same_v<T, int> ||
                         std::is_same_v<T, std::optional<int>>) {
      _value = std::stoi(*it);
      return args.erase(it);
    } else if constexpr (std::is_same_v<T, float> ||
                         std::is_same_v<T, std::optional<float>>) {
      _value = std::stof(*it);
      return args.erase(it);
    } else if constexpr (std::is_same_v<T, double> ||
                         std::is_same_v<T, std::optional<double>>) {
      _value = std::stod(*it);
      return args.erase(it);
    } else if constexpr (std::is_same_v<T, std::string>) {
      _value = *it;
      return args.erase(it);
      ;
    } else if constexpr (std::is_same_v<T,
                                        std::optional<roofer::TBox<double>>>) {
      roofer::TBox<double> box;
      // Check if there are enough arguments
      if (std::distance(it, args.end()) < 4) {
        throw std::runtime_error("Not enough arguments, need 4.");
      }

      // Extract the values safely
      box.pmin[0] = std::stod(*it);
      it = args.erase(it);
      box.pmin[1] = std::stod(*it);
      it = args.erase(it);
      box.pmax[0] = std::stod(*it);
      it = args.erase(it);
      box.pmax[1] = std::stod(*it);
      it = args.erase(it);
      _value = box;
      return it;
    } else if constexpr (std::is_same_v<T, roofer::arr2f>) {
      roofer::arr2f arr;
      // Check if there are enough arguments
      if (std::distance(it, args.end()) < 2) {
        throw std::runtime_error("Not enough arguments, need 2.");
      }
      arr[0] = std::stof(*it);
      it = args.erase(it);
      arr[1] = std::stof(*it);
      it = args.erase(it);
      _value = arr;
      return it;
    } else if constexpr (std::is_same_v<T, std::optional<roofer::arr3d>>) {
      roofer::arr3d arr;
      // Check if there are enough arguments
      if (std::distance(it, args.end()) < 3) {
        throw std::runtime_error("Not enough arguments, need 3.");
      }
      arr[0] = std::stod(*it);
      it = args.erase(it);
      arr[1] = std::stod(*it);
      it = args.erase(it);
      arr[2] = std::stod(*it);
      it = args.erase(it);
      _value = arr;
      return it;
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "Unsupported type for ConfigParameterByReference::set()");
    }
  }

  void set_from_toml(const toml::table& table,
                     const std::string& name) override {
    if constexpr (std::is_same_v<T, std::array<float, 2>>) {
      if (const toml::array* a = table[name].as_array()) {
        if (a->size() == 2 &&
            (a->is_homogeneous(toml::node_type::floating_point) ||
             a->is_homogeneous(toml::node_type::integer))) {
          _value = roofer::arr2f{*a->get(0)->value<float>(),
                                 *a->get(1)->value<float>()};
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T,
                                        std::optional<std::array<double, 3>>>) {
      if (const toml::array* a = table[name].as_array()) {
        if (a->size() == 3 &&
            (a->is_homogeneous(toml::node_type::floating_point) ||
             a->is_homogeneous(toml::node_type::integer))) {
          _value = roofer::arr3d{*a->get(0)->value<double>(),
                                 *a->get(1)->value<double>(),
                                 *a->get(2)->value<double>()};
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T,
                                        std::optional<roofer::TBox<double>>>) {
      if (const toml::array* a = table[name].as_array()) {
        if (a->size() == 4 &&
            (a->is_homogeneous(toml::node_type::floating_point) ||
             a->is_homogeneous(toml::node_type::integer))) {
          _value = roofer::TBox<double>{
              *a->get(0)->value<double>(), *a->get(1)->value<double>(), 0,
              *a->get(2)->value<double>(), *a->get(3)->value<double>(), 0};
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else {
      if (auto value = table[name].value<T>(); value.has_value()) {
        _value = *value;
      }
    }
  }

  std::string type_description() override {
    if constexpr (std::is_same_v<T, bool>) {
      return "";
    } else if constexpr (std::is_same_v<T, int>) {
      return "<int>";
    } else if constexpr (std::is_same_v<T, float>) {
      return "<float>";
    } else if constexpr (std::is_same_v<T, double>) {
      return "<double>";
    } else if constexpr (std::is_same_v<T, std::string>) {
      return "<str>";
    } else if constexpr (std::is_same_v<T,
                                        std::optional<roofer::TBox<double>>>) {
      return "<xmin ymin xmax ymax>";
    } else if constexpr (std::is_same_v<T, roofer::arr2f>) {
      return "<x y>";
    } else if constexpr (std::is_same_v<T, std::optional<roofer::arr3d>>) {
      return "<x y z>";
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "Unsupported type for "
                    "ConfigParameterByReference::type_description()");
    }
  }
};

struct CLIArgs {
  std::string program_name;
  std::list<std::string> args;

  CLIArgs(int argc, const char* argv[]) {
    program_name = argv[0];
    for (int i = 1; i < argc; i++) {
      args.push_back(argv[i]);
    }
  }
};

struct RooferConfigHandler {
  // parametermap
  RooferConfig& _cfg;
  std::vector<InputPointcloud>& _input_pointclouds;
  std::unordered_map<std::string, std::unique_ptr<ConfigParameter>> _pmap;
  std::unordered_map<std::string, std::unique_ptr<ConfigParameter>> _rmap;

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
  RooferConfigHandler(RooferConfig& cfg,
                      std::vector<InputPointcloud>& input_pointclouds)
      : _cfg(cfg), _input_pointclouds(input_pointclouds) {
    // TODO: indicate per parameter if it is:
    // - [experimental]: do not advertise in help
    // - [positional]: positional argument in cli

    add("id-attribute", "Building ID attribute", _cfg.id_attribute, {});
    add("force-lod11-attribute", "Building attribute for forcing lod11",
        _cfg.force_lod11_attribute, {});
    add("yoc-attribute", "Attribute containing building year of construction",
        _cfg.yoc_attribute, {});
    add("polygon-source-layer",
        "Load this layer from <polygon-source> [default: first layer]",
        _cfg.layer_name, {});
    // add("layer_id", "id of layer", _cfg.layer_id,
    // {roofer::v::HigherOrEqualTo<int>(0)}});
    add("filter",
        "Specify WHERE clause in OGR SQL to select specfic features from "
        "<polygon-source>",
        _cfg.attribute_filter, {});
    add("ceil-point-density",
        "Enfore this point density ceiling on each building pointcloud.",
        _cfg.ceil_point_density, {roofer::v::HigherThan<float>(0)});
    add("cellsize", "Cellsize used for quick pointcloud analysis",
        _cfg.cellsize, {roofer::v::HigherThan<float>(0)});
    add("lod11-fallback-area", "lod11 fallback area", _cfg.lod11_fallback_area,
        {roofer::v::HigherThan<int>(0)});
    add("reconstruct-insufficient",
        "reconstruct buildings with insufficient pointcloud data",
        _cfg.clear_if_insufficient, {});
    // add("lod11-fallback-density", "lod11 fallback density",
    // _cfg.lod11_fallback_density, {roofer::v::HigherThan<float>(0)}});
    add("tilesize", "Tilesize used for output tiles", _cfg.tilesize,
        {roofer::v::HigherThan<roofer::arr2f>({0, 0})});
    add("box",
        "Region of interest. Data outside of this region will be ignored",
        _cfg.region_of_interest,
        {[](const std::optional<roofer::TBox<double>>& box)
             -> std::optional<std::string> {
          if (box.has_value()) {
            auto error_msg = roofer::v::ValidBox(*box);
            if (error_msg) {
              return error_msg;
            }
          }
          return std::nullopt;
        }});

    addr("lod",
         "Which LoDs to generate, possible values: 12, 13, 22 [default: all]",
         _cfg.rec.lod, {roofer::v::OneOf<int>({0, 12, 13, 22})});
    addr("complexity-factor", "Complexity factor building reconstruction",
         _cfg.rec.complexity_factor, {roofer::v::InRange<float>(0, 1)});
    // addr("clip-ground", "clip ground", _cfg.rec.clip_ground, {});
    addr("lod13-step-height", "Step height used for LoD1.3 generation",
         _cfg.rec.lod13_step_height, {roofer::v::HigherThan<float>(0)});
    add("srs", "Override SRS for both inputs and outputs", _cfg.srs_override,
        {});
    add("split-cjseq",
        "Output CityJSONSequence file for each building [default: one file per "
        "output tile]",
        _cfg.split_cjseq, {});
    add("omit-metadata", "Omit metadata in CityJSON output", _cfg.omit_metadata,
        {});
    add("cj-scale", "Scaling applied to CityJSON output vertices",
        _cfg.cj_scale, {});
    add("cj-translate", "Translation applied to CityJSON output vertices",
        _cfg.cj_translate, {});
    add("lod11-fallback-planes",
        "Fallback to LoD11 if number of detected planes exceeds this value.",
        _cfg.lod11_fallback_planes, {roofer::v::HigherThan<int>(0)});
    add("lod11-fallback-time",
        "Fallback to LoD11 if time spent on detecting planes exceeds this "
        "value. In milliseconds.",
        _cfg.lod11_fallback_time, {roofer::v::HigherThan<int>(0)});
    addr("plane-detect-k", "plane detect k", _cfg.rec.plane_detect_k,
         {roofer::v::HigherThan<int>(0)});
    addr("plane-detect-min-points", "plane detect min points",
         _cfg.rec.plane_detect_min_points, {roofer::v::HigherThan<int>(2)});
    addr("plane-detect-epsilon", "plane detect epsilon",
         _cfg.rec.plane_detect_epsilon, {roofer::v::HigherThan<float>(0)});
  };

  template <typename T>
  void add(std::string name, std::string help, T& value,
           std::vector<Validator<T>> validators) {
    _pmap[name] = std::make_unique<ConfigParameterByReference<T>>(help, value,
                                                                  validators);
  }
  template <typename T>
  void addr(std::string name, std::string help, T& value,
            std::vector<Validator<T>> validators) {
    _rmap[name] = std::make_unique<ConfigParameterByReference<T>>(help, value,
                                                                  validators);
  }

  void validate() {
    for (auto& [name, param] : _pmap) {
      if (auto error_msg = param->validate()) {
        throw std::runtime_error(fmt::format(
            "Validation error for parameter {}. {}", name, *error_msg));
      }
    }
    for (auto& [name, param] : _rmap) {
      if (auto error_msg = param->validate()) {
        throw std::runtime_error(
            fmt::format("Validation error for reconstruction parameter {}. {}",
                        name, *error_msg));
      }
    }

    if (_input_pointclouds.empty()) {
      throw std::runtime_error("No input pointclouds specified.");
    }
    if (!_skip_pc_check) {
      for (auto& ipc : _input_pointclouds) {
        if (ipc.paths.empty()) {
          throw std::runtime_error(
              "No files found for one of the input pointclouds.");
        }
      }
    }
    // if (auto error_msg = roofer::v::PathExists(_cfg.source_footprints)) {
    //   throw std::runtime_error(fmt::format(
    //       "Footprint source does not exist: {}.", _cfg.source_footprints));
    // }
    if (auto error_msg = roofer::v::DirIsWritable(_cfg.output_path)) {
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
    std::cout << "   " << program_name;
    std::cout << " [options] <pointcloud-path>... <polygon-source> "
                 "<output-directory>"
              << "\n";
    std::cout << "   " << program_name;
    std::cout
        << " [options] (-c | --config) <config-file> [(<pointcloud-path>... "
           "<polygon-source>)] <output-directory>"
        << "\n";
    std::cout << "   " << program_name;
    std::cout << " -h | --help" << "\n";
    std::cout << "   " << program_name;
    std::cout << " -v | --version" << "\n";
    std::cout << "\n";
    std::cout << "Positional arguments:" << "\n";
    std::cout << "   <pointcloud-path>            Path to pointcloud file "
                 "(.LAS or .LAZ) or folder that contains pointcloud files.\n";
    std::cout
        << "   <polygon-source>           Path to footprint polygon source. "
           "Can be "
           "an OGR supported file (eg. GPKG) or database connection string.\n";
    std::cout << "   <output-directory>           Output directory.\n";
    std::cout << "\n";
    std::cout << "Optional arguments:\n";
    std::cout << "   -h, --help                   Show this help message.\n";
    std::cout << "   -v, --version                Show version." << "\n";
    std::cout << "   -l, --loglevel <level>       Specify loglevel. Can be "
                 "trace, debug, info, warning [default: info]"
              << "\n";
    std::cout << "   --trace-interval <s>         Trace interval in "
                 "seconds. Implies --loglevel trace [default: 10].\n";
    std::cout << "   -c <file>, --config <file>   TOML configuration file."
              << "\n";
    std::cout << "   -j <n>, --jobs <n>           Number of threads to use. "
                 "[default: number of cores]\n";
    // std::cout << "   --crop-only                  Only crop pointclouds. Skip
    // reconstruction.\n";
    std::cout << "   --crop-output                Output cropped building "
                 "pointclouds.\n";
    std::cout << "   --crop-output-all            Output files for each "
                 "candidate pointcloud instead of only the optimal candidate. "
                 "Implies --crop-output.\n";
    std::cout << "   --crop-rasters               Output rasterised crop "
                 "pointclouds. Implies --crop-output.\n";
    std::cout << "   --index                      Output index.gpkg file with "
                 "crop analytics.\n";
    std::cout << "   --skip-pc-check              Do not check if pointcloud "
                 "files exist\n";
    std::cout << "\n";
    for (auto& [name, param] : _pmap) {
      std::cout << "   --" << std::setw(33) << std::left
                << (name + " " + param->type_description())
                << param->description() << "\n";
    }
    std::cout << "\n";
    for (auto& [name, param] : _rmap) {
      std::cout << "   -R" << std::setw(33) << std::left
                << (name + " " + param->type_description())
                << param->description() << "\n";
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
          if (auto error_msg = roofer::v::OneOf<std::string>(
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
          if (auto error_msg = roofer::v::PathExists(_config_path)) {
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
        _cfg.write_crop_outputs = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-output") {
        _cfg.write_crop_outputs = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-output-all") {
        _cfg.write_crop_outputs = true;
        _cfg.output_all = true;
        it = c.args.erase(it);
      } else if (arg == "--crop-rasters") {
        _cfg.write_crop_outputs = true;
        _cfg.write_rasters = true;
        it = c.args.erase(it);
      } else if (arg == "--index") {
        _cfg.write_index = true;
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
          if (auto p = _pmap.find(argname); p != _pmap.end()) {
            it = c.args.erase(it);
            it = p->second->set(c.args, it);
          } else {
            throw std::runtime_error(fmt::format("Unknown argument: {}.", arg));
          }

        } else if (arg.starts_with("-R")) {
          auto argname = arg.substr(2);
          if (auto p = _rmap.find(argname); p != _rmap.end()) {
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
    bool fp_set = _cfg.source_footprints.size() > 0;
    bool pc_set = _input_pointclouds.size() > 0;
    bool output_set = _cfg.output_path.size() > 0;

    if (pc_set && fp_set && output_set && c.args.size() == 0) {
      // all set
    } else if (pc_set && fp_set && c.args.size() == 1) {
      _cfg.output_path = c.args.back();
    } else if (c.args.size() > 2) {
      _cfg.output_path = c.args.back();
      c.args.pop_back();
      _cfg.source_footprints = c.args.back();
      c.args.pop_back();

      _input_pointclouds.clear();
      _input_pointclouds.emplace_back(InputPointcloud{
          .paths = find_filepaths(c.args, {".las", ".LAS", ".laz", ".LAZ"},
                                  _skip_pc_check)});
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
          get_toml_value(config, "polygon-source", _cfg.source_footprints);
        } else if (key == "output-directory") {
          get_toml_value(config, "output-directory", _cfg.output_path);
        } else if (auto p = _pmap.find(key.data()); p != _pmap.end()) {
          p->second->set_from_toml(config, key.data());
        } else if (auto p = _rmap.find(key.data()); p != _rmap.end()) {
          p->second->set_from_toml(config, key.data());
        } else if (key == "pointclouds") {
          if (toml::array* arr = config["pointclouds"].as_array()) {
            // visitation with for_each() helps deal with heterogeneous data
            for (auto& el : *arr) {
              toml::table* tb = el.as_table();
              auto& pc = _input_pointclouds.emplace_back();

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
                        "Failed to read pointclouds.source. Make sure it is a "
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
        } else if (key == "output-attributes") {
          if (toml::table* tb = config["output-attributes"].as_table()) {
            for (const auto& [key, value] : *tb) {
              if (auto p = _cfg.n.find(key.data()); p != _cfg.n.end()) {
                std::string name = "";
                get_toml_value(*tb, key.data(), name);
                if (!name.empty()) {
                  p->second = name;
                }
              } else {
                throw std::runtime_error(
                    fmt::format("Unknown output attribute: {}.", key.data()));
              }
            }
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

    // parameters
    // get_toml_value(config["crop"], "ceil_point_density",
    // _cfg.ceil_point_density);

    // reconstruction parameters
    // get_toml_value(config["reconstruct"], "complexity_factor",
    //           _cfg.rec.complexity_factor);
    // get_toml_value(config["reconstruct"], "clip_ground",
    // _cfg.rec.clip_ground); get_toml_value(config["reconstruct"], "lod",
    // _cfg.rec.lod); get_toml_value(config["reconstruct"], "lod13_step_height",
    //           _cfg.rec.lod13_step_height);
    // get_toml_value(config["reconstruct"], "plane_detect_k",
    // _cfg.rec.plane_detect_k); get_toml_value(config["reconstruct"],
    // "plane_detect_min_points",
    //           _cfg.rec.plane_detect_min_points);
    // get_toml_value(config["reconstruct"], "plane_detect_epsilon",
    //           _cfg.rec.plane_detect_epsilon);
    // get_toml_value(config["reconstruct"], "plane_detect_normal_angle",
    //           _cfg.rec.plane_detect_normal_angle);
    // get_toml_value(config["reconstruct"], "line_detect_epsilon",
    //           _cfg.rec.line_detect_epsilon);
    // get_toml_value(config["reconstruct"], "thres_alpha",
    // _cfg.rec.thres_alpha); get_toml_value(config["reconstruct"],
    // "thres_reg_line_dist",
    //           _cfg.rec.thres_reg_line_dist);
    // get_toml_value(config["reconstruct"], "thres_reg_line_ext",
    //           _cfg.rec.thres_reg_line_ext);
  }
};
