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
#include <roofer/common/formatters.hpp>
#include <string>
#include <format>

struct DocAttrib {
  std::string* value;
  std::string description;
  DocAttrib(std::string* value, std::string description)
      : value(value), description(description){};

  // copy constructor
  DocAttrib(const DocAttrib& other) : value(other.value) {
    description = other.description;
  }

  void operator=(const std::string& str) { *value = str; };
};
using DocAttribMap = std::map<std::string, DocAttrib>;

// std::formatter for DocAttribMap
template <>
struct std::formatter<DocAttribMap> {
  bool as_toml = false;
  constexpr auto parse(std::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && *it == 't') {
      as_toml = true;
      ++it;
    }
    return it;
  }

  auto format(const DocAttribMap& map, std::format_context& ctx) const {
    std::string result;
    for (const auto& [key, value] : map) {
      if (as_toml) {
        result += std::format("## {}\n", value.description);
        result += fmt::format("{} = \"{}\"\n", key, *value.value);
      } else {
        result += fmt::format("{}={},", key, *value.value);
      }
    }
    if (!result.empty() && !as_toml) {
      result.pop_back();  // Remove the last comma
    }
    return std::format_to(ctx.out(), "{}", result);
  }
};

// Formatter for roofer::enum::TerrainStrategy
template <>
struct std::formatter<roofer::enums::TerrainStrategy> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::enums::TerrainStrategy& strategy,
              std::format_context& ctx) const {
    switch (strategy) {
      case roofer::enums::BUFFER_TILE:
        return std::format_to(ctx.out(), "buffer_tile");
      case roofer::enums::BUFFER_USER:
        return std::format_to(ctx.out(), "buffer_user");
      case roofer::enums::USER:
        return std::format_to(ctx.out(), "user");
      default:
        return std::format_to(ctx.out(), "unknown");
    }
  }
};

// Formatter for roofer::logger::LogLevel
template <>
struct std::formatter<roofer::logger::LogLevel> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const roofer::logger::LogLevel& level,
              std::format_context& ctx) const {
    switch (level) {
      case roofer::logger::LogLevel::trace:
        return std::format_to(ctx.out(), "trace");
      case roofer::logger::LogLevel::debug:
        return std::format_to(ctx.out(), "debug");
      case roofer::logger::LogLevel::info:
        return std::format_to(ctx.out(), "info");
      default:
        return std::format_to(ctx.out(), "unknown");
    }
  }
};

struct ConfigParameter {
  std::string help_;
  std::string longname_;
  std::string example_;
  std::optional<char> shortname_;
  ConfigParameter(std::string longname, char shortname, std::string help)
      : longname_(longname), shortname_(shortname), help_(help){};
  ConfigParameter(std::string longname, std::string help)
      : longname_(longname), help_(help){};
  virtual ~ConfigParameter() = default;

  virtual std::optional<std::string> validate() = 0;

  virtual std::list<std::string>::iterator set(
      std::list<std::string>& args, std::list<std::string>::iterator it) = 0;
  virtual void unset() = 0;

  virtual void set_from_toml(const toml::table& table,
                             const std::string& name) = 0;

  virtual std::string description() = 0;
  virtual std::string type_description() = 0;
  virtual std::string to_string() = 0;
  virtual std::string default_to_string() = 0;
  virtual std::string cli_flag() = 0;
  virtual std::string to_toml() = 0;

  std::string example_to_string() {
    if (example_.size() == 0) {
      return "<no example>";
    } else {
      return std::format("{}", example_);
    }
  }
};

template <typename T>
struct ConfigParameterByReference : public ConfigParameter {
  T& value_;
  T default_value_;
  std::vector<Validator<T>> _validators;

  ConfigParameterByReference(std::string longname, std::string help, T& value,
                             std::vector<Validator<T>> validators)
      : ConfigParameter(longname, help),
        value_(value),
        default_value_(value),
        _validators(validators){};
  ConfigParameterByReference(std::string longname, char shortname,
                             std::string help, T& value,
                             std::vector<Validator<T>> validators)
      : ConfigParameter(longname, shortname, help),
        value_(value),
        _validators(validators){};

