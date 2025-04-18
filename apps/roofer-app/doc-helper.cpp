#include <iostream>
#include "config.hpp"

int main() {
  RooferConfigHandler rch{};

  // open file for writing
  // std::ofstream ofs("config-example.toml");
  for (const auto& [groupname, param_list] : rch.params_) {
    for (const auto& param : param_list) {
      std::cout << "# " << param->description() << "\n";
      std::cout << param->longname_ << " = " << param->to_string() << "\n";
    }
  }
  return 0;
}
