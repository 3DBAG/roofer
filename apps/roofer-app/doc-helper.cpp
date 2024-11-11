#include <iostream>
#include "config.hpp"

int main() {
  RooferConfig roofer_cfg;
  std::vector<InputPointcloud> input_pointclouds;
  RooferConfigHandler rch{roofer_cfg, input_pointclouds};

  // open file for writing
  // std::ofstream ofs("config-example.toml");
  for (auto& [name, param] : rch._pmap) {
    std::cout << "# " << param->description() << "\n";
    std::cout << name << " = " << param->get_as_string() << "\n";
  }
  for (auto& [name, param] : rch._rmap) {
    std::cout << "# " << param->description() << "\n";
    std::cout << name << " = " << param->get_as_string() << "\n";
  }

  return 0;
}