  std::optional<std::string> validate() override {
    for (auto& validator : _validators) {
      if (auto error_msg = validator(value_)) {
        return error_msg;
      }
    }
    return std::nullopt;
  }

  std::string to_string() override { return std::format("{}", value_); }

  std::string to_toml() override {
    if constexpr (std::is_same_v<T, DocAttribMap>) {
      return std::format("{:t}", value_);
    } else {
      return std::format("{}", value_);
    }
  }

  std::string default_to_string() override {
    std::string s = std::format("{}", default_value_);
    if (s.size() == 0) {
      return "<no value>";
    } else {
      return std::format("{}", default_value_);
    }
  }

  std::string cli_flag() override {
    if (shortname_.has_value()) {
      return std::format("-{}, --{}", shortname_.value(), longname_);
    } else {
      if constexpr (std::is_same_v<T, bool>) {
        return std::format("--[no-]{}", longname_);
      } else {
        return std::format("--{}", longname_);
      }
    }
  }

  void unset() override {
    if constexpr (std::is_same_v<T, bool>) {
      value_ = false;
    } else {
      value_ = default_value_;
    }
  }

  std::list<std::string>::iterator set(
      std::list<std::string>& args,
      std::list<std::string>::iterator it) override {
    if constexpr (std::is_same_v<T, bool>) {
      value_ = true;
      return it;
    } else {
      if (it == args.end()) {
        throw std::runtime_error("Missing argument for parameter");
      } else if constexpr (std::is_same_v<T, int> ||
                           std::is_same_v<T, std::optional<int>>) {
        value_ = std::stoi(*it);
        return args.erase(it);
      } else if constexpr (std::is_same_v<T, float> ||
                           std::is_same_v<T, std::optional<float>>) {
        value_ = std::stof(*it);
        return args.erase(it);
      } else if constexpr (std::is_same_v<T, double> ||
                           std::is_same_v<T, std::optional<double>>) {
        value_ = std::stod(*it);
        return args.erase(it);
      } else if constexpr (std::is_same_v<T, std::string>) {
        value_ = *it;
        return args.erase(it);
      } else if constexpr (std::is_same_v<
                               T, std::optional<roofer::TBox<double>>> ||
                           std::is_same_v<T, roofer::TBox<double>>) {
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
        value_ = box;
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
        value_ = arr;
        return it;
      } else if constexpr (std::is_same_v<T, std::optional<roofer::arr3d>> ||
                           std::is_same_v<T, roofer::arr3d>) {
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
        value_ = arr;
        return it;
      } else if constexpr (std::is_same_v<T, roofer::enums::TerrainStrategy>) {
        if (*it == "buffer_tile") {
          value_ = roofer::enums::TerrainStrategy::BUFFER_TILE;
        } else if (*it == "buffer_user") {
          value_ = roofer::enums::TerrainStrategy::BUFFER_USER;
        } else if (*it == "user") {
          value_ = roofer::enums::TerrainStrategy::USER;
        } else {
          throw std::runtime_error("Invalid argument for TerrainStrategy");
        }
        return args.erase(it);
      } else if constexpr (std::is_same_v<T, roofer::logger::LogLevel>) {
        if (*it == "trace") {
          value_ = roofer::logger::LogLevel::trace;
        } else if (*it == "debug") {
          value_ = roofer::logger::LogLevel::debug;
        } else if (*it == "info") {
          value_ = roofer::logger::LogLevel::info;
        } else {
          throw std::runtime_error("Invalid argument for LogLevel");
        }
        return args.erase(it);
      } else if constexpr (std::is_same_v<T, DocAttribMap>) {
        // split by comma
        auto keyval_pairs = roofer::split_string(*it, ",");
        // split the string "key=value" into key and value
        for (auto& keyval : keyval_pairs) {
          auto kv = roofer::split_string(keyval, "=");
          if (kv.size() == 1) {
            // if no value is given
            value_.at(kv[0]) = "";
          } else if (kv.size() == 2) {
            // if key and value are given
            value_.at(kv[0]) = kv[1];
          } else {
            throw std::runtime_error("Invalid argument for StringParameterMap");
          }
        }
        return args.erase(it);
      } else {
        static_assert(!std::is_same_v<T, T>,
                      "Unsupported type for ConfigParameterByReference::set()");
      }
    }
  }

