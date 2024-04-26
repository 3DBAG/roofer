#pragma once

#include <algorithm>
#include <vector>

using pointcloud = std::vector<int>;
using footprint = std::vector<int>;

std::vector<int> reconstruct(pointcloud& pc, footprint& fp);
