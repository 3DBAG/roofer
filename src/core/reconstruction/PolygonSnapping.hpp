#pragma once

#include <roofer/reconstruction/ArrangementBase.hpp>
#include <roofer/reconstruction/PolygonSnapper.hpp>
#include <functional>

namespace roofer::reconstruction::detail {
  // Use the labelled, snapped CDT directly; do not rebuild an output
  // arrangement.
  std::vector<PolygonRings> snapped_polygon_boundaries(
      Arrangement_2& source, double threshold,
      const std::function<arr3d(const Point_2&)>& lift);
}  // namespace roofer::reconstruction::detail