  void set_from_toml(const toml::table& table,
                     const std::string& name) override {
    if constexpr (std::is_same_v<T, std::array<float, 2>>) {
      if (const toml::array* a = table[name].as_array()) {
        if (a->size() == 2 &&
            (a->is_homogeneous(toml::node_type::floating_point) ||
             a->is_homogeneous(toml::node_type::integer))) {
          value_ = roofer::arr2f{*a->get(0)->value<float>(),
                                 *a->get(1)->value<float>()};
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T,
                                        std::optional<std::array<double, 3>>> ||
                         std::is_same_v<T, std::array<double, 3>>) {
      if (const toml::array* a = table[name].as_array()) {
        if (a->size() == 3 &&
            (a->is_homogeneous(toml::node_type::floating_point) ||
             a->is_homogeneous(toml::node_type::integer))) {
          value_ = roofer::arr3d{*a->get(0)->value<double>(),
                                 *a->get(1)->value<double>(),
                                 *a->get(2)->value<double>()};
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T,
                                        std::optional<roofer::TBox<double>>> ||
                         std::is_same_v<T, roofer::TBox<double>>) {
      if (const toml::array* a = table[name].as_array()) {
        if (a->size() == 4 &&
            (a->is_homogeneous(toml::node_type::floating_point) ||
             a->is_homogeneous(toml::node_type::integer))) {
          value_ = roofer::TBox<double>{
              *a->get(0)->value<double>(), *a->get(1)->value<double>(), 0,
              *a->get(2)->value<double>(), *a->get(3)->value<double>(), 0};
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T, roofer::enums::TerrainStrategy>) {
      if (const toml::value<std::string>* s = table[name].as_string()) {
        if (*s == "buffer_tile") {
          value_ = roofer::enums::TerrainStrategy::BUFFER_TILE;
        } else if (*s == "buffer_user") {
          value_ = roofer::enums::TerrainStrategy::BUFFER_USER;
        } else if (*s == "user") {
          value_ = roofer::enums::TerrainStrategy::USER;
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T, roofer::logger::LogLevel>) {
      if (const toml::value<std::string>* s = table[name].as_string()) {
        if (*s == "trace") {
          value_ = roofer::logger::LogLevel::trace;
        } else if (*s == "debug") {
          value_ = roofer::logger::LogLevel::debug;
        } else if (*s == "info") {
          value_ = roofer::logger::LogLevel::info;
        } else {
          throw std::runtime_error("Failed to read value for " + name +
                                   " from config file.");
        }
      }
    } else if constexpr (std::is_same_v<T, DocAttribMap>) {
      if (const toml::array* a = table[name].as_array()) {
        for (const auto& el : *a) {
          if (const toml::table* tb = el.as_table()) {
            for (const auto& [key, value] : *tb) {
              std::string key_str(key.str());
              std::string value_str = *value.value<std::string>();
              value_.at(key_str) = value_str;
            }
          }
        }
      }
    } else {
      if (auto value = table[name].value<T>(); value.has_value()) {
        value_ = *value;
      }
    }
  }

  std::string description() override { return std::format("{}", help_); }

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
      return "<string>";
    } else if constexpr (std::is_same_v<T,
                                        std::optional<roofer::TBox<double>>> ||
                         std::is_same_v<T, roofer::TBox<double>>) {
      return "(xmin ymin xmax ymax)";
    } else if constexpr (std::is_same_v<T, roofer::arr2f>) {
      return "(x y)";
    } else if constexpr (std::is_same_v<T, std::optional<roofer::arr3d>> ||
                         std::is_same_v<T, roofer::arr3d>) {
      return "(x y z)";
    } else if constexpr (std::is_same_v<T, roofer::enums::TerrainStrategy>) {
      return "(buffer_tile|buffer_user|user)";
    } else if constexpr (std::is_same_v<T, roofer::logger::LogLevel>) {
      return "(trace|debug|info)";
    } else if constexpr (std::is_same_v<T, DocAttribMap>) {
      return "key=value[,...]";
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "Unsupported type for "
                    "ConfigParameterByReference::type_description()");
    }
  }
};

class ParameterVector {
 public:
  // std::string name_;
  std::vector<std::unique_ptr<ConfigParameter>> params_;

  ParameterVector(){};
  ~ParameterVector() = default;

  // Ensure move constructor and move assignment are enabled
  ParameterVector(ParameterVector&&) = default;             // Move constructor
  ParameterVector& operator=(ParameterVector&&) = default;  // Move assignment

  // If you have a custom copy constructor or destructor, ensure they are
  // compatible For example, if copying is not needed, explicitly delete it
  ParameterVector(const ParameterVector&) = delete;
  ParameterVector& operator=(const ParameterVector&) = delete;

  template <typename T>
  ConfigParameter& add(const std::string& longname, const std::string& help,
                       T& value, std::vector<Validator<T>> validators = {}) {
    params_.emplace_back(std::make_unique<ConfigParameterByReference<T>>(
        longname, help, value, std::move(validators)));
    return *params_.back();
  }

  template <typename T>
  ConfigParameter& add(const std::string& longname, const char shortname,
                       const std::string& help, T& value,
                       std::vector<Validator<T>> validators = {}) {
    params_.emplace_back(std::make_unique<ConfigParameterByReference<T>>(
        longname, shortname, help, value, std::move(validators)));
    return *params_.back();
  }

  void add_to_index(std::unordered_map<std::string, ConfigParameter*>& index) {
    for (auto& param : params_) {
      index[param->longname_] = param.get();
      if (param->shortname_.has_value()) {
        index[std::string(1, param->shortname_.value())] = param.get();
      }
    }
  }

  // iteraror functionality to params_
  auto begin() { return params_.begin(); }
  auto end() { return params_.end(); }
  auto begin() const { return params_.begin(); }
  auto end() const { return params_.end(); }
  auto size() const { return params_.size(); }
  auto empty() const { return params_.empty(); }
  auto& operator[](size_t i) { return params_[i]; }
  auto& operator[](size_t i) const { return params_[i]; }

  // std::optional<std::string> validate() {
  //   for (auto& param : group_) {
  //     if (auto error_msg = param->validate()) {
  //       return error_msg;
  //     }
  //   }
  //   return std::nullopt;
  // }
};

// template <>
// struct std::formatter<ConfigParameterByReference<typename T>> {
//     bool add_quotes = false; // Flag to control quotes

//     // Parse the format specification (e.g., ":q" for quotes)
//     constexpr auto parse(std::format_parse_context& ctx) {
//         auto it = ctx.begin(), end = ctx.end();
//         if (it != end && *it == 'q') {
//             add_quotes = true; // Enable quotes if 'q' is specified
//             ++it;
//         }
//         if (it != end && *it != '}') {
//             throw std::format_error("Invalid format specifier");
//         }
//         return it;
//     }

//     // Format the MyString object
//     template <typename FormatContext>
//     auto format(const MyString& str, FormatContext& ctx) const {
//         if (add_quotes) {
//             // Output with quotes
//             return std::format_to(ctx.out(), "\"{}\"", str.value);
//         } else {
//             // Output without quotes
//             return std::format_to(ctx.out(), "{}", str.value);
//         }
//     }
// };
