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

int main() {
  RooferConfigHandler rch{};

  print_params(rch.app_params_);
  print_params(rch.params_);
  return 0;
}
