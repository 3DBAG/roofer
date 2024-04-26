#include "reconstruct.h"

std::vector<int> reconstruct(pointcloud& pc, footprint& fp) {
  std::vector<int> model;
  std::transform(fp.cbegin(), fp.cend(), pc.cbegin(), model.begin(),
                 std::multiplies<>{});
  return model;
}
