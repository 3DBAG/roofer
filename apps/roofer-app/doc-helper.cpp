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
