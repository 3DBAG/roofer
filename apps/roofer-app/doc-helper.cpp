#include <iostream>
#include "config.hpp"

void print_params(RooferConfigHandler::param_group_map& params) {
  for (const auto& [group_name, param_list] : params) {
    std::cout << "#### " << group_name << "\n\n";
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
    std::cout << std::format("## {}\n", group_name);
    for (const auto& param : param_list) {
      std::cout << std::format("# {}\n", param->description());
      std::cout << std::format("{} = {}\n", param->longname_,
                               param->to_string());
    }
    std::cout << "\n";
  }
}

int main(int argc, const char* argv[]) {
  std::string format = argv[1];
  RooferConfigHandler rch{};

  if (format == "toml") {
    print_params_as_toml(rch.param_groups_);
  } else if (format == "md") {
    print_params(rch.app_param_groups_);
    print_params(rch.param_groups_);
  } else {
    std::cerr << "Unknown format: " << format << "\n";
    return EXIT_FAILURE;
  }

  return 0;
}
