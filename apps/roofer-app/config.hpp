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

#include <functional>
#include <concepts>
#include "toml.hpp"

#include <roofer/ReconstructionConfig.hpp>
#include <stdexcept>
#include <string>

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

  bool write_crop_outputs = false;
  bool output_all = false;
  bool write_rasters = false;
  bool write_index = false;

  // general parameters
  std::optional<roofer::TBox<double>> region_of_interest;
  std::string srs_override;

  // crop output
  bool split_cjseq = false;
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
  roofer::ReconstructionConfig rec;
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
        return fmt::format("Value must be higher than or equal to {}", min);
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
        return fmt::format("Value {} is not one of the allowed values", val);
      }
      return std::nullopt;
    };
  };

  // Box validator
  auto ValidBox =
      [](const roofer::TBox<double>& box) -> std::optional<std::string> {
    if (box.pmin[0] >= box.pmax[0] || box.pmin[1] >= box.pmax[1]) {
      return "Box is invalid";
    }
    return std::nullopt;
  };

  // Path exists validator
  auto PathExists = [](const std::string& path) -> std::optional<std::string> {
    if (!std::filesystem::exists(path)) {
      return fmt::format("Path {} does not exist", path);
    }
    return std::nullopt;
  };

  // Helper function to check if a directory is writable
  bool isDirectoryWritable(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
      return false;
    }

    // Try to create a temporary file in the directory
    auto testPath = path / "write_test_tmp";
    try {
      std::ofstream test_file(testPath);
      if (test_file) {
        test_file.close();
        std::filesystem::remove(testPath);
        return true;
      }
    } catch (...) {
      if (std::filesystem::exists(testPath)) {
        std::filesystem::remove(testPath);
      }
    }
    return false;
  }

  // Create a validator for file path writeability
  auto PathIsWritable =
      [](const std::string& path) -> std::optional<std::string> {
    try {
      std::filesystem::path fs_path(path);

      // Check if path is empty
      if (path.empty()) {
        return "Path cannot be empty";
      }

      // Get the parent directory
      auto parent_path = fs_path.parent_path();
      if (parent_path.empty()) {
        parent_path = std::filesystem::current_path();
      }

      // If file exists, check if it's writable
      if (std::filesystem::exists(fs_path)) {
        std::ofstream test_file(fs_path, std::ios::app);
        if (!test_file) {
          return fmt::format("Existing file '{}' is not writable", path);
        }
      }
      // If file doesn't exist, check if we can create it
      else {
        // Check if parent directory exists and is writable
        if (!std::filesystem::exists(parent_path)) {
          return fmt::format("Directory '{}' does not exist",
                             parent_path.string());
        }

        if (!isDirectoryWritable(parent_path)) {
          return fmt::format("Directory '{}' is not writable",
                             parent_path.string());
        }
      }

      return std::nullopt;

    } catch (const std::filesystem::filesystem_error& e) {
      return fmt::format("Filesystem error: {}", e.what());
    } catch (const std::exception& e) {
      return fmt::format("Error checking path: {}", e.what());
    }
  };
}  // namespace roofer::v

