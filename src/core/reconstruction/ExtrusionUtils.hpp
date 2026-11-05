#pragma once

#include <roofer/common/datastructures.hpp>
#include <roofer/reconstruction/cgal_shared_definitions.hpp>

namespace roofer::reconstruction::detail {
  // Convert exact arrangement XY to mesh coordinates only when emitting them.
  inline arr3f lifted(const Point_2& point, float elevation) {
    return {float(CGAL::to_double(point.x())),
            float(CGAL::to_double(point.y())), elevation};
  }

  inline void append(vec3f& ring, arr3f point) {
    if (ring.empty() || ring.back() != point) ring.push_back(point);
  }
}  // namespace roofer::reconstruction::detail
