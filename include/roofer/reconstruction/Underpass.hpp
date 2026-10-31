#pragma once

#include <roofer/common/datastructures.hpp>

namespace roofer {
  /** A horizontal underpass ceiling. Polygon Z coordinates are ignored.
   * Holes retain solid pillars. XY and elevation use reconstruction units and
   * the same local origin as the footprint and point cloud.
   */
  struct Underpass {
    LinearRing polygon;
    double ceiling_elevation = 0;
    std::string source_id;  // Optional identifier for diagnostics.
  };
}  // namespace roofer