std::vector<std::string> find_filepaths(
    const std::list<std::string>& filepath_parts,
    std::initializer_list<std::string> extensions) {
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
      } else {
        throw std::runtime_error("File not found: " + filepath_part);
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
  std::string description() { return _help; }
  virtual std::string type_description() = 0;
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

  std::list<std::string>::iterator set(
      std::list<std::string>& args,
      std::list<std::string>::iterator it) override {
    if (it == args.end()) {
      throw std::runtime_error("Missing argument for parameter");
    } else if constexpr (std::is_same_v<T, bool>) {
      _value = !(_value);
      return it;
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
        throw std::runtime_error("Not enough arguments, need 4");
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
        throw std::runtime_error("Not enough arguments, need 2");
      }
      arr[0] = std::stof(*it);
      it = args.erase(it);
      arr[1] = std::stof(*it);
      it = args.erase(it);
      _value = arr;
      return it;
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "Unsupported type for ConfigParameterByReference::set()");
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
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "Unsupported type for ConfigParameterByReference::set()");
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
  std::string _loglevel = "info";
  size_t _trace_interval = 10;
  std::string _config_path;
  size_t _jobs = std::thread::hardware_concurrency();

  // methods
  RooferConfigHandler(RooferConfig& cfg,
                      std::vector<InputPointcloud>& input_pointclouds)
      : _cfg(cfg), _input_pointclouds(input_pointclouds) {
    // {"config_path", ConfigParameterByReference{"path to the config file",
    //                  _config_path,
    //                  {roofer::v::PathExists}},
    // add("source-footprints", "source of the footprints",
    // _cfg.source_footprints,
    // {});
    add("id-attribute", "Building ID attribute", _cfg.id_attribute, {});
    add("force-lod11-attribute", "Building attribute for forcing lod11",
        _cfg.force_lod11_attribute, {});
    // add("yoc-attribute", "Attribute containing building year of
    // construction",
    //     _cfg.yoc_attribute, {});
    add("layer",
        "Load this layer from <footprint-source> [default: first layer]",
        _cfg.layer_name, {});
    // add("layer_id", "id of layer", _cfg.layer_id,
    // {roofer::v::HigherOrEqualTo<int>(0)}});
    add("filter", "Attribute filter", _cfg.attribute_filter, {});
    // add("ceil_point_density", "ceiling point density",
    // _cfg.ceil_point_density, {roofer::v::HigherThan<float>(0)}});
    add("cellsize", "Cellsize used for quick pointcloud analysis",
        _cfg.cellsize, {roofer::v::HigherThan<float>(0)});
    // add("lod11_fallback_area", "lod11 fallback area",
    // _cfg.lod11_fallback_area, {roofer::v::HigherThan<int>(0)}});
    // add("lod11_fallback_density", "lod11 fallback density",
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
    // addr("plane-detect_k", "plane detect k", _cfg.rec.plane_detect_k,
    //      {roofer::v::HigherThan<int>(0)});
    // addr("plane-detect-min-points", "plane detect min points",
    //      _cfg.rec.plane_detect_min_points, {roofer::v::HigherThan<int>(2)});
    // addr("plane-detect-epsilon", "plane detect epsilon",
    //      _cfg.rec.plane_detect_epsilon, {roofer::v::HigherThan<float>(0)});
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
    if (auto error_msg = roofer::v::PathIsWritable(_cfg.output_path)) {
      throw std::runtime_error(
          fmt::format("Check output path. {}", *error_msg));
    }
  }

  template <typename T, typename node>
  void get_param(const node& config, const std::string& key, T& result) {
    if (auto tml_value = config[key].template value<T>();
        tml_value.has_value()) {
      result = *tml_value;
    }
  }

  void print_help(std::string program_name) {
    // see http://docopt.org/
    std::cout << "Usage:" << "\n";
    std::cout << "   " << program_name;
    std::cout << " [options] <pointcloud-path>... <footprint-source> "
                 "<output-directory>"
              << "\n";
    std::cout << "   " << program_name;
    std::cout
        << " [options] (-c | --config) <config-file> [(<pointcloud-path>... "
           "<footprint-source>)] <output-directory>"
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
        << "   <footprint-source>           Path to footprint source. Can be "
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
    std::cout << "\n";
    for (auto& [name, param] : _pmap) {
      std::cout << "   --" << std::setw(33) << std::left
                << (name + " " + param->type_description())
                << param->description() << "\n";
    }
    std::cout << "\n";
    for (auto& [name, param] : _rmap) {
      std::cout << "   --" << std::setw(33) << std::left
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
          throw std::runtime_error("Missing argument for --loglevel");
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
          throw std::runtime_error("Missing argument for --trace-interval");
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
          throw std::runtime_error("Missing argument for --config");
        }
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
            throw std::runtime_error(fmt::format("Unknown argument: {}", arg));
          }

        } else if (arg.starts_with("-R")) {
          auto argname = arg.substr(2);
          if (auto p = _rmap.find(argname); p != _rmap.end()) {
            it = c.args.erase(it);
            it = p->second->set(c.args, it);
          } else {
            throw std::runtime_error(fmt::format("Unknown argument: {}", arg));
          }
        } else {
          ++it;
        }
      } catch (const std::exception& e) {
        throw std::runtime_error(
            fmt::format("Error parsing argument: {}. {}", arg, e.what()));
      }
    }

    // now c.arg shoulg only contain positional arguments
    // we assume either only the output path given on CLI or also fp and pc
    // inputs
    bool fp_set = _cfg.source_footprints.size() > 0;
    bool pc_set = _input_pointclouds.size() > 0;

    if (pc_set && fp_set && c.args.size() == 1) {
      _cfg.output_path = c.args.back();
    } else if (c.args.size() > 2) {
      _cfg.output_path = c.args.back();
      c.args.pop_back();
      _cfg.source_footprints = c.args.back();
      c.args.pop_back();

      _input_pointclouds.clear();
      _input_pointclouds.emplace_back(InputPointcloud{
          .paths = find_filepaths(c.args, {".las", ".LAS", ".laz", ".LAZ"})});
    } else {
      throw std::runtime_error(
          "Unable set all inputs and output. Need to provide at least <ouput "
          "path> and set input paths in config file or provide all of <point "
          "cloud sources> <footprint source> <ouput path>.");
    }
  };

  void parse_config_file() {
    auto& logger = roofer::logger::Logger::get_logger();
    toml::table config;
    config = toml::parse_file(_config_path);

    get_param(config["input"]["footprint"], "source", _cfg.source_footprints);
    get_param(config["input"]["footprint"], "layer_name", _cfg.layer_name);
    get_param(config["input"]["footprint"], "layer_id", _cfg.layer_id);
    get_param(config["input"]["footprint"], "attribute_filter",
              _cfg.attribute_filter);
    get_param(config["input"]["footprint"], "id_attribute", _cfg.id_attribute);
    get_param(config["input"]["footprint"], "force_lod11_attribute",
              _cfg.force_lod11_attribute);
    get_param(config["input"]["footprint"], "year_of_construction_attribute",
              _cfg.yoc_attribute);

    auto tml_pointclouds = config["input"]["pointclouds"];
    if (toml::array* arr = tml_pointclouds.as_array()) {
      // visitation with for_each() helps deal with heterogeneous data
      for (auto& el : *arr) {
        toml::table* tb = el.as_table();
        auto& pc = _input_pointclouds.emplace_back();

        get_param(*tb, "name", pc.name);
        get_param(*tb, "quality", pc.quality);
        get_param(*tb, "date", pc.date);
        get_param(*tb, "force_lod11", pc.force_lod11);
        get_param(*tb, "select_only_for_date", pc.select_only_for_date);
        get_param(*tb, "building_class", pc.bld_class);
        get_param(*tb, "ground_class", pc.grnd_class);
        // get_param(*tb, "source", pc.path);
        std::list<std::string> input_paths;
        if (toml::array* pc_paths = tb->at("source").as_array()) {
          for (auto& pc_path : *pc_paths) {
            input_paths.push_back(*pc_path.value<std::string>());
          }
        }
        pc.paths =
            find_filepaths(input_paths, {".las", ".LAS", ".laz", ".LAZ"});
      };
    }

    // parameters
    get_param(config["crop"], "ceil_point_density", _cfg.ceil_point_density);
    get_param(config["crop"], "tilesize_x", _cfg.tilesize[0]);
    get_param(config["crop"], "tilesize_y", _cfg.tilesize[1]);
    get_param(config["crop"], "cellsize", _cfg.cellsize);
    get_param(config["crop"], "lod11_fallback_area", _cfg.lod11_fallback_area);

    if (toml::array* region_of_interest_ =
            config["crop"]["region_of_interest"].as_array()) {
      if (region_of_interest_->size() == 4 &&
          (region_of_interest_->is_homogeneous(
               toml::node_type::floating_point) ||
           region_of_interest_->is_homogeneous(toml::node_type::integer))) {
        _cfg.region_of_interest =
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
    get_param(config["reconstruct"], "complexity_factor",
              _cfg.rec.complexity_factor);
    get_param(config["reconstruct"], "clip_ground", _cfg.rec.clip_ground);
    get_param(config["reconstruct"], "lod", _cfg.rec.lod);
    get_param(config["reconstruct"], "lod13_step_height",
              _cfg.rec.lod13_step_height);
    get_param(config["reconstruct"], "plane_detect_k", _cfg.rec.plane_detect_k);
    get_param(config["reconstruct"], "plane_detect_min_points",
              _cfg.rec.plane_detect_min_points);
    get_param(config["reconstruct"], "plane_detect_epsilon",
              _cfg.rec.plane_detect_epsilon);
    get_param(config["reconstruct"], "plane_detect_normal_angle",
              _cfg.rec.plane_detect_normal_angle);
    get_param(config["reconstruct"], "line_detect_epsilon",
              _cfg.rec.line_detect_epsilon);
    get_param(config["reconstruct"], "thres_alpha", _cfg.rec.thres_alpha);
    get_param(config["reconstruct"], "thres_reg_line_dist",
              _cfg.rec.thres_reg_line_dist);
    get_param(config["reconstruct"], "thres_reg_line_ext",
              _cfg.rec.thres_reg_line_ext);

    // output
    get_param(config["output"], "split_cjseq", _cfg.split_cjseq);
    // get_param(config["output"], "folder", _cfg.output_path);
    get_param(config["output"], "srs_override", _cfg.srs_override);
  }
};
