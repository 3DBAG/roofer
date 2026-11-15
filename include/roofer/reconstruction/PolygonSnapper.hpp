#pragma once

#include <roofer/common/datastructures.hpp>
#include <optional>

namespace roofer::reconstruction {
  // Source-coordinate rings, without a repeated closing vertex. Snapping acts
  // on XY; Z is retained or interpolated along the original boundary.
  struct PolygonRings {
    std::vector<arr3d> exterior;
    std::vector<std::vector<arr3d>> holes;
  };

  struct PolygonSnapResult {
    std::vector<PolygonRings> polygons;
    std::optional<std::string> error;
  };

  // Threshold is in input coordinate units. Zero is a no-op. The caller owns
  // feature IDs/attributes and copies them onto each returned polygon part.
  // No float conversion or validation of float representations is performed.
  PolygonSnapResult snap_polygon(const PolygonRings& polygon, double threshold);
}  // namespace roofer::reconstruction
