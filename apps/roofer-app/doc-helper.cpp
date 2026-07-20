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
#include <iostream>
#include "config.hpp"

template <typename T>
std::string reconstruction_toml_value(const T& value) {
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_same_v<T, roofer::enums::TerrainStrategy>) {
    return '"' + std::format("{}", value) + '"';
  } else if constexpr (std::is_same_v<
                           T, roofer::reconstruction::PointCountRange>) {
    return std::format("[{}, {}]", value.first, value.second);
  } else {
    return std::format("{}", value);
  }
}

template <typename Config>
void print_reconstruction_fields(const Config& config) {
  roofer::config::for_each_field(config, [](auto field, const auto& value) {
    std::cout << "## " << field.description << "\n";
    std::cout << field.toml_name() << " = " << reconstruction_toml_value(value)
              << "\n";
  });
}

void print_reconstruction_toml(
    const roofer::reconstruction::ReconstructionConfig& config) {
  std::cout << "### Reconstruction options\n[reconstruction]\n";
  print_reconstruction_fields(config);
  config.visit_components([&](std::string_view name, const auto& component) {
    using Component = std::remove_cvref_t<decltype(component)>;
    if (!roofer::config::has_public_fields<Component>()) return;
    std::cout << "\n[reconstruction." << name << "]\n";
    print_reconstruction_fields(component);
  });
  std::cout << "\n";
}

void print_params(RooferConfigHandler::param_group_map& params) {
  for (const auto& [group_name, param_list] : params) {
    std::cout << "### " << group_name << " options\n\n";
    for (const auto& param : param_list) {
      std::cout << std::format("```{{option}} {} {} (default: {})\n",
                               param->cli_flag(), param->type_description(),
                               param->default_to_string());
      std::cout << param->description() << "\n```\n";
    }
  }
}

void print_pointcloud_toml() {
  InputPointcloud pc_defaults{};
  std::cout << "\n[[input.pointclouds]]\n";
  std::cout << "## Name of the pointcloud\n";
  std::cout << "name = \"\"\n";
  std::cout << "## Path to the pointcloud\n";
  std::cout << "source = [\"\"]\n";
  std::cout << "## LAS classification code that contains the ground points.\n";
  std::cout << "ground_class = " << pc_defaults.grnd_class << "\n";
  std::cout
      << "## LAS classification code that contains the building points.\n";
  std::cout << "building_class = " << pc_defaults.bld_class << "\n";
  std::cout << "## Quality\n";
  std::cout << "quality = " << pc_defaults.quality << "\n";
  std::cout << "## Date\n";
  std::cout << "date = " << pc_defaults.date << "\n";
  std::cout << "## Force LoD11\n";
  std::cout << "force_lod11 = " << (pc_defaults.force_lod11 ? "true" : "false")
            << "\n";
  std::cout << "## Select only for date\n";
  std::cout << "select_only_for_date = "
            << (pc_defaults.select_only_for_date ? "true" : "false") << "\n";
}

void print_params_as_toml(
    RooferConfigHandler::param_group_map& params,
    const roofer::reconstruction::ReconstructionConfig& reconstruction) {
  for (const auto& [group_name, param_list] : params) {
    if (group_name == "Reconstruction") {
      print_reconstruction_toml(reconstruction);
      continue;
    }
    std::cout << std::format("### {} options\n", group_name);
    if (group_name == "Input") {
      std::cout << "[input]\n";
      std::cout << "## Path to roofprint polygons source. Can be an OGR "
                   "supported file (eg. GPKG) or database connection string.\n";
      std::cout << "# polygon-source = \"\"\n";
    } else if (group_name == "Crop") {
      std::cout << "[crop]\n";
    } else if (group_name == "Output") {
      std::cout << "[output]\n";
      std::cout << "## Output directory. The building models will be written "
                   "to a CityJSONSequence file in this directory.\n";
      std::cout << "# output-directory = \"\"\n";
    }
    for (const auto& param : param_list) {
      if (param->longname_ == "attribute-rename") std::cout << "\n";
      std::cout << std::format("## {}\n", param->description());
      // check if param is a string
      auto str = std::format("{}", param->to_string());
      if (param->example_.size() != 0) {
        std::cout << std::format("# {} = {}\n", param->longname_,
                                 param->example_);
      } else if (str.size() == 0) {
        std::cout << std::format("# {} = {}\n", param->longname_,
                                 param->to_string());
      } else if (param->type_description() == "<string>") {
        std::cout << std::format("{} = \"{}\"\n", param->longname_,
                                 param->to_string());
      } else if (param->longname_ == "attribute-rename") {
        std::cout << "[output.attributes]\n";
        std::cout << param->to_toml();
      } else {
        std::cout << std::format("{} = {}\n", param->longname_,
                                 param->to_string());
      }
    }
    if (group_name == "Input") print_pointcloud_toml();
    std::cout << "\n";
  }
}

void print_attributes(DocAttribMap& attributes) {
  for (const auto& [key, attr] : attributes) {
    std::cout << "```{option} " << key << " (default name: " << *attr.value
              << ")\n";
    std::cout << attr.description << "\n```\n";
  }
}

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <format>\n";
    std::cerr << "Available formats: config, attr, params\n";
    return EXIT_FAILURE;
  }
  std::string format = argv[1];
  RooferConfigHandler rch{};

  if (format == "config") {
    print_params_as_toml(rch.param_groups_, rch.cfg_.reconstruction);
  } else if (format == "attr") {
    print_attributes(rch.output_attr_);
  } else if (format == "params") {
    print_params(rch.app_param_groups_);
    print_params(rch.param_groups_);
  } else {
    std::cerr << "Unknown format: " << format << "\n";
    return EXIT_FAILURE;
  }

  return 0;
}
