#pragma once

#include <array>
#include <vector>

using arr3f = std::array<float, 3>;
using vec3f = std::vector<float>;

struct Points {
  vec3f x;
  vec3f y;
  vec3f z;
};
