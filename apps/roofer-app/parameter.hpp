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

  std::string description() { return help_; }
  virtual std::string type_description() = 0;
  virtual std::string to_string() = 0;
};

template <typename T>
class ConfigParameterByReference : public ConfigParameter {
  T& _value;
  std::vector<Validator<T>> _validators;

 public:
  ConfigParameterByReference(std::string longname, std::string help, T& value,
                             std::vector<Validator<T>> validators)
      : ConfigParameter(longname, help),
        _value(value),
        _validators(validators){};
  ConfigParameterByReference(std::string longname, char shortname,
                             std::string help, T& value,
                             std::vector<Validator<T>> validators)
      : ConfigParameter(longname, shortname, help),
        _value(value),
        _validators(validators){};

  std::optional<std::string> validate() override {
    for (auto& validator : _validators) {
      if (auto error_msg = validator(_value)) {
        return error_msg;
      }
    }
    return std::nullopt;
  }

  std::string to_string() override {
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
