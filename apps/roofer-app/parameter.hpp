#include <roofer/common/formatters.hpp>
#include "config.hpp"

class ConfigParameter {
 public:
  std::string help_;
  std::string longname_;
  std::optional<char> shortname_;
  ConfigParameter(std::string longname, char shortname, std::string help)
      : longname_(longname), shortname_(shortname), help_(help){};
  ConfigParameter(std::string longname, std::string help)
      : longname_(longname), help_(help){};
  virtual ~ConfigParameter() = default;

  virtual std::optional<std::string> validate() = 0;

  virtual std::list<std::string>::iterator set(
      std::list<std::string>& args, std::list<std::string>::iterator it) = 0;

  virtual void set_from_toml(const toml::table& table,
                             const std::string& name) = 0;

  virtual std::string description() = 0;
  virtual std::string type_description() = 0;
  virtual std::string to_string() = 0;
  virtual std::string default_to_string() = 0;
};

template <typename T>
class ConfigParameterByReference : public ConfigParameter {
  T& value_;
  T default_value_;
  std::vector<Validator<T>> _validators;

 public:
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

  std::string default_to_string() override {
    std::string s = std::format("{}", default_value_);
    if (s.size() == 0) {
      return "<no value>";
    } else {
      return std::format("{}", default_value_);
    }
  }

  std::list<std::string>::iterator set(
      std::list<std::string>& args,
      std::list<std::string>::iterator it) override {
    if (it == args.end()) {
      throw std::runtime_error("Missing argument for parameter");
    } else if constexpr (std::is_same_v<T, bool>) {
      if (*it == "true" || *it == "yes" || *it == "1") {
        value_ = true;
      } else if (*it == "false" || *it == "no" || *it == "0") {
        value_ = false;
      } else {
        throw std::runtime_error("Invalid argument for boolean parameter");
      }
      return args.erase(it);
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
    } else if constexpr (std::is_same_v<T,
                                        std::optional<roofer::TBox<double>>> ||
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
    } else {
      if (auto value = table[name].value<T>(); value.has_value()) {
        value_ = *value;
      }
    }
  }

  std::string description() override { return std::format("{}", help_); }

  std::string type_description() override {
    if constexpr (std::is_same_v<T, bool>) {
      return "<true|false>";
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
      return "<buffer_tile|buffer_user|user>";
    } else {
      static_assert(!std::is_same_v<T, T>,
                    "Unsupported type for "
                    "ConfigParameterByReference::type_description()");
    }
  }
};
