#pragma once

#include <memory>
#include <roofer/common/datastructures.hpp>
#include <roofer/misc/Vector2DOps.hpp>

namespace roofer::io {
  /** 2D line segments from an OGR source, selected and clipped per footprint.
   */
  class LineIndex {
   public:
    // Reads the LineString and MultiLineString features of `layer` (default:
    // first layer) in source coordinates; other geometry types are skipped.
    // Throws if the layer CRS differs horizontally from a nonempty
    // `expected_crs`.
    LineIndex(const std::string& source, const std::string& layer,
              const std::string& expected_crs = {});
    size_t size() const { return segments_.size(); }
    // The parts of the segments inside the footprint (holes excluded), in
    // local coordinates (source minus `offset`), at z 0.
    SegmentCollection clip(const LinearRing& footprint,
                           const arr3d& offset) const;

   private:
    std::vector<std::array<arr2d, 2>> segments_;
    std::unique_ptr<misc::RTreeInterface> rtree_;
  };
}  // namespace roofer::io
