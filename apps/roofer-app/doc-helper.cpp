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

void print_params_as_toml(RooferConfigHandler::param_group_map& params) {
  for (const auto& [group_name, param_list] : params) {
    std::cout << std::format("[{}]\n", group_name);
    for (const auto& param : param_list) {
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
        std::cout << std::format("[{}.{}]\n", group_name, param->longname_);
        std::cout << param->to_toml();
      } else {
        std::cout << std::format("{} = {}\n", param->longname_,
                                 param->to_string());
      }
    }
    std::cout << "\n";
  }
  // print pointcloud block
  std::cout << "[[pointclouds]]\n";
  std::cout << "## Name of the pointcloud\n";
  std::cout << "name = \"\"\n";
  std::cout << "## Path to the pointcloud\n";
  std::cout << "source = \"\"\n";
  std::cout << "## Ground class\n";
  std::cout << "ground_class = 0\n";
  std::cout << "## building class\n";
  std::cout << "building_class = 0\n";
  std::cout << "## Quality\n";
  std::cout << "quality = 0\n";
  std::cout << "## Date\n";
  std::cout << "date = 0\n";
  std::cout << "## Force LoD11\n";
  std::cout << "force_lod11 = false\n";
  std::cout << "## Select only for date\n";
  std::cout << "select_only_for_date = false\n";
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
    print_params_as_toml(rch.param_groups_);
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
